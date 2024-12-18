#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
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
    int shm_fd = shm_open(SHARED_MEM_NAME, O_RDWR, 0666);  // Открытие общей памяти для чтения и записи
    if (shm_fd == -1) {
        error_exit("Error opening shared memory");
    }

    char *shared_mem = mmap(NULL, SHARED_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); // Отображаем общую память в адресное пространство
    if (shared_mem == MAP_FAILED) {
        error_exit("Error mapping shared memory");
    }

    char result_buffer[SHARED_MEM_SIZE];
    size_t result_offset = 0;

    char line[BUFSIZ];
    size_t line_pos = 0;
    for (size_t i = 0; shared_mem[i] != '\0'; i++) {
        char c = shared_mem[i];
        line[line_pos++] = c;

        if (c == '\n' || shared_mem[i + 1] == '\0') { // Конец строки или конец памяти
            line[line_pos] = '\0';
            line_pos = 0;

            char *token = strtok(line, " ");
            float result = 0.0;

            if (token != NULL) {
                result = strtof(token, NULL);

                while ((token = strtok(NULL, " ")) != NULL) {
                    float divisor = strtof(token, NULL);
                    if (divisor == 0.0) {
                        munmap(shared_mem, SHARED_MEM_SIZE);
                        exit(EXIT_FAILURE); // Деление на 0
                    }
                    result /= divisor;
                }
            }

            int len = snprintf(result_buffer + result_offset, SHARED_MEM_SIZE - result_offset, "%.2f\n", result);
            if (len < 0 || (size_t)len >= SHARED_MEM_SIZE - result_offset) {
                error_exit("Error writing to shared memory");
            }
            result_offset += len;
        }
    }

    memcpy(shared_mem, result_buffer, result_offset); // Копируем результаты в общую память
    shared_mem[result_offset] = '\0';

    munmap(shared_mem, SHARED_MEM_SIZE); // Отключаем отображение общей памяти
    close(shm_fd); // Закрываем файловый дескриптор общей памяти

    return 0;
}
