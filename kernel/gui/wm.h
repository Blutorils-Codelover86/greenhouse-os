/* ==============================================================================
 * Greenhouse OS - Phase 7: Window Manager
 *
 * Responsibilities, in one place:
 *   - keeps the window table and the z-order,
 *   - composites frames (desktop background, windows, decorations, cursor),
 *   - decides which window a pointer event belongs to (hit testing),
 *   - focus handling, dragging by the title bar and resizing by the border,
 *   - routes events to the window callbacks.
 *
 * It deliberately knows nothing about PS/2 or about VGA registers.
 * ==============================================================================
 */

#ifndef WM_H
#define WM_H

#include <stdint.h>
#include "window.h"

/* Title bar / border geometry used by hit testing and composition */
#define WM_TITLE_HEIGHT   24
#define WM_BORDER_SIZE    4
#define WM_RESIZE_ZONE    6
#define WM_BUTTON_W       16
#define WM_BUTTON_H       16
#define WM_BUTTON_MARGIN  4

void wm_init(void);

window_t* wm_create_window(const char* title, int x, int y, int w, int h, uint32_t accent);
int      wm_destroy_window(window_id_t id);
int      wm_window_count(void);
window_t* wm_get_window(window_id_t id);
window_t* wm_window_at_index(int index);          /* 0 = bottom of the stack */
window_t* wm_top_window(void);
window_t* wm_focused_window(void);
int      wm_focus_window(window_id_t id);
int      wm_raise_window(window_id_t id);
int      wm_set_bounds(window_id_t id, int x, int y, int w, int h);

/* Pointer/graphics coordinates of a window frame and its client area. */
void wm_frame_rect(const window_t* win, int* x, int* y, int* w, int* h);
void wm_client_rect(const window_t* win, int* x, int* y, int* w, int* h);

int  wm_hit_test(int px, int py, window_t** out_win, int* out_zone);
int  wm_handle_event(const input_event_t* ev);
void wm_compose(void);
void wm_request_redraw(void);
int  wm_needs_redraw(void);

typedef struct {
    uint32_t composed_frames;
    uint32_t drag_moves;
    uint32_t resize_moves;
    uint32_t clicks;
    uint32_t focus_changes;
} wm_stats_t;
void wm_get_stats(wm_stats_t* out);

#endif /* WM_H */
