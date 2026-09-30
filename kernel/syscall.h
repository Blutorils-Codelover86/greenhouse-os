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
#define SYS_GFX_ENTER 18
#define SYS_GFX_LEAVE 19
#define SYS_GFX_INFO  20
#define SYS_GFX_CLEAR 21
#define SYS_GFX_RECT  22
#define SYS_GFX_LINE  23
#define SYS_GFX_TEXT  24
#define SYS_GFX_PRESENT 25
#define SYS_INPUT_POLL 26

typedef struct {
    uint32_t size;
    uint32_t flags;
    uint32_t is_dir;
} user_stat_t;

/* Surface description handed to user space.  Field order and types have to
 * match user/libc/syscall.h - the two are compiled for different address
 * spaces but share the same ABI. */
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

/* One dequeued input event.  Mirrors input_event_t without the padding the
 * kernel struct picks up, so both sides agree on every offset. */
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

typedef void (*syscall_write_hook_t)(const char* buf, size_t count);
void syscall_set_write_hook(syscall_write_hook_t hook);
syscall_write_hook_t syscall_get_write_hook(void);

#endif /* SYSCALL_H */
