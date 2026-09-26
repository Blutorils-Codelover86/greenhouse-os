#ifndef USER_STRING_H
#define USER_STRING_H

#include <stdint.h>
#include <stddef.h>

size_t strlen(const char* s);
int strcmp(const char* s1, const char* s2);
int strcasecmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
void* memset(void* dest, int val, size_t count);
void* memcpy(void* dest, const void* src, size_t count);

#endif /* USER_STRING_H */
