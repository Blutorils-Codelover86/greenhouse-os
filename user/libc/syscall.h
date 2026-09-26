#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define SYS_EXIT      1
#define SYS_WRITE     2
#define SYS_READ      3
#define SYS_OPEN      4
#define SYS_CLOSE     5
#define SYS_STAT      6
#define SYS_GETPID    7
#define SYS_SLEEP     8
#define SYS_YIELD     9
#define SYS_SEEK      10
#define SYS_MKDIR     11
#define SYS_UNLINK    12
#define SYS_GETCWD    13
#define SYS_CHDIR     14
#define SYS_SPAWN     15
#define SYS_WAITPID   16
#define SYS_TIME      17

int64_t syscall0(uint64_t number);
int64_t syscall1(uint64_t number, uint64_t a1);
int64_t syscall2(uint64_t number, uint64_t a1, uint64_t a2);
int64_t syscall3(uint64_t number, uint64_t a1, uint64_t a2, uint64_t a3);
int64_t syscall4(uint64_t number, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4);
int64_t syscall5(uint64_t number, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);

#endif /* USER_SYSCALL_H */
