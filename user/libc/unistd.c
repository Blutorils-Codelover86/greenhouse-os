#include "unistd.h"

int write(int fd, const void* buf, size_t count) {
    return (int)syscall3(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

int read(int fd, void* buf, size_t count) {
    return (int)syscall3(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

int open(const char* path, int flags) {
    return (int)syscall2(SYS_OPEN, (uint64_t)path, (uint64_t)flags);
}

int close(int fd) {
    return (int)syscall1(SYS_CLOSE, (uint64_t)fd);
}

void exit(int code) {
    syscall1(SYS_EXIT, (uint64_t)code);
    while (1) {}
}

int getpid(void) {
    return (int)syscall0(SYS_GETPID);
}

void sleep(uint64_t ms) {
    syscall1(SYS_SLEEP, ms);
}

void yield(void) {
    syscall0(SYS_YIELD);
}

int mkdir(const char* path) {
    return (int)syscall1(SYS_MKDIR, (uint64_t)path);
}

int unlink(const char* path) {
    return (int)syscall1(SYS_UNLINK, (uint64_t)path);
}

int getcwd(char* buf, size_t size) {
    return (int)syscall2(SYS_GETCWD, (uint64_t)buf, (uint64_t)size);
}

int chdir(const char* path) {
    return (int)syscall1(SYS_CHDIR, (uint64_t)path);
}

int spawn(const char* path) {
    return (int)syscall1(SYS_SPAWN, (uint64_t)path);
}

int waitpid(int pid, int* status) {
    return (int)syscall2(SYS_WAITPID, (uint64_t)pid, (uint64_t)status);
}
