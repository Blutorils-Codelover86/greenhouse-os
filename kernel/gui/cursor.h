/* ==============================================================================
 * Greenhouse OS — Modern Dedicated Cursor System
 * ==============================================================================
 * Centralized, antialiased, modern cursor family:
 *   - Crisp arrow with correct hotspot
 *   - Antialiased edges with subtle ambient drop shadows
 *   - High-DPI safe scaling support
 *   - Complete cursor family: default, pointer, text, resize-h, resize-v,
 *     resize-diagonal, busy (animated botanical spinner)
 * ==============================================================================
 */

#ifndef CURSOR_H
#define CURSOR_H

#include <stdint.h>

typedef enum {
    CURSOR_DEFAULT = 0,
    CURSOR_POINTER,
    CURSOR_TEXT,
    CURSOR_RESIZE_H,
    CURSOR_RESIZE_V,
    CURSOR_RESIZE_DIAG,
    CURSOR_BUSY,
    CURSOR_TYPE_COUNT
} cursor_type_t;

/* Backward compatibility aliases */
#define CURSOR_ARROW        CURSOR_DEFAULT
#define CURSOR_HAND         CURSOR_POINTER
#define CURSOR_WAIT         CURSOR_BUSY
#define CURSOR_COUNT        CURSOR_TYPE_COUNT

void cursor_init(void);
void cursor_set_shape(int shape);
int  cursor_get_shape(void);
void cursor_set_visible(int visible);
int  cursor_is_visible(void);
void cursor_set_scale(int scale);
int  cursor_get_scale(void);
void cursor_set_hotspot(int x, int y);
void cursor_get_hotspot(int shape, int* hx, int* hy);
void cursor_draw(int x, int y);
void cursor_tick(void); /* advance spinner animation */
int  cursor_get_width(void);
int  cursor_get_height(void);
const char* cursor_shape_name(int shape);

#endif /* CURSOR_H */
