/* ==============================================================================
 * Greenhouse OS - VERDANT Canvas / Graphics Demo Surface (implementation)
 * ==============================================================================
 */

#include "canvas_surface.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "../irq.h"

void canvas_surface_init(void) {
}

void canvas_surface_draw(surface_t* s, int cx, int cy, int cw, int ch) {
    (void)s;

    /* 1. Header */
    graphics_fill_gradient_h(cx + 8, cy + 8, cw - 16, 26, 0xFF2A1238, 0xFF140822);
    graphics_draw_rounded_rect(cx + 8, cy + 8, cw - 16, 26, 4, 0xFFA855F7);
    draw_text(cx + 14, cy + 13, "[*] 2D GRAPHICS & HARDWARE RENDERER", 0xFFD8B4FE, FONT_TRANSPARENT, 1);

    /* 2. Concentric Circles / Radar */
    int radar_cx = cx + 64;
    int radar_cy = cy + 90;
    for (int r = 10; r <= 45; r += 10) {
        graphics_draw_circle(radar_cx, radar_cy, r, 0xFF1E3A5F);
    }
    graphics_draw_circle(radar_cx, radar_cy, 45, 0xFFA855F7);
    graphics_fill_circle(radar_cx, radar_cy, 4, 0xFF10B981);

    /* Rotating radar line */
    uint64_t t = timer_get_ticks();
    int angle_idx = (int)((t / 2) % 16);
    static const int cos_table[16] = { 40, 36, 28, 15, 0, -15, -28, -36, -40, -36, -28, -15, 0, 15, 28, 36 };
    static const int sin_table[16] = { 0, 15, 28, 36, 40, 36, 28, 15, 0, -15, -28, -36, -40, -36, -28, -15 };
    graphics_draw_line(radar_cx, radar_cy, radar_cx + cos_table[angle_idx], radar_cy + sin_table[angle_idx], 0xFF34D399);

    /* 3. Color Gradients and Palette chips */
    int chip_x = cx + 130;
    int chip_y = cy + 45;
    int chip_w = cw - 145;
    if (chip_w > 80) {
        graphics_fill_gradient_h(chip_x, chip_y, chip_w, 20, 0xFF10B981, 0xFF38BDF8);
        draw_text(chip_x + 6, chip_y + 3, "Emerald -> Sky Gradient", 0xFF04101A, FONT_TRANSPARENT, 0);

        chip_y += 28;
        graphics_fill_gradient_h(chip_x, chip_y, chip_w, 20, 0xFFF43F5E, 0xFFFBBF24);
        draw_text(chip_x + 6, chip_y + 3, "Berry -> Amber Gradient", 0xFF140810, FONT_TRANSPARENT, 0);

        chip_y += 28;
        graphics_fill_glass_panel(chip_x, chip_y, chip_w, 32, 6, 0xFF142438, 0xFF0B1420, 0xFF10B981, 0xFF38BDF8);
        draw_text(chip_x + 8, chip_y + 8, "Liquid-Glass Panel Primitive", 0xFFE2E8F0, FONT_TRANSPARENT, 1);
    }

    /* 4. Geometry and Stats at bottom */
    int by = cy + ch - 24;
    draw_text(cx + 12, by, "Software Back-Buffer * 32-bpp TrueColor * Hardware Clamped", 0xFF64748B, FONT_TRANSPARENT, 0);
}

int canvas_surface_event(surface_t* s, const input_event_t* ev) {
    (void)s; (void)ev;
    return 0;
}
