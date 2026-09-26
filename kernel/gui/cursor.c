/* ==============================================================================
 * Greenhouse OS - Phase 7: Software Cursor (implementation)
 * ==============================================================================
 */

#include "cursor.h"
#include "../graphics/graphics.h"

/* Bitmaps are 16 rows of 16 bits.  MSB of each row is the leftmost pixel. */

static const uint16_t cursor_arrow_opaque[16] = {
    0x8000, 0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00,
    0xFF80, 0xFF80, 0xFF00, 0xFE00, 0xF000, 0xE000, 0xC000, 0x8000
};
static const uint16_t cursor_arrow_transparent[16] = {
    0x0000, 0x1000, 0x3800, 0x7800, 0x7C00, 0x7E00, 0x7F00, 0x7F80,
    0x7F80, 0x7F00, 0x7E00, 0x7800, 0x7000, 0x3800, 0x1000, 0x0000
};

static const uint16_t cursor_hand_opaque[16] = {
    0x0300, 0x0F00, 0x3F00, 0x7F00, 0x7F80, 0x7F80, 0x7F80, 0x7F80,
    0x7F80, 0x7F80, 0x7F80, 0x7F80, 0x3FC0, 0x3FC0, 0x0F00, 0x0000
};
static const uint16_t cursor_hand_transparent[16] = {
    0x0000, 0x0000, 0x0080, 0x01C0, 0x0180, 0x0180, 0x0180, 0x0180,
    0x0180, 0x0180, 0x0180, 0x0180, 0x0000, 0x0000, 0x0000, 0x0000
};

static const uint16_t cursor_resize_h_opaque[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FF8,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
static const uint16_t cursor_resize_h_transparent[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x07F8,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static const uint16_t cursor_resize_v_opaque[16] = {
    0x0000, 0x0180, 0x03C0, 0x07E0, 0x0FF0, 0x0180, 0x0180, 0x0180,
    0x0180, 0x0180, 0x0180, 0x0FF0, 0x07E0, 0x03C0, 0x0180, 0x0000
};
static const uint16_t cursor_resize_v_transparent[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0180, 0x0180, 0x0180,
    0x0180, 0x0180, 0x0180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static const uint16_t cursor_wait_opaque[16] = {
    0x0FF0, 0x1860, 0x1C60, 0x1C60, 0x1C60, 0x1C60, 0x18E0, 0x0FF0,
    0x0FF0, 0x0E78, 0x0C78, 0x0C78, 0x0C78, 0x0C78, 0x0E78, 0x0FF0
};
static const uint16_t cursor_wait_transparent[16] = {
    0x0000, 0x0FF0, 0x1998, 0x1998, 0x1998, 0x1998, 0x1998, 0x0FF0,
    0x0FF0, 0x0FF0, 0x0998, 0x0998, 0x0998, 0x0998, 0x0FF0, 0x0FF0
};

static const uint16_t* cursor_opaque[CURSOR_COUNT] = {
    cursor_arrow_opaque, cursor_hand_opaque, cursor_resize_h_opaque,
    cursor_resize_v_opaque, cursor_wait_opaque
};
static const uint16_t* cursor_transparent[CURSOR_COUNT] = {
    cursor_arrow_transparent, cursor_hand_transparent, cursor_resize_h_transparent,
    cursor_resize_v_transparent, cursor_wait_transparent
};
static const int cursor_hot_x[CURSOR_COUNT] = { 0, 4, 7, 7, 7 };
static const int cursor_hot_y[CURSOR_COUNT] = { 0, 0, 7, 7, 7 };

static int cursor_shape = CURSOR_ARROW;
static int cursor_visible = 1;
static int cursor_hotspot_x = 0;
static int cursor_hotspot_y = 0;

void cursor_init(void) {
    cursor_shape = CURSOR_ARROW;
    cursor_visible = 1;
    cursor_hotspot_x = cursor_hot_x[CURSOR_ARROW];
    cursor_hotspot_y = cursor_hot_y[CURSOR_ARROW];
}

void cursor_set_shape(int shape) {
    if (shape < 0 || shape >= CURSOR_COUNT) return;
    if (shape == cursor_shape) return;
    cursor_shape = shape;
    cursor_hotspot_x = cursor_hot_x[shape];
    cursor_hotspot_y = cursor_hot_y[shape];
}

int  cursor_get_shape(void)    { return cursor_shape; }
void cursor_set_visible(int visible) { cursor_visible = visible ? 1 : 0; }
int  cursor_is_visible(void)   { return cursor_visible; }
void cursor_set_hotspot(int x, int y) { cursor_hotspot_x = x; cursor_hotspot_y = y; }

int cursor_get_width(void)  { return 16; }
int cursor_get_height(void) { return 16; }

const char* cursor_shape_name(int shape) {
    switch (shape) {
        case CURSOR_ARROW:    return "arrow";
        case CURSOR_HAND:     return "hand";
        case CURSOR_RESIZE_H: return "resize-h";
        case CURSOR_RESIZE_V: return "resize-v";
        case CURSOR_WAIT:     return "wait";
        default:              return "unknown";
    }
}

void cursor_draw(int x, int y) {
    if (!cursor_visible || !graphics_is_active()) return;

    x -= cursor_hotspot_x;
    y -= cursor_hotspot_y;

    const uint16_t* opaque = cursor_opaque[cursor_shape];
    const uint16_t* clear = cursor_transparent[cursor_shape];

    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            uint16_t bit = (uint16_t)(1 << (15 - col));
            uint32_t color;
            if (opaque[row] & bit)      color = 0xFFFFFFFF;   /* white core */
            else if (clear[row] & bit)  color = 0xFF101828;   /* dark outline */
            else                         continue;
            graphics_put_pixel(x + col, y + row, color);
        }
    }
}
