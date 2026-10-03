#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

static void write_int(int fd, long x) {
    char tmp[32];
    int i = 0;
    int neg = 0;

    if (x == 0) {
        write(fd, "0", 1);
        return;
    }
    if (x < 0) {
        neg = 1;
        x = -x;
    }
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

int main(void) {
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
        const char msg[] = "child: empty filename\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    int fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd == -1) {
        const char msg[] = "child: open failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    long sum = 0;
    long val = 0;
    int sign = 1;
    int in_num = 0;

    char buf[256];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (ssize_t k = 0; k < n; ++k) {
            char ch = buf[k];

            if (ch == '-') {
                sign = -1;
                in_num = 1;
            } else if (ch >= '0' && ch <= '9') {
                val = val * 10 + (ch - '0');
                in_num = 1;
            } else {
                if (in_num) {
                    sum += sign * val;
                    val = 0;
                    sign = 1;
                    in_num = 0;
                }
            }
        }
    }
    if (in_num) {
        sum += sign * val;
    }

    write_int(fd, sum);
    write(fd, "\n", 1);

    close(fd);
    return 0;
}
