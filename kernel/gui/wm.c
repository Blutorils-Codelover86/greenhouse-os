/* ==============================================================================
 * Greenhouse OS - Phase 7: Window Manager (implementation)
 * ==============================================================================
 */

#include "wm.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"

#define WM_ZONE_NONE    0
#define WM_ZONE_CLIENT  1
#define WM_ZONE_TITLE   2
#define WM_ZONE_RESIZE  3
#define WM_ZONE_CLOSE   4

static window_t  wm_windows[WINDOW_MAX];
static int        wm_order[WINDOW_MAX];   /* back to front, window indices */
static int        wm_count = 0;
static int        wm_focused = -1;       /* index into wm_windows */
static window_id_t wm_next_id = 1;

static int  wm_redraw_needed = 1;
static int  wm_drag_active = 0;
static int  wm_drag_index = -1;
static int  wm_drag_offset_x = 0;
static int  wm_drag_offset_y = 0;
static int  wm_resize_active = 0;
static int  wm_resize_grab_x = 0;
static int  wm_resize_grab_y = 0;
static int  wm_resize_w = 0;
static int  wm_resize_h = 0;

static wm_stats_t wm_stats;

/* ------------------------------------------------------------------------------
 * Table management
 * -------------------------------------------------------------------------- */

static void wm_reorder_from(int index) {
    /* Move a window to the front (end of wm_order). */
    int pos = -1;
    for (int i = 0; i < wm_count; i++) {
        if (wm_order[i] == index) { pos = i; break; }
    }
    if (pos < 0 || pos == wm_count - 1) return;

    for (int i = pos; i < wm_count - 1; i++) wm_order[i] = wm_order[i + 1];
    wm_order[wm_count - 1] = index;
}

static int wm_find_index(window_id_t id) {
    for (int i = 0; i < wm_count; i++) {
        if (wm_windows[i].id == id) return i;
    }
    return -1;
}

void wm_init(void) {
    for (int i = 0; i < WINDOW_MAX; i++) wm_order[i] = -1;
    wm_count = 0;
    wm_focused = -1;
    wm_next_id = 1;
    wm_redraw_needed = 1;
    wm_drag_active = 0;
    wm_drag_index = -1;
    wm_resize_active = 0;
    wm_stats.composed_frames = 0;
    wm_stats.drag_moves = 0;
    wm_stats.resize_moves = 0;
    wm_stats.clicks = 0;
    wm_stats.focus_changes = 0;
}

window_t* wm_create_window(const char* title, int x, int y, int w, int h, uint32_t accent) {
    if (wm_count >= WINDOW_MAX) return 0;

    int index = wm_count;
    window_init(&wm_windows[index], wm_next_id++, title, x, y, w, h, accent);

    for (int i = 0; i < wm_count; i++) wm_order[i] = i;
    wm_order[wm_count] = index;
    wm_count++;

    wm_focused = index;
    wm_stats.focus_changes++;
    wm_redraw_needed = 1;
    return &wm_windows[index];
}

int wm_destroy_window(window_id_t id) {
    int index = wm_find_index(id);
    if (index < 0) return -1;

    /* Compact the window table, remembering which entry moved where. */
    int moved_from = -1;
    for (int i = index; i < wm_count - 1; i++) {
        wm_windows[i] = wm_windows[i + 1];
        if (wm_order[i] == index + 1) moved_from = i;
    }
    wm_count--;

    /* Rebuild the z-order: drop the closed window, keep the stacking order. */
    int new_order[WINDOW_MAX];
    int n = 0;
    for (int i = 0; i < wm_count; i++) {
        int entry = wm_order[i];
        if (entry == index) continue;
        if (entry > index) entry--;
        new_order[n++] = entry;
    }
    for (int i = 0; i < n; i++) wm_order[i] = new_order[i];
    (void)moved_from;

    if (wm_drag_index == index) { wm_drag_active = 0; wm_drag_index = -1; }
    if (wm_focused == index) wm_focused = (wm_count > 0) ? wm_count - 1 : -1;
    if (wm_resize_active) wm_resize_active = 0;

    wm_redraw_needed = 1;
    return 0;
}

int wm_window_count(void) { return wm_count; }

window_t* wm_get_window(window_id_t id) {
    int index = wm_find_index(id);
    return (index < 0) ? 0 : &wm_windows[index];
}

window_t* wm_window_at_index(int index) {
    if (index < 0 || index >= wm_count) return 0;
    return &wm_windows[wm_order[index]];
}

window_t* wm_top_window(void) {
    return (wm_count > 0) ? &wm_windows[wm_order[wm_count - 1]] : 0;
}

window_t* wm_focused_window(void) {
    return (wm_focused >= 0 && wm_focused < wm_count) ? &wm_windows[wm_focused] : 0;
}

int wm_focus_window(window_id_t id) {
    int index = wm_find_index(id);
    if (index < 0) return -1;
    if (index != wm_focused) {
        wm_focused = index;
        wm_stats.focus_changes++;
    }
    wm_reorder_from(index);
    wm_redraw_needed = 1;
    return 0;
}

int wm_raise_window(window_id_t id) {
    int index = wm_find_index(id);
    if (index < 0) return -1;
    wm_reorder_from(index);
    wm_redraw_needed = 1;
    return 0;
}

int wm_set_bounds(window_id_t id, int x, int y, int w, int h) {
    window_t* win = wm_get_window(id);
    if (!win) return -1;

    int sw = graphics_get_width();
    int sh = graphics_get_height();
    if (w < win->min_w) w = win->min_w;
    if (h < win->min_h) h = win->min_h;
    if (w > sw) w = sw;
    if (h > sh) h = sh;
    if (x > sw - w) x = sw - w;
    if (y > sh - h) y = sh - h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    wm_redraw_needed = 1;
    return 0;
}

void wm_frame_rect(const window_t* win, int* x, int* y, int* w, int* h) {
    if (!win) return;
    if (x) *x = win->x;
    if (y) *y = win->y;
    if (w) *w = win->w;
    if (h) *h = win->h;
}

void wm_client_rect(const window_t* win, int* x, int* y, int* w, int* h) {
    if (!win) return;

    int decor = (win->flags & WINDOW_FLAG_NO_DECOR) ? 0 : WM_BORDER_SIZE;
    int title = (win->flags & WINDOW_FLAG_NO_DECOR) ? 0 : WM_TITLE_HEIGHT;

    if (x) *x = win->x + decor;
    if (y) *y = win->y + decor + title;
    if (w) *w = win->w - 2 * decor;
    if (h) *h = win->h - 2 * decor - title;
}

/* ------------------------------------------------------------------------------
 * Hit testing
 * -------------------------------------------------------------------------- */

int wm_hit_test(int px, int py, window_t** out_win, int* out_zone) {
    if (out_win) *out_win = 0;
    if (out_zone) *out_zone = WM_ZONE_NONE;

    for (int i = wm_count - 1; i >= 0; i--) {
        window_t* win = &wm_windows[wm_order[i]];
        if (!window_includes(win, px, py)) continue;

        int zone = WM_ZONE_CLIENT;
        int decor = (win->flags & WINDOW_FLAG_NO_DECOR) ? 0 : WM_BORDER_SIZE;

        if (decor) {
            int left = px - win->x;
            int top = py - win->y;
            if (left < WM_RESIZE_ZONE || top < WM_RESIZE_ZONE ||
                left >= win->w - WM_RESIZE_ZONE || top >= win->h - WM_RESIZE_ZONE) {
                zone = WM_ZONE_RESIZE;
            }
        }

        if (zone == WM_ZONE_CLIENT && !(win->flags & WINDOW_FLAG_NO_DECOR)) {
            int local_y = py - (win->y + decor);
            if (local_y < WM_TITLE_HEIGHT) {
                zone = WM_ZONE_TITLE;
                /* The close box sits at the right end of the title bar. */
                int box_x = win->x + win->w - WM_BORDER_SIZE - WM_BUTTON_W - WM_BUTTON_MARGIN;
                if (px >= box_x && px < box_x + WM_BUTTON_W) zone = WM_ZONE_CLOSE;
            }
        }

        if (out_win) *out_win = win;
        if (out_zone) *out_zone = zone;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------------
 * Event routing
 * -------------------------------------------------------------------------- */

static int wm_index_of(const window_t* win) {
    if (!win) return -1;
    for (int i = 0; i < wm_count; i++) {
        if (&wm_windows[i] == win) return i;
    }
    return -1;
}

int wm_handle_event(const input_event_t* ev) {
    if (!ev) return 0;

    if (ev->type == INPUT_EVENT_MOUSE_MOVE) {
        if (wm_drag_active && wm_drag_index >= 0) {
            window_t* win = &wm_windows[wm_drag_index];
            int x = ev->x - wm_drag_offset_x;
            int y = ev->y - wm_drag_offset_y;

            int sw = graphics_get_width(), sh = graphics_get_height();
            if (x > sw - win->w) x = sw - win->w;
            if (y > sh - win->h) y = sh - win->h;
            if (x < 0) x = 0;
            if (y < 0) y = 0;

            win->x = x;
            win->y = y;
            wm_stats.drag_moves++;
            wm_redraw_needed = 1;
        } else if (wm_resize_active && wm_focused >= 0) {
            window_t* win = &wm_windows[wm_focused];
            int w = wm_resize_w + (ev->x - wm_resize_grab_x);
            int h = wm_resize_h + (ev->y - wm_resize_grab_y);
            if (w < win->min_w) w = win->min_w;
            if (h < win->min_h) h = win->min_h;
            int sw = graphics_get_width(), sh = graphics_get_height();
            if (w > sw) w = sw;
            if (h > sh) h = sh;
            win->w = w;
            win->h = h;
            wm_stats.resize_moves++;
            wm_redraw_needed = 1;
        } else {
            /* Hover state: redraw so widgets and the title bar can react. */
            window_t* hover = 0;
            int zone = WM_ZONE_NONE;
            wm_hit_test(ev->x, ev->y, &hover, &zone);
            for (int i = 0; i < wm_count; i++) wm_redraw_needed = 1;
        }
        return 0;
    }

    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        window_t* win = 0;
        int zone = WM_ZONE_NONE;
        wm_stats.clicks++;

        if (wm_hit_test(ev->x, ev->y, &win, &zone)) {
            wm_focus_window(win->id);

            if (zone == WM_ZONE_TITLE) {
                wm_drag_active = 1;
                wm_drag_index = wm_index_of(win);
                wm_drag_offset_x = ev->x - win->x;
                wm_drag_offset_y = ev->y - win->y;
            } else if (zone == WM_ZONE_RESIZE && (win->flags & WINDOW_FLAG_RESIZABLE)) {
                wm_resize_active = 1;
                wm_focused = wm_index_of(win);
                wm_resize_grab_x = ev->x;
                wm_resize_grab_y = ev->y;
                wm_resize_w = win->w;
                wm_resize_h = win->h;
            } else if (zone == WM_ZONE_CLOSE) {
                wm_destroy_window(win->id);
                return 1;
            } else if (zone == WM_ZONE_CLIENT && win->on_event) {
                return win->on_event(win, ev);
            }
        }
        wm_redraw_needed = 1;
        return 0;
    }

    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_UP) {
        if (wm_drag_active || wm_resize_active) {
            wm_drag_active = 0;
            wm_drag_index = -1;
            wm_resize_active = 0;
            wm_redraw_needed = 1;
        }
        window_t* win = wm_top_window();
        if (win && win->on_event) return win->on_event(win, ev);
        return 0;
    }

    if (ev->type == INPUT_EVENT_KEY_DOWN || ev->type == INPUT_EVENT_MOUSE_WHEEL) {
        window_t* win = wm_focused_window();
        if (win && win->on_event) return win->on_event(win, ev);
        return 0;
    }

    return 0;
}

/* ------------------------------------------------------------------------------
 * Composition
 * -------------------------------------------------------------------------- */

static void wm_draw_close_button(const window_t* win) {
    int box_x = win->x + win->w - WM_BORDER_SIZE - WM_BUTTON_W - WM_BUTTON_MARGIN;
    int box_y = win->y + WM_BORDER_SIZE + (WM_TITLE_HEIGHT - WM_BUTTON_H) / 2;

    graphics_fill_rect(box_x, box_y, WM_BUTTON_W, WM_BUTTON_H, 0xFF3A4658);
    for (int i = 0; i < 6; i++) {
        graphics_put_pixel(box_x + 5 + i, box_y + 5 + i, 0xFFE8EDF5);
        graphics_put_pixel(box_x + 10 - i, box_y + 5 + i, 0xFFE8EDF5);
    }
}

static void wm_draw_window(window_t* win) {
    if (!win->visible) return;

    int focused = (win == wm_focused_window());

    graphics_fill_rect(win->x, win->y, win->w, win->h, win->bg_color);
    graphics_fill_rect(win->x, win->y, win->w, WM_BORDER_SIZE,
                       focused ? win->accent : win->border_color);
    graphics_fill_rect(win->x, win->y + win->h - WM_BORDER_SIZE, win->w, WM_BORDER_SIZE,
                       focused ? win->accent : win->border_color);
    graphics_fill_rect(win->x, win->y, WM_BORDER_SIZE, win->h,
                       focused ? win->accent : win->border_color);
    graphics_fill_rect(win->x + win->w - WM_BORDER_SIZE, win->y, WM_BORDER_SIZE, win->h,
                       focused ? win->accent : win->border_color);

    if (win->flags & WINDOW_FLAG_NO_DECOR) return;

    graphics_fill_rect(win->x + WM_BORDER_SIZE, win->y + WM_BORDER_SIZE,
                       win->w - 2 * WM_BORDER_SIZE, WM_TITLE_HEIGHT, win->title_bg);
    graphics_fill_rect(win->x + WM_BORDER_SIZE + 3, win->y + WM_BORDER_SIZE,
                       4, WM_TITLE_HEIGHT - 2, win->accent);

    uint32_t title_color = focused ? win->title_fg : 0xFF9AA5B6;
    int max_chars = (win->w - 2 * WM_BORDER_SIZE - 40) / (font_glyph_width() + 1);
    if (max_chars > WINDOW_TITLE_LEN - 1) max_chars = WINDOW_TITLE_LEN - 1;
    if (max_chars > 0) {
        char label[WINDOW_TITLE_LEN];
        int i = 0;
        for (; win->title[i] && i < max_chars; i++) label[i] = win->title[i];
        label[i] = '\0';
        draw_text(win->x + WM_BORDER_SIZE + 12, win->y + WM_BORDER_SIZE + 4,
                  label, title_color, win->title_bg, 1);
    }

    wm_draw_close_button(win);

    int cx, cy, cw, ch;
    wm_client_rect(win, &cx, &cy, &cw, &ch);
    if (cw > 0 && ch > 0) {
        graphics_fill_rect(cx, cy, cw, ch, win->client_color);
        graphics_set_clip(cx, cy, cw, ch);
        if (win->on_draw) win->on_draw(win, cx, cy, cw, ch);
        graphics_clear_clip();
    }
}

void wm_compose(void) {
    int sw = graphics_get_width();
    int sh = graphics_get_height();

    graphics_fill_gradient_v(0, 0, sw, sh, 0xFF16233A, 0xFF0B1220);

    /* Desktop grid so the pointer position is readable at a glance. */
    for (int x = 0; x < sw; x += 64) {
        graphics_fill_rect(x, 0, 1, sh, 0xFF1C2C48);
    }
    for (int y = 0; y < sh; y += 64) {
        graphics_fill_rect(0, y, sw, 1, 0xFF1C2C48);
    }

    for (int i = 0; i < wm_count; i++) wm_draw_window(&wm_windows[wm_order[i]]);

    wm_stats.composed_frames++;
    wm_redraw_needed = 0;
}

void wm_request_redraw(void) { wm_redraw_needed = 1; }
int  wm_needs_redraw(void)    { return wm_redraw_needed; }

void wm_get_stats(wm_stats_t* out) {
    if (out) *out = wm_stats;
}
