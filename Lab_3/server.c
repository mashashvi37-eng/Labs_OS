#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <shm_name> <sem_name>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *shm_name = argv[1];
    const char *sem_name = argv[2];

    if (shm_name[0] != '/' || shm_name[1] == '\0' ||
        strchr(shm_name + 1, '/') != NULL ||
        sem_name[0] != '/' || sem_name[1] == '\0' ||
        strchr(sem_name + 1, '/') != NULL) {
        fprintf(stderr, "Names must start with '/' and contain no other '/'\n");
        return EXIT_FAILURE;
    }

    char *input_buf = NULL;
    size_t input_capacity = 0;

    ssize_t input_length = getline(&input_buf, &input_capacity, stdin);

    if (input_length == -1) {
        perror("getline error");
        free(input_buf);
        return EXIT_FAILURE;
    }

    int shm_fd = -1;
    char *shm_ptr = MAP_FAILED;
    sem_t *sem = SEM_FAILED;

    int shm_created = 0;
    int sem_created = 0;

    pid_t pid = -1;
    int exit_status = EXIT_FAILURE;

    shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);

    if (shm_fd == -1) {
        perror("shm_open error");
        goto cleanup;
    }

    shm_created = 1;

    uint64_t data_length = (uint64_t)input_length;
    size_t shm_size = sizeof(data_length) + (size_t)data_length;

    if (ftruncate(shm_fd, (off_t)shm_size) == -1) {
        perror("ftruncate error");
        goto cleanup;
    }

    shm_ptr = mmap(NULL, shm_size,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED, shm_fd, 0);

    if (shm_ptr == MAP_FAILED) {
        perror("mmap error");
        goto cleanup;
    }

    sem = sem_open(sem_name, O_CREAT | O_EXCL, 0600, 0);

    if (sem == SEM_FAILED) {
        perror("sem_open error");
        goto cleanup;
    }

    sem_created = 1;

    memcpy(shm_ptr, &data_length, sizeof(data_length));

    memcpy(shm_ptr + sizeof(data_length),
           input_buf, (size_t)data_length);

    pid = fork();

    if (pid == -1) {
        perror("fork error");
        goto cleanup;
    }

    if (pid == 0) {
        sem_close(sem);
        munmap(shm_ptr, shm_size);
        close(shm_fd);

        char *args[] = {
            "./client",
            (char *)shm_name,
            (char *)sem_name,
            NULL
        };

        execv("./client", args);

        perror("execv error");
        _exit(EXIT_FAILURE);
    }

    if (sem_post(sem) == -1) {
        perror("sem_post error");
        kill(pid, SIGTERM);
        goto cleanup;
    }

    int status;
    pid_t result;

    do {
        result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        perror("waitpid error");
        goto cleanup;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
        exit_status = EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Сервер: клиент завершился с ошибкой.\n");
    }

cleanup:

    if (shm_ptr != MAP_FAILED) {
        munmap(shm_ptr,
               shm_created ? (size_t)sizeof(uint64_t) +
               (size_t)input_length : 0);
    }

    if (shm_fd != -1) {
        close(shm_fd);
    }
    if (sem != SEM_FAILED) {
        sem_close(sem);
    }
    if (sem_created) {
        sem_unlink(sem_name);
    }
    if (shm_created) {
        shm_unlink(shm_name);
    }
    free(input_buf);

    return exit_status;
}
