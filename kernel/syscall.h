#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "process.h"

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

typedef struct {
    uint32_t size;
    uint32_t flags;
    uint32_t is_dir;
} user_stat_t;

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} user_datetime_t;

void syscall_init(void);
int64_t syscall_dispatch(interrupt_frame_t* frame);

#endif /* SYSCALL_H */
