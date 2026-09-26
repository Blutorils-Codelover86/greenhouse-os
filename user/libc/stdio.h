#ifndef USER_STDIO_H
#define USER_STDIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

void putchar(char c);
void puts(const char* s);
void print(const char* s);
void printf(const char* format, ...);
char getchar(void);
int readline(char* buf, size_t max_len);

#endif /* USER_STDIO_H */
