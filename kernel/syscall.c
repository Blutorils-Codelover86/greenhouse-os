#include "syscall.h"
#include "process.h"
#include "vfs.h"
#include "heap.h"
#include "graphics/graphics.h"
#include "graphics/framebuffer.h"
#include "graphics/font.h"
#include "input/input.h"

extern void put_char(char c);
extern uint8_t keyboard_getchar(void);

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} RTCDateTime;
extern void rtc_get_datetime(RTCDateTime* dt);

static int s_validate_user_ptr(const void* ptr, size_t size) {
    if (!ptr) return 0;
    uintptr_t addr = (uintptr_t)ptr;
    if (addr < 0x1000) return 0; /* Null page */
    if (addr >= 0x0000800000000000ULL || (addr + size) > 0x0000800000000000ULL) return 0;
    return 1;
}

static syscall_write_hook_t s_write_hook = NULL;

void syscall_set_write_hook(syscall_write_hook_t hook) {
    s_write_hook = hook;
}

syscall_write_hook_t syscall_get_write_hook(void) {
    return s_write_hook;
}

void syscall_init(void) {
    /* Syscall interface initialized */
}

int64_t syscall_dispatch(interrupt_frame_t* frame) {
    if (!frame) return -1;

    process_t* cur = process_get_current();
    if (!cur) return -1;

    uint64_t sys_no = frame->rax;
    uint64_t a1 = frame->rdi;
    uint64_t a2 = frame->rsi;
    uint64_t a3 = frame->rdx;
    uint64_t a4 = frame->r10; /* System V uses R10 for 4th syscall arg */
    uint64_t a5 = frame->r8;  /* and R8 for the 5th */

    switch (sys_no) {
        case SYS_EXIT: {
            int code = (int)a1;
            process_exit(code);
            return 0;
        }

        case SYS_WRITE: {
            int fd = (int)a1;
            const char* buf = (const char*)a2;
            size_t count = (size_t)a3;

            if (cur->is_user && !s_validate_user_ptr(buf, count)) {
                return -1;
            }

            if (fd == 1 || fd == 2) {
                /* stdout / stderr */
                if (s_write_hook) {
                    s_write_hook(buf, count);
                }
                for (size_t i = 0; i < count; i++) {
                    put_char(buf[i]);
                }
                return (int64_t)count;
            }

            file_descriptor_t* fdesc = process_get_fd(cur, fd);
            if (!fdesc || !fdesc->node) return -1;

            int written = vfs_write(fdesc->node, fdesc->offset, count, (const uint8_t*)buf);
            if (written > 0) {
                fdesc->offset += (uint32_t)written;
            }
            return (int64_t)written;
        }

        case SYS_READ: {
            int fd = (int)a1;
            char* buf = (char*)a2;
            size_t count = (size_t)a3;

            if (cur->is_user && !s_validate_user_ptr(buf, count)) {
                return -1;
            }

            if (fd == 0) {
                /* stdin */
                size_t read_bytes = 0;
                while (read_bytes < count) {
                    uint8_t c = keyboard_getchar();
                    if (c != 0) {
                        buf[read_bytes++] = (char)c;
                        if (c == '\n') break;
                    } else {
                        process_yield();
                    }
                }
                return (int64_t)read_bytes;
            }

            file_descriptor_t* fdesc = process_get_fd(cur, fd);
            if (!fdesc || !fdesc->node) return -1;

            int n = vfs_read(fdesc->node, fdesc->offset, count, (uint8_t*)buf);
            if (n > 0) {
                fdesc->offset += (uint32_t)n;
            }
            return (int64_t)n;
        }

        case SYS_OPEN: {
            const char* path = (const char*)a1;
            int flags = (int)a2;

            if (cur->is_user && !s_validate_user_ptr(path, 1)) {
                return -1;
            }

            vfs_node_t* node = vfs_resolve_path(cur->cwd, path);
            if (!node) {
                if (flags & 0x02) { // Write / create
                    /* Extract parent dir and filename */
                    char parent_path[128];
                    char filename[64];
                    size_t len = 0;
                    while (path[len]) len++;

                    size_t slash = len;
                    while (slash > 0 && path[slash - 1] != '\\' && path[slash - 1] != '/') slash--;

                    if (slash == 0) {
                        parent_path[0] = '\0';
                        for (size_t i = 0; i < len && i < 63; i++) filename[i] = path[i];
                        filename[(len < 63) ? len : 63] = '\0';
                    } else {
                        for (size_t i = 0; i < slash - 1 && i < 127; i++) parent_path[i] = path[i];
                        parent_path[(slash - 1 < 127) ? slash - 1 : 127] = '\0';
                        for (size_t i = slash; i < len && (i - slash) < 63; i++) filename[i - slash] = path[i];
                        filename[(len - slash < 63) ? len - slash : 63] = '\0';
                    }

                    const char* pdir = (parent_path[0] != '\0') ? parent_path : cur->cwd;
                    vfs_node_t* pnode = vfs_resolve_path(cur->cwd, pdir);
                    if (pnode && (pnode->flags & VFS_DIRECTORY)) {
                        vfs_create(pnode, filename, VFS_FILE);
                        node = vfs_resolve_path(cur->cwd, path);
                    }
                }
            }

            if (!node) return -1;

            int fd = process_alloc_fd(cur, node, flags);
            return (int64_t)fd;
        }

        case SYS_CLOSE: {
            int fd = (int)a1;
            return (int64_t)process_free_fd(cur, fd);
        }

        case SYS_STAT: {
            const char* path = (const char*)a1;
            user_stat_t* st = (user_stat_t*)a2;

            if (cur->is_user && (!s_validate_user_ptr(path, 1) || !s_validate_user_ptr(st, sizeof(user_stat_t)))) {
                return -1;
            }

            vfs_node_t* node = vfs_resolve_path(cur->cwd, path);
            if (!node) return -1;

            st->size = node->size;
            st->flags = node->flags;
            st->is_dir = (node->flags & VFS_DIRECTORY) ? 1 : 0;
            return 0;
        }

        case SYS_GETPID: {
            return (int64_t)cur->pid;
        }

        case SYS_SLEEP: {
            uint64_t ms = a1;
            uint64_t ticks = (ms + 9) / 10;
            process_sleep(ticks);
            return 0;
        }

        case SYS_YIELD: {
            process_yield();
            return 0;
        }

        case SYS_SEEK: {
            int fd = (int)a1;
            int offset = (int)a2;
            int whence = (int)a3;

            file_descriptor_t* fdesc = process_get_fd(cur, fd);
            if (!fdesc || !fdesc->node) return -1;

            if (whence == 0) { // SEEK_SET
                fdesc->offset = (uint32_t)offset;
            } else if (whence == 1) { // SEEK_CUR
                fdesc->offset += (uint32_t)offset;
            } else if (whence == 2) { // SEEK_END
                fdesc->offset = fdesc->node->size + (uint32_t)offset;
            }
            return (int64_t)fdesc->offset;
        }

        case SYS_MKDIR: {
            const char* path = (const char*)a1;
            if (cur->is_user && !s_validate_user_ptr(path, 1)) return -1;

            vfs_node_t* parent = vfs_resolve_path(cur->cwd, cur->cwd);
            if (!parent) return -1;
            return (int64_t)vfs_mkdir(parent, path);
        }

        case SYS_UNLINK: {
            const char* path = (const char*)a1;
            if (cur->is_user && !s_validate_user_ptr(path, 1)) return -1;

            vfs_node_t* parent = vfs_resolve_path(cur->cwd, cur->cwd);
            if (!parent) return -1;
            return (int64_t)vfs_delete(parent, path);
        }

        case SYS_GETCWD: {
            char* buf = (char*)a1;
            size_t size = (size_t)a2;
            if (cur->is_user && !s_validate_user_ptr(buf, size)) return -1;

            size_t i;
            for (i = 0; i < size - 1 && cur->cwd[i] != '\0'; i++) {
                buf[i] = cur->cwd[i];
            }
            buf[i] = '\0';
            return 0;
        }

        case SYS_CHDIR: {
            const char* path = (const char*)a1;
            if (cur->is_user && !s_validate_user_ptr(path, 1)) return -1;

            vfs_node_t* node = vfs_resolve_path(cur->cwd, path);
            if (!node || !(node->flags & VFS_DIRECTORY)) return -1;

            size_t i;
            for (i = 0; i < sizeof(cur->cwd) - 1 && path[i] != '\0'; i++) {
                cur->cwd[i] = path[i];
            }
            cur->cwd[i] = '\0';
            return 0;
        }

        case SYS_TIME: {
            user_datetime_t* udt = (user_datetime_t*)a1;
            if (cur->is_user && !s_validate_user_ptr(udt, sizeof(user_datetime_t))) return -1;

            RTCDateTime kdt;
            rtc_get_datetime(&kdt);
            udt->year = kdt.year;
            udt->month = kdt.month;
            udt->day = kdt.day;
            udt->hour = kdt.hour;
            udt->minute = kdt.minute;
            udt->second = kdt.second;
            return 0;
        }

        case SYS_GFX_ENTER: {
            if (graphics_is_active()) return 0;
            return graphics_enter();
        }

        case SYS_GFX_LEAVE: {
            if (!graphics_is_active()) return 0;
            return graphics_leave();
        }

        case SYS_GFX_INFO: {
            user_gfx_info_t* info = (user_gfx_info_t*)a1;
            if (cur->is_user && !s_validate_user_ptr(info, sizeof(user_gfx_info_t))) return -1;

            const framebuffer_info_t* fb = framebuffer_get_info();
            if (!fb) return -1;

            info->width = fb->width;
            info->height = fb->height;
            info->pitch = fb->pitch;
            info->bpp = fb->bpp;
            info->red_offset = fb->red_offset;
            info->red_size = fb->red_size;
            info->green_offset = fb->green_offset;
            info->green_size = fb->green_size;
            info->blue_offset = fb->blue_offset;
            info->blue_size = fb->blue_size;
            info->back_buffer = 0; /* user space never maps the surface itself */
            info->has_back_buffer = (uint32_t)graphics_has_back_buffer();
            return graphics_is_active() ? 0 : -1;
        }

        /* Every drawing call needs a live surface.  Refusing loudly beats
         * scribbling into a memory map that is not there. */
        case SYS_GFX_CLEAR:
        case SYS_GFX_RECT:
        case SYS_GFX_LINE:
        case SYS_GFX_TEXT:
        case SYS_GFX_PRESENT: {
            if (!graphics_is_active()) return -1;

            switch (sys_no) {
            case SYS_GFX_CLEAR:
                graphics_clear((uint32_t)a1);
                return 0;

            case SYS_GFX_RECT: {
                int x = (int)(int64_t)a1, y = (int)(int64_t)a2;
                int w = (int)(int64_t)a3, h = (int)(int64_t)a4;
                if (w <= 0 || h <= 0) return 0;
                graphics_fill_rect(x, y, w, h, (uint32_t)a5);
                return 0;
            }

            case SYS_GFX_LINE: {
                int x0 = (int)(int64_t)a1, y0 = (int)(int64_t)a2;
                int x1 = (int)(int64_t)a3, y1 = (int)(int64_t)a4;
                graphics_draw_line(x0, y0, x1, y1, (uint32_t)a5);
                return 0;
            }

            case SYS_GFX_TEXT: {
                const char* text = (const char*)a3;
                if (cur->is_user) {
                    if (!s_validate_user_ptr(text, 1)) return -1;
                    /* Bounded scan: a string that is not terminated inside a
                     * sane window must not walk the whole address space. */
                    size_t len = 0;
                    while (len < 256 && text[len] != '\0') {
                        if (!s_validate_user_ptr(text + len, 1)) return -1;
                        len++;
                    }
                }
                draw_text((int)(int64_t)a1, (int)(int64_t)a2, text,
                          (uint32_t)a4, (uint32_t)a5, 1);
                return 0;
            }

            case SYS_GFX_PRESENT:
            default:
                graphics_present();
                return 0;
            }
        }

        case SYS_INPUT_POLL: {
            user_input_event_t* uev = (user_input_event_t*)a1;
            if (cur->is_user && !s_validate_user_ptr(uev, sizeof(user_input_event_t))) return -1;

            input_event_t kev;
            int got = input_poll_event(&kev);
            if (!got) return 0;

            uev->type = (uint32_t)kev.type;
            uev->scancode = kev.scancode;
            uev->ascii = kev.ascii;
            uev->extended = kev.extended;
            uev->modifiers = kev.modifiers;
            uev->buttons = kev.buttons;
            uev->x = kev.x;
            uev->y = kev.y;
            uev->dx = kev.dx;
            uev->dy = kev.dy;
            uev->timestamp = kev.timestamp;
            return 1;
        }

        default:
            return -38; /* -ENOSYS: Function not implemented */
    }
}
