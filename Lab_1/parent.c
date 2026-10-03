#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>


int main(void) {
    int pipe1[2];
    int pipe2[2];

    if (pipe(pipe1) == -1) {
        const char msg[] = "error: pipe1 failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    if (pipe(pipe2) == -1) {
        const char msg[] = "error: pipe2 failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0) {
        const char msg[] = "error: fork failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {

        close(pipe1[1]);
        close(pipe2[0]);

        
        if (dup2(pipe1[0], STDIN_FILENO) == -1) {
            const char msg[] = "error: dup2 stdin failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }

        if (dup2(pipe2[1], STDOUT_FILENO) == -1) {
            const char msg[] = "error: dup2 stdout failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }

        close(pipe1[0]);
        close(pipe2[1]);

        execlp("./child", "./child", (char *)NULL);

        const char msg[] = "error: exec failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    close(pipe1[0]);
    close(pipe2[1]);

    char fname[256];
    ssize_t n = 0;
    char c;
    int i = 0;

    while (i < 255) {
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r <= 0) break;
        if (c == '\n') break;
        fname[i++] = c;
    }
    fname[i] = '\0';

    if (i == 0) {
        const char msg[] = "error: empty filename\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    n = 0;
    while (fname[n]) ++n;
    write(pipe1[1], fname, n);
    write(pipe1[1], "\n", 1);

    char line[512];
    i = 0;
    while (i < 511) {
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r <= 0) break;
        if (c == '\n') break;
        line[i++] = c;
    }
    line[i] = '\0';

    if (i == 0) {
        const char msg[] = "error: empty numbers line\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    write(pipe1[1], line, i);
    write(pipe1[1], "\n", 1);

    close(pipe1[1]);
    wait(NULL);
    close(pipe2[0]);
    return 0;
}
