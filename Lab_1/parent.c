#include <stdint.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>

static char CHILD_PROGRAM_NAME[] = "child";

static int my_strlen(const char *s) {
    int n = 0;
    while (s[n]) ++n;
    return n;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    char progpath[1024];
    {
        ssize_t len = readlink("/proc/self/exe", progpath, sizeof(progpath) - 1);
        if (len == -1) {
            const char msg[] = "error: failed to read full program path\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        while (progpath[len] != '/')
            --len;
        progpath[len] = '\0';
    }

    int parent_to_child[2];
    if (pipe(parent_to_child) == -1) {
        const char msg[] = "error: failed to create pipe1\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    int child_to_parent[2];
    if (pipe(child_to_parent) == -1) {
        const char msg[] = "error: failed to create pipe2\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    const pid_t child = fork();

    switch (child) {
    case -1: {
        const char msg[] = "error: failed to spawn new process\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    } break;

    case 0: {
        {
            pid_t pid = getpid();
            char msg[64];
            const int32_t length = snprintf(msg, sizeof(msg),
                "%d: I'm a child\n", pid);
            write(STDOUT_FILENO, msg, length);
        }

        close(parent_to_child[1]);
        close(child_to_parent[0]);

        dup2(parent_to_child[0], STDIN_FILENO);
        close(parent_to_child[0]);

        dup2(child_to_parent[1], STDOUT_FILENO);
        close(child_to_parent[1]);

        {
            char path[1024];
            snprintf(path, sizeof(path) - 1, "%s/%s", progpath, CHILD_PROGRAM_NAME);

            char *const args[] = {CHILD_PROGRAM_NAME, NULL};

            int32_t status = execv(path, args);

            if (status == -1) {
                const char msg[] = "error: failed to exec into new executable image\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                exit(EXIT_FAILURE);
            }
        }
    } break;

    default: {
        {
            pid_t pid = getpid();
            char msg[64];
            const int32_t length = snprintf(msg, sizeof(msg),
                "%d: I'm a parent, my child has PID %d\n", pid, child);
            write(STDOUT_FILENO, msg, length);
        }

        close(parent_to_child[0]);
        close(child_to_parent[1]);

        char buf[4096];
        ssize_t bytes;

        char fname[256];
        int i = 0;
        char c;
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

        write(parent_to_child[1], fname, my_strlen(fname));
        write(parent_to_child[1], "\n", 1);

        char line[4096];
        i = 0;
        while (i < 4095) {
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

        write(parent_to_child[1], line, my_strlen(line));
        write(parent_to_child[1], "\n", 1);

        close(parent_to_child[1]);
        close(child_to_parent[0]);
        wait(NULL);

        (void)buf;
        (void)bytes;
    } break;
    }

    return 0;
}
