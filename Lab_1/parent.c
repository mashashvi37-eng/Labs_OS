#include <stdint.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>

static char CHILD_PROGRAM_NAME[] = "child";

static int stlen(const char *s) {
    int n = 0;
    while (s[n]) ++n;
    return n;
}

static int append(char *dst, int i, const char *src) {
    int j = 0;
    while (src[j]) dst[i++] = src[j++];
    return i;
}

static int append_int(char *dst, int i, int x) {
    char tmp[16];
    int n = 0;
    int neg = 0;
    if (x == 0) { dst[i++] = '0'; return i; }
    if (x < 0) { neg = 1; x = -x; }
    while (x > 0) { tmp[n++] = '0' + (x % 10); x /= 10; }
    if (neg) tmp[n++] = '-';
    for (int a = 0; a < n / 2; ++a) {
        char t = tmp[a];
        tmp[a] = tmp[n - 1 - a];
        tmp[n - 1 - a] = t;
    }
    for (int a = 0; a < n; ++a) dst[i++] = tmp[a];
    return i;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    char progpath[1024];
    {
        ssize_t len = readlink("/proc/self/exe", progpath, sizeof(progpath) - 1);
        if (len == -1) {
            const char msg[] = "error: failed to read full program path\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        while (progpath[len] != '/') --len;
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
            int i = 0;
            i = append_int(msg, i, pid);
            i = append(msg, i, ": I'm a child\n");
            write(STDOUT_FILENO, msg, i);
        }

        close(parent_to_child[1]);
        close(child_to_parent[0]);

        dup2(parent_to_child[0], STDIN_FILENO);
        close(parent_to_child[0]);

        dup2(child_to_parent[1], STDOUT_FILENO);
        close(child_to_parent[1]);

        {
            char path[2048];
            int p = 0;
            p = append(path, p, progpath);
            p = append(path, p, "/");
            p = append(path, p, CHILD_PROGRAM_NAME);
            path[p] = '\0';

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
            char msg[128];
            int i = 0;
            i = append_int(msg, i, pid);
            i = append(msg, i, ": I'm a parent, my child has PID ");
            i = append_int(msg, i, child);
            i = append(msg, i, "\n");
            write(STDOUT_FILENO, msg, i);
        }

        close(parent_to_child[0]);
        close(child_to_parent[1]);

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

        write(parent_to_child[1], fname, stlen(fname));
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

        write(parent_to_child[1], line, stlen(line));
        write(parent_to_child[1], "\n", 1);

        close(parent_to_child[1]);
        close(child_to_parent[0]);

        wait(NULL);
    } break;
    }

    return 0;
}
