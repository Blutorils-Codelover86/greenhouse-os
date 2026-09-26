#ifndef USER_GUI_H
#define USER_GUI_H

#include <stdint.h>
#include "syscall.h"

/* Drawing surface, filled in by gfx_get_info(). */
typedef struct {
    uint32_t width, height;
    uint32_t bpp;
} gfx_surface_t;

/* Input events, mirroring the kernel event types. */
#define GFX_EVENT_NONE              0
#define GFX_EVENT_KEY_DOWN          1
#define GFX_EVENT_KEY_UP            2
#define GFX_EVENT_MOUSE_MOVE        3
#define GFX_EVENT_MOUSE_BUTTON_DOWN 4
#define GFX_EVENT_MOUSE_BUTTON_UP   5
#define GFX_EVENT_MOUSE_WHEEL       6

#define GFX_MOD_SHIFT   0x0001
#define GFX_MOD_CTRL    0x0002
#define GFX_MOD_ALT     0x0004
#define GFX_MOD_CAPS    0x0008

#define GFX_MOUSE_LEFT    0x01
#define GFX_MOUSE_RIGHT   0x02
#define GFX_MOUSE_MIDDLE  0x04

/* 0x00RRGGBB.  The kernel maps this onto whatever layout the adapter uses. */
#define GFX_RGB(r, g, b) ((uint32_t)(((r) & 0xFF) << 16 | ((g) & 0xFF) << 8 | ((b) & 0xFF)))

int  gfx_enter(void);
int  gfx_leave(void);
int  gfx_get_info(gfx_surface_t* out);

void gfx_clear(uint32_t color);
void gfx_rect(int x, int y, int w, int h, uint32_t color);
void gfx_line(int x0, int y0, int x1, int y1, uint32_t color);
void gfx_text(int x, int y, const char* text, uint32_t fg, uint32_t bg);
void gfx_present(void);

/* 1 = an event was stored in *out, 0 = the queue was empty, negative = error. */
int  gfx_poll_event(user_input_event_t* out);

#endif /* USER_GUI_H */
