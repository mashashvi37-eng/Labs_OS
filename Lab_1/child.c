#include <stdint.h>
#include <stdbool.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static int my_strlen(const char *s) {
    int n = 0;
    while (s[n]) ++n;
    return n;
}

static void write_int(int fd, long x) {
    char tmp[32];
    int i = 0;
    int neg = 0;

    if (x == 0) {
        write(fd, "0", 1);
        return;
    }
    if (x < 0) { neg = 1; x = -x; }

    while (x > 0) {
        tmp[i++] = '0' + (x % 10);
        x /= 10;
    }
    if (neg) tmp[i++] = '-';

    for (int j = 0; j < i / 2; ++j) {
        char t = tmp[j];
        tmp[j] = tmp[i - 1 - j];
        tmp[i - 1 - j] = t;
    }
    write(fd, tmp, i);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    pid_t pid = getpid();
    (void)pid;

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

    int32_t file = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file == -1) {
        const char msg[] = "error: failed to open requested file\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    long sum = 0;
    long val = 0;
    int sign = 1;
    int num = 0;

    char buf[4096];
    ssize_t bytes;
    while ((bytes = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (ssize_t k = 0; k < bytes; ++k) {
            char ch = buf[k];

            if (ch == '-') {
                sign = -1;
                num = 1;
            } else if (ch >= '0' && ch <= '9') {
                val = val * 10 + (ch - '0');
                num = 1;
            } else {
                if (num) {
                    sum += sign * val;
                    val = 0;
                    sign = 1;
                    num = 0;
                }
            }
        }
    }

    if (bytes < 0) {
        const char msg[] = "error: failed to read from stdin\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    if (num) {
        sum += sign * val;
    }

    write_int(file, sum);
    write(file, "\n", 1);

    close(file);

    (void)my_strlen;

    return 0;
}
