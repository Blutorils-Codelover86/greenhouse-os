/* ==============================================================================
 * Greenhouse OS — Modern Liquid-Glass Compositor
 * ==============================================================================
 */

#ifndef GUI_COMPOSITOR_H
#define GUI_COMPOSITOR_H

#include <stdint.h>
#include <stddef.h>
#include "renderer.h"

#define COMPOSITOR_TITLE_HEIGHT   32
#define COMPOSITOR_BORDER_SIZE    1
#define COMPOSITOR_CORNER_RADIUS  10

/* Window Control Zone Hit Testing */
#define COMPOSITOR_ZONE_NONE      0
#define COMPOSITOR_ZONE_TITLE     1
#define COMPOSITOR_ZONE_CLOSE     2
#define COMPOSITOR_ZONE_MINIMIZE  3
#define COMPOSITOR_ZONE_MAXIMIZE  4
#define COMPOSITOR_ZONE_RESIZE    5
#define COMPOSITOR_ZONE_CLIENT    6

void compositor_init(void);

/* Render calm atmospheric organic wallpaper */
void compositor_draw_wallpaper(int width, int height);

/* Render modern glass window frame & decorations */
void compositor_draw_surface_frame(int x, int y, int w, int h,
                                   const char* title, const char* subtitle,
                                   int is_active, int is_maximized);

/* Hit test window control regions */
int  compositor_hit_test(int win_x, int win_y, int win_w, int win_h, int click_x, int click_y);

/* Calculate client rectangle for a surface */
struct surface;
void compositor_client_rect(const struct surface* s, int* cx, int* cy, int* cw, int* ch);

#endif /* GUI_COMPOSITOR_H */
