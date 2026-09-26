/* ==============================================================================
 * Greenhouse OS - Phase 7: Window Manager (implementation)
 * ==============================================================================
 */

#include "wm.h"
#include "compositor.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"

static window_t   wm_windows[WINDOW_MAX];
static int        wm_order[WINDOW_MAX];
static int        wm_count = 0;
static int        wm_focused = -1;
static window_id_t wm_next_id = 1;
static int        wm_redraw_needed = 1;
static wm_stats_t wm_stats;

static void wm_reorder_from(int index) {
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

    for (int i = index; i < wm_count - 1; i++) {
        wm_windows[i] = wm_windows[i + 1];
    }
    wm_count--;

    int new_order[WINDOW_MAX];
    int n = 0;
    for (int i = 0; i < wm_count; i++) {
        int entry = wm_order[i];
        if (entry == index) continue;
        if (entry > index) entry--;
        new_order[n++] = entry;
    }
    for (int i = 0; i < n; i++) wm_order[i] = new_order[i];

    if (wm_focused == index) wm_focused = (wm_count > 0) ? wm_count - 1 : -1;
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

int wm_hit_test(int px, int py, window_t** out_win, int* out_zone) {
    (void)px; (void)py; (void)out_win; (void)out_zone;
    return 0;
}

int wm_handle_event(const input_event_t* ev) {
    (void)ev;
    return 0;
}

void wm_compose(void) {
}

void wm_request_redraw(void) { wm_redraw_needed = 1; }
int  wm_needs_redraw(void)    { return wm_redraw_needed; }

void wm_get_stats(wm_stats_t* out) {
    if (out) *out = wm_stats;
}
