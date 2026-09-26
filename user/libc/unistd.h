#ifndef USER_UNISTD_H
#define USER_UNISTD_H

#include <stdint.h>
#include <stddef.h>
#include "syscall.h"

int write(int fd, const void* buf, size_t count);
int read(int fd, void* buf, size_t count);
int open(const char* path, int flags);
int close(int fd);
void exit(int code);
int getpid(void);
void sleep(uint64_t ms);
void yield(void);
int mkdir(const char* path);
int unlink(const char* path);
int getcwd(char* buf, size_t size);
int chdir(const char* path);
int spawn(const char* path);
int waitpid(int pid, int* status);

#endif /* USER_UNISTD_H */
