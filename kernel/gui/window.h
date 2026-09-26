/* ==============================================================================
 * Greenhouse OS - Phase 7: Window Objects
 *
 * A window is a plain description: position, size, appearance and the two
 * callbacks the window manager uses (draw the client area, handle an input
 * event).  Everything about stacking, focus, dragging and decoration belongs to
 * the window manager; everything about pixels belongs to the graphics
 * subsystem.  A window object never touches the screen itself.
 * ==============================================================================
 */

#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>
#include "../input/input.h"

#define WINDOW_MAX            16
#define WINDOW_TITLE_LEN      48

#define WINDOW_FLAG_RESIZABLE 0x01
#define WINDOW_FLAG_NO_DECOR  0x02
#define WINDOW_FLAG_MODAL     0x04

typedef uint32_t window_id_t;
#define WINDOW_ID_NONE 0

typedef struct window window_t;

/* Client area callback: draw inside the (already clipped) client rectangle. */
typedef void (*window_draw_fn)(window_t* win, int cx, int cy, int cw, int ch);

/* Event callback: return 1 when the event was consumed. */
typedef int  (*window_event_fn)(window_t* win, const input_event_t* ev);

struct window {
    window_id_t id;
    char       title[WINDOW_TITLE_LEN];
    int        x, y, w, h;
    int        min_w, min_h;
    int        visible;
    int        flags;

    uint32_t   bg_color;
    uint32_t   fg_color;
    uint32_t   border_color;
    uint32_t   title_bg;
    uint32_t   title_fg;
    uint32_t   accent;
    uint32_t   client_color;

    void*          user_data;
    window_draw_fn on_draw;
    window_event_fn on_event;
};

void  window_init(window_t* win, window_id_t id, const char* title,
                  int x, int y, int w, int h, uint32_t accent);
void  window_set_title(window_t* win, const char* title);
void  window_set_colors(window_t* win, uint32_t bg, uint32_t fg, uint32_t border);
void  window_set_client_color(window_t* win, uint32_t color);
int   window_includes(const window_t* win, int x, int y);
void  window_invalidate(window_t* win);

#endif /* WINDOW_H */
