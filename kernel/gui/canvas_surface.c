/* ==============================================================================
 * Greenhouse OS - Light-Mode Canvas / Graphics Demo Surface (implementation)
 * ==============================================================================
 */

#include "canvas_surface.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "gh_theme.h"
#include "../irq.h"

void canvas_surface_init(void) {
}

void canvas_surface_draw(surface_t* s, int cx, int cy, int cw, int ch) {
    (void)s;

    /* 1. Header */
    graphics_fill_gradient_h(cx + 8, cy + 8, cw - 16, 26, GH_COLOR_GREEN_LEAF_SOFT, GH_COLOR_GREEN_LEAF_DEEP);
    graphics_draw_rounded_rect(cx + 8, cy + 8, cw - 16, 26, 4, GH_COLOR_GREEN_LEAF);
    draw_text(cx + 14, cy + 13, "[*] 2D GRAPHICS & HARDWARE RENDERER", GH_COLOR_WHITE, FONT_TRANSPARENT, 1);

    /* 2. Concentric Circles / Radar */
    int radar_cx = cx + 64;
    int radar_cy = cy + 90;
    for (int r = 10; r <= 45; r += 10) {
        graphics_draw_circle(radar_cx, radar_cy, r, GH_COLOR_BORDER_LIGHT);
    }
    graphics_draw_circle(radar_cx, radar_cy, 45, GH_COLOR_GREEN_LEAF);
    graphics_fill_circle(radar_cx, radar_cy, 4, GH_COLOR_GREEN_LEAF_DEEP);

    /* Rotating radar line */
    uint64_t t = timer_get_ticks();
    int angle_idx = (int)((t / 2) % 16);
    static const int cos_table[16] = { 40, 36, 28, 15, 0, -15, -28, -36, -40, -36, -28, -15, 0, 15, 28, 36 };
    static const int sin_table[16] = { 0, 15, 28, 36, 40, 36, 28, 15, 0, -15, -28, -36, -40, -36, -28, -15 };
    graphics_draw_line(radar_cx, radar_cy, radar_cx + cos_table[angle_idx], radar_cy + sin_table[angle_idx], GH_COLOR_GREEN_LEAF);

    /* 3. Color Gradients and Palette chips */
    int chip_x = cx + 130;
    int chip_y = cy + 45;
    int chip_w = cw - 145;
    if (chip_w > 80) {
        graphics_fill_gradient_h(chip_x, chip_y, chip_w, 20, GH_COLOR_GREEN_LEAF_DEEP, GH_COLOR_BLUE_INFO);
        draw_text(chip_x + 6, chip_y + 3, "Emerald -> Sky Gradient", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);

        chip_y += 28;
        graphics_fill_gradient_h(chip_x, chip_y, chip_w, 20, GH_COLOR_RED_ERROR, GH_COLOR_AMBER_WARN);
        draw_text(chip_x + 6, chip_y + 3, "Berry -> Amber Gradient", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);

        chip_y += 28;
        graphics_fill_rounded_rect(chip_x, chip_y, chip_w, 32, 4, GH_COLOR_SURFACE_HOVER);
        graphics_draw_rounded_rect(chip_x, chip_y, chip_w, 32, 4, GH_COLOR_BORDER_LIGHT);
        draw_text(chip_x + 8, chip_y + 8, "Card Panel Primitive", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 1);
    }

    /* 4. Geometry and Stats at bottom */
    int by = cy + ch - 24;
    draw_text(cx + 12, by, "Software Back-Buffer * 32-bpp TrueColor * Hardware Clamped", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
}

int canvas_surface_event(surface_t* s, const input_event_t* ev) {
    (void)s; (void)ev;
    return 0;
}