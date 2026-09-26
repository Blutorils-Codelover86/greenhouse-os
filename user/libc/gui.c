#include "gui.h"

/* Thin syscall wrappers.  Everything interesting happens in the kernel: user
 * space has no mapping of the framebuffer and no VGA ports, so every pixel and
 * every event travels through int 0x80. */

int gfx_enter(void) {
    return (int)syscall0(SYS_GFX_ENTER);
}

int gfx_leave(void) {
    return (int)syscall0(SYS_GFX_LEAVE);
}

int gfx_get_info(gfx_surface_t* out) {
    user_gfx_info_t info;
    int rc = (int)syscall1(SYS_GFX_INFO, (uint64_t)&info);
    if (rc < 0 || !out) return rc < 0 ? rc : -1;

    out->width = info.width;
    out->height = info.height;
    out->bpp = info.bpp;
    return 0;
}

void gfx_clear(uint32_t color) {
    syscall1(SYS_GFX_CLEAR, (uint64_t)color);
}

void gfx_rect(int x, int y, int w, int h, uint32_t color) {
    syscall5(SYS_GFX_RECT, (uint64_t)(int64_t)x, (uint64_t)(int64_t)y,
             (uint64_t)(int64_t)w, (uint64_t)(int64_t)h, (uint64_t)color);
}

void gfx_line(int x0, int y0, int x1, int y1, uint32_t color) {
    syscall5(SYS_GFX_LINE, (uint64_t)(int64_t)x0, (uint64_t)(int64_t)y0,
             (uint64_t)(int64_t)x1, (uint64_t)(int64_t)y1, (uint64_t)color);
}

void gfx_text(int x, int y, const char* text, uint32_t fg, uint32_t bg) {
    if (!text) return;
    syscall5(SYS_GFX_TEXT, (uint64_t)(int64_t)x, (uint64_t)(int64_t)y,
             (uint64_t)text, (uint64_t)fg, (uint64_t)bg);
}

void gfx_present(void) {
    syscall0(SYS_GFX_PRESENT);
}

int gfx_poll_event(user_input_event_t* out) {
    if (!out) return -1;
    return (int)syscall1(SYS_INPUT_POLL, (uint64_t)out);
}
