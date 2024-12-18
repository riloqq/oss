#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define SHARED_MEM_NAME "/shared_mem_example"
#define SHARED_MEM_SIZE 4096

void error_exit(const char *msg) {
    const char *err = strerror(errno);
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, ": ", 2);
    write(STDERR_FILENO, err, strlen(err));
    write(STDERR_FILENO, "\n", 1);
    exit(EXIT_FAILURE);
}

int main() {
    char filename[BUFSIZ];

    if (write(STDOUT_FILENO, "Put name of file: ", 18) == -1) {
        error_exit("Error writing");
    }
    ssize_t bytes_read = read(STDIN_FILENO, filename, sizeof(filename) - 1);
    if (bytes_read <= 0) {
        error_exit("Error reading filename");
    }

    if (filename[bytes_read - 1] == '\n') {
        filename[bytes_read - 1] = '\0';
    }

    int file = open(filename, O_RDONLY);
    if (file < 0) {
        error_exit("Error opening file");
    }

    int shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);  // Создание/открытие общей памяти
    if (shm_fd == -1) {
        error_exit("Error creating shared memory");
    }

    if (ftruncate(shm_fd, SHARED_MEM_SIZE) == -1) { // Установка размера общей памяти
        error_exit("Error setting size for shared memory");
    }

    char *shared_mem = mmap(NULL, SHARED_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);  // Отображение общей памяти
    if (shared_mem == MAP_FAILED) {
        error_exit("Error mapping shared memory");
    }

    pid_t pid = fork(); // Создание нового процесса
    if (pid == -1) {
        error_exit("Error creating process");
    }

    if (pid == 0) { // Дочерний процесс
        char *args[] = {"./child", NULL};
        execvp(args[0], args); // Запуск дочернего процесса
        error_exit("Error executing child process");
    } else { // Родительский процесс
        size_t offset = 0;
        char buffer[256];
        ssize_t nread;

        while ((nread = read(file, buffer, sizeof(buffer))) > 0) {
            if (offset + nread >= SHARED_MEM_SIZE) { // Проверка на переполнение общей памяти
                write(STDERR_FILENO, "Shared memory overflow\n", 24);
                close(file);
                shm_unlink(SHARED_MEM_NAME); // Удаление общей памяти
                munmap(shared_mem, SHARED_MEM_SIZE); // Отключение общей памяти
                exit(EXIT_FAILURE);
            }
            memcpy(shared_mem + offset, buffer, nread); // Копирование данных в общую память
            offset += nread;
        }

        if (nread == -1) {
            error_exit("Error reading file");
        }

        shared_mem[offset] = '\0';
        close(file);

        int status;
        wait(&status);

        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_FAILURE) {
            const char error_msg[] = "Child process terminated with division by zero.\n";
            write(STDERR_FILENO, error_msg, sizeof(error_msg) - 1);
        } else {
            write(STDOUT_FILENO, "Results:\n", 9);
            write(STDOUT_FILENO, shared_mem, strlen(shared_mem));
        }

        shm_unlink(SHARED_MEM_NAME); // Удаление общей памяти
        munmap(shared_mem, SHARED_MEM_SIZE); // Отключение общей памяти
    }

    return 0;
}
