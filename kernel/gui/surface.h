/* ==============================================================================
 * Greenhouse OS - VERDANT Surface System
 *
 * A surface is the primary visual and spatial unit in Verdant. Surfaces have
 * position, geometry, z-order, focus state, morphing targets, styling, and
 * callbacks for drawing and event dispatch.
 * ==============================================================================
 */

#ifndef SURFACE_H
#define SURFACE_H

#include <stdint.h>
#include "../input/input.h"

#define SURFACE_MAX            16
#define SURFACE_TITLE_LEN      48
#define SURFACE_TAG_LEN        12

#define SURFACE_FLAG_RESIZABLE 0x01
#define SURFACE_FLAG_NO_DECOR  0x02
#define SURFACE_FLAG_MODAL     0x04
#define SURFACE_FLAG_PINNED    0x08

typedef uint32_t surface_id_t;
#define SURFACE_ID_NONE 0

typedef struct surface surface_t;

/* Client area callback: draw inside the (already clipped) client rectangle. */
typedef void (*surface_draw_fn)(surface_t* s, int cx, int cy, int cw, int ch);

/* Event callback: return 1 when the event was consumed. */
typedef int  (*surface_event_fn)(surface_t* s, const input_event_t* ev);

/* Close callback */
typedef void (*surface_close_fn)(surface_t* s);

struct surface {
    surface_id_t     id;
    char             title[SURFACE_TITLE_LEN];
    char             tag[SURFACE_TAG_LEN];      /* e.g. "TERM", "FILES", "BERRY" */
    int              x, y, w, h;
    int              target_x, target_y, target_w, target_h;
    int              min_w, min_h;
    int              visible;
    int              is_minimized;
    int              is_maximized;
    int              saved_x, saved_y, saved_w, saved_h;
    int              flags;
    int              z_order;
    uint8_t          opacity;                   /* 0..255 */

    uint32_t         bg_color;
    uint32_t         fg_color;
    uint32_t         border_color;
    uint32_t         title_bg;
    uint32_t         title_fg;
    uint32_t         accent;
    uint32_t         client_color;

    void*            user_data;
    surface_draw_fn  on_draw;
    surface_event_fn on_event;
    surface_close_fn on_close;
};


void surface_init(surface_t* s, surface_id_t id, const char* title, const char* tag,
                  int x, int y, int w, int h, uint32_t accent);
void surface_set_title(surface_t* s, const char* title);
void surface_set_tag(surface_t* s, const char* tag);
void surface_set_colors(surface_t* s, uint32_t bg, uint32_t fg, uint32_t border);
void surface_set_client_color(surface_t* s, uint32_t color);
int  surface_includes(const surface_t* s, int px, int py);
void surface_step_animation(surface_t* s);
void surface_maximize(surface_t* s, int max_w, int max_h);
void surface_restore(surface_t* s);

#endif /* SURFACE_H */
