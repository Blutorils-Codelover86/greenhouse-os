/* ==============================================================================
 * Greenhouse OS - Phase 7: Window Objects (implementation)
 * ==============================================================================
 */

#include "window.h"

static int window_copy_string(char* dst, size_t cap, const char* src) {
    size_t i = 0;
    if (src) {
        for (; src[i] && i + 1 < cap; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
    return (int)i;
}

void window_init(window_t* win, window_id_t id, const char* title,
                 int x, int y, int w, int h, uint32_t accent) {
    if (!win) return;

    win->id = id;
    window_copy_string(win->title, WINDOW_TITLE_LEN, title ? title : "window");

    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->min_w = 160;
    win->min_h = 80;
    win->visible = 1;
    win->flags = WINDOW_FLAG_RESIZABLE;

    win->accent = accent;
    win->title_bg = 0xFF2D3748;
    win->title_fg = 0xFFE8EDF5;
    win->bg_color = 0xFF1B2333;
    win->client_color = 0xFF111827;
    win->fg_color = 0xFFE8EDF5;
    win->border_color = 0xFF4A5568;

    win->user_data = 0;
    win->on_draw = 0;
    win->on_event = 0;
}

void window_set_title(window_t* win, const char* title) {
    if (!win) return;
    window_copy_string(win->title, WINDOW_TITLE_LEN, title);
}

void window_set_colors(window_t* win, uint32_t bg, uint32_t fg, uint32_t border) {
    if (!win) return;
    win->bg_color = bg;
    win->fg_color = fg;
    win->border_color = border;
}

void window_set_client_color(window_t* win, uint32_t color) {
    if (!win) return;
    win->client_color = color;
}

int window_includes(const window_t* win, int x, int y) {
    if (!win || !win->visible) return 0;
    if (x < win->x || y < win->y) return 0;
    if (x >= win->x + win->w || y >= win->y + win->h) return 0;
    return 1;
}

void window_invalidate(window_t* win) {
    (void)win;   /* the compositor redraws the whole screen each frame */
}
