/* ==============================================================================
 * Greenhouse OS - Phase 7: Widgets
 *
 * A very small immediate-mode style widget layer: widgets live in plain structs,
 * draw themselves through the graphics primitives and test pointer input
 * against their own rectangles.  Enough for a real demonstration window
 * (buttons, a checkbox, a progress bar) without a retained scene graph.
 * ==============================================================================
 */

#ifndef WIDGET_H
#define WIDGET_H

#include <stdint.h>

#define WIDGET_MAX 32

typedef enum {
    WIDGET_NONE = 0,
    WIDGET_LABEL,
    WIDGET_BUTTON,
    WIDGET_CHECKBOX,
    WIDGET_PROGRESS
} widget_type_t;

typedef struct widget {
    widget_type_t type;
    int      x, y, w, h;
    char     text[32];
    int      enabled;
    int      pressed;
    int      checked;
    int      percent;          /* 0..100 for WIDGET_PROGRESS */
    uint32_t bg;
    uint32_t fg;
    uint32_t border;
    uint32_t accent;
    void*    user_data;
} widget_t;

void widget_init_array(widget_t* widgets, int count);
void widget_set_rect(widget_t* w, int x, int y, int width, int height);
void widget_set_text(widget_t* w, const char* text);
void widget_set_pressed(widget_t* w, int pressed);
void widget_set_checked(widget_t* w, int checked);
void widget_set_percent(widget_t* w, int percent);
int  widget_is_checked(const widget_t* w);
int  widget_get_percent(const widget_t* w);

/* Draw the widget.  Coordinates are window client area relative. */
void widget_draw(const widget_t* w);

/* Returns 1 when the widget consumed a mouse button press, 0 otherwise.
 * Coordinates are window client area relative. */
int  widget_handle_mouse(widget_t* w, int px, int py, int pressed);

/* Draw a whole array inside a client area of the given size. */
void widget_draw_all(const widget_t* widgets, int count);
int  widget_handle_mouse_all(widget_t* widgets, int count, int px, int py, int pressed);

#endif /* WIDGET_H */
