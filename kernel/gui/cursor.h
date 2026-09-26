/* ==============================================================================
 * Greenhouse OS - Phase 7: Software Cursor
 *
 * The kernel has no hardware cursor in graphics mode, so the pointer is drawn
 * by the compositor from small bitmaps.  Each shape is two bitmaps: opaque
 * pixels and transparent pixels, so the cursor composites correctly over any
 * window content.
 * ==============================================================================
 */

#ifndef CURSOR_H
#define CURSOR_H

#include <stdint.h>

#define CURSOR_ARROW  0
#define CURSOR_HAND   1
#define CURSOR_RESIZE_H 2
#define CURSOR_RESIZE_V 3
#define CURSOR_WAIT   4
#define CURSOR_COUNT  5

void cursor_init(void);
void cursor_set_shape(int shape);
int  cursor_get_shape(void);
void cursor_set_visible(int visible);
int  cursor_is_visible(void);
void cursor_set_hotspot(int x, int y);
void cursor_draw(int x, int y);
int  cursor_get_width(void);
int  cursor_get_height(void);
const char* cursor_shape_name(int shape);

#endif /* CURSOR_H */
