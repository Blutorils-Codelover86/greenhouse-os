#include "stdio.h"
#include "unistd.h"
#include "string.h"

void putchar(char c) {
    write(1, &c, 1);
}

void puts(const char* s) {
    if (s) {
        write(1, s, strlen(s));
    }
    putchar('\n');
}

void print(const char* s) {
    if (s) {
        write(1, s, strlen(s));
    }
}

static void print_num(int64_t n, int base) {
    char buf[32];
    int idx = 0;
    int is_neg = 0;

    if (n < 0 && base == 10) {
        is_neg = 1;
        n = -n;
    }

    uint64_t un = (uint64_t)n;
    if (un == 0) {
        putchar('0');
        return;
    }

    while (un > 0) {
        uint64_t rem = un % base;
        buf[idx++] = (rem < 10) ? (char)('0' + rem) : (char)('A' + (rem - 10));
        un /= base;
    }

    if (is_neg) {
        putchar('-');
    }

    while (idx > 0) {
        putchar(buf[--idx]);
    }
}

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd':
                case 'i': {
                    int val = va_arg(args, int);
                    print_num((int64_t)val, 10);
                    break;
                }
                case 'u': {
                    unsigned int val = va_arg(args, unsigned int);
                    print_num((int64_t)val, 10);
                    break;
                }
                case 'x':
                case 'X': {
                    unsigned int val = va_arg(args, unsigned int);
                    print_num((int64_t)val, 16);
                    break;
                }
                case 's': {
                    const char* s = va_arg(args, const char*);
                    print(s ? s : "(null)");
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    putchar(c);
                    break;
                }
                case '%': {
                    putchar('%');
                    break;
                }
                default:
                    putchar('%');
                    putchar(*format);
                    break;
            }
        } else {
            putchar(*format);
        }
        format++;
    }

    va_end(args);
}

char getchar(void) {
    char c = 0;
    read(0, &c, 1);
    return c;
}

int readline(char* buf, size_t max_len) {
    size_t idx = 0;
    while (idx < max_len - 1) {
        char c = getchar();
        if (c == '\r' || c == '\n') {
            putchar('\n');
            break;
        }
        if (c == '\b') {
            if (idx > 0) {
                idx--;
                putchar('\b');
            }
            continue;
        }
        if (c >= ' ' && c <= '~') {
            buf[idx++] = c;
            putchar(c);
        }
    }
    buf[idx] = '\0';
    return (int)idx;
}
