#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <shm_name> <sem_name>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *shm_name = argv[1];
    const char *sem_name = argv[2];

    int shm_fd = -1;
    char *shm_ptr = MAP_FAILED;
    sem_t *sem = SEM_FAILED;
    char *input = NULL;

    int exit_status = EXIT_FAILURE;
    size_t shm_size = 0;

    shm_fd = shm_open(shm_name, O_RDWR, 0);

    if (shm_fd == -1) {
        perror("shm_open error");
        goto cleanup;
    }

    struct stat shm_info;

    if (fstat(shm_fd, &shm_info) == -1) {
        perror("fstat error");
        goto cleanup;
    }

    if (shm_info.st_size < (off_t)sizeof(uint64_t)) {
        fprintf(stderr, "Invalid shared memory size\n");
        goto cleanup;
    }

    shm_size = (size_t)shm_info.st_size;

    shm_ptr = mmap(NULL, shm_size,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED, shm_fd, 0);

    if (shm_ptr == MAP_FAILED) {
        perror("mmap error");
        goto cleanup;
    }

    sem = sem_open(sem_name, 0);

    if (sem == SEM_FAILED) {
        perror("sem_open error");
        goto cleanup;
    }

    while (sem_wait(sem) == -1) {
        if (errno != EINTR) {
            perror("sem_wait error");
            goto cleanup;
        }
    }

    uint64_t data_length;

    memcpy(&data_length, shm_ptr, sizeof(data_length));

    if (data_length > shm_size - sizeof(data_length)) {
        fprintf(stderr, "Invalid data length\n");
        goto cleanup;
    }

    input = malloc((size_t)data_length + 1);

    if (input == NULL) {
        perror("malloc error");
        goto cleanup;
    }

    memcpy(input,
           shm_ptr + sizeof(data_length),
           (size_t)data_length);

    input[data_length] = '\0';

    int64_t sum = 0;
    char *ptr = input;

    while (*ptr != '\0') {

        while (isspace((unsigned char)*ptr)) {
            ptr++;
        }

        if (*ptr == '\0') {
            break;
        }

        errno = 0;
        char *end;

        long number = strtol(ptr, &end, 10);

        if (ptr == end || errno == ERANGE ||
            number < INT_MIN || number > INT_MAX) {
            fprintf(stderr, "Invalid integer: %s\n", ptr);
            goto cleanup;
        }

        if (*end != '\0' &&
            !isspace((unsigned char)*end)) {
            fprintf(stderr, "Invalid input near: %s\n", end);
            goto cleanup;
        }

        if ((number > 0 && sum > INT64_MAX - number) ||
            (number < 0 && sum < INT64_MIN - number)) {
            fprintf(stderr, "Sum overflow\n");
            goto cleanup;
        }

        sum += number;
        ptr = end;
    }

    FILE *file = fopen("result.txt", "w");

    if (file == NULL) {
        perror("fopen error");
        goto cleanup;
    }

    if (fprintf(file, "%" PRId64 "\n", sum) < 0) {
        perror("fprintf error");
        fclose(file);
        goto cleanup;
    }

    if (fclose(file) == EOF) {
        perror("fclose error");
        goto cleanup;
    }

    printf("Клиент: сумма = %" PRId64 "\n", sum);
    printf("Клиент: результат записан в result.txt\n");

    exit_status = EXIT_SUCCESS;

cleanup:
    free(input);

    if (sem != SEM_FAILED) {
        sem_close(sem);
    }

    if (shm_ptr != MAP_FAILED) {
        munmap(shm_ptr, shm_size);
    }

    if (shm_fd != -1) {
        close(shm_fd);
    }

    return exit_status;
}
