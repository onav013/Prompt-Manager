#include "../include/input_utils.h"

#include <stdio.h>
#include <string.h>

void read_line(const char *hint, char *buf, size_t size) {
    if (hint && *hint) {
        printf("%s", hint);
    }
    if (!fgets(buf, (int)size, stdin)) {
        buf[0] = '\0';
        return;
    }
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        buf[--n] = '\0';
    }
}
