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
#define SYS_GFX_ENTER 18
#define SYS_GFX_LEAVE 19
#define SYS_GFX_INFO  20
#define SYS_GFX_CLEAR 21
#define SYS_GFX_RECT  22
#define SYS_GFX_LINE  23
#define SYS_GFX_TEXT  24
#define SYS_GFX_PRESENT 25
#define SYS_INPUT_POLL 26

/* Keep this struct byte-for-byte compatible with kernel/syscall.h. */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t red_offset;
    uint32_t red_size;
    uint32_t green_offset;
    uint32_t green_size;
    uint32_t blue_offset;
    uint32_t blue_size;
    uint32_t back_buffer;
    uint32_t has_back_buffer;
} user_gfx_info_t;

typedef struct {
    uint32_t type;
    uint32_t scancode;
    uint32_t ascii;
    uint32_t extended;
    uint32_t modifiers;
    uint32_t buttons;
    int32_t  x, y;
    int32_t  dx, dy;
    uint64_t timestamp;
} user_input_event_t;

int64_t syscall0(uint64_t number);
int64_t syscall1(uint64_t number, uint64_t a1);
int64_t syscall2(uint64_t number, uint64_t a1, uint64_t a2);
int64_t syscall3(uint64_t number, uint64_t a1, uint64_t a2, uint64_t a3);
int64_t syscall4(uint64_t number, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4);
int64_t syscall5(uint64_t number, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);

#endif /* USER_SYSCALL_H */
