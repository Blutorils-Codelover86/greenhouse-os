/* ==============================================================================
 * Greenhouse OS — Modern Liquid-Glass 2D Renderer (Implementation)
 * ==============================================================================
 */

#include "renderer.h"

/* Fast integer alpha blend: result = (src * alpha + dst * (255 - alpha)) / 255 */
uint32_t renderer_alpha_blend(uint32_t src, uint32_t dst, uint8_t alpha) {
    if (alpha == 255) return src | 0xFF000000;
    if (alpha == 0) return dst;

    uint32_t inv_a = 255 - alpha;

    uint32_t s_r = (src >> 16) & 0xFF;
    uint32_t s_g = (src >> 8) & 0xFF;
    uint32_t s_b = src & 0xFF;

    uint32_t d_r = (dst >> 16) & 0xFF;
    uint32_t d_g = (dst >> 8) & 0xFF;
    uint32_t d_b = dst & 0xFF;

    uint32_t out_r = (s_r * alpha + d_r * inv_a) / 255;
    uint32_t out_g = (s_g * alpha + d_g * inv_a) / 255;
    uint32_t out_b = (s_b * alpha + d_b * inv_a) / 255;

    return (0xFF000000) | (out_r << 16) | (out_g << 8) | out_b;
}

void renderer_blend_pixel(int x, int y, uint32_t color, uint8_t alpha) {
    if (!graphics_is_active()) return;
    int sw = graphics_get_width(), sh = graphics_get_height();
    if (x < 0 || x >= sw || y < 0 || y >= sh) return;

    uint32_t current = 0;
    if (graphics_get_pixel(x, y, &current) == 0) {
        uint32_t blended = renderer_alpha_blend(color, current, alpha);
        graphics_put_pixel(x, y, blended);
    }
}

void renderer_fill_alpha_rect(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    if (w <= 0 || h <= 0 || !graphics_is_active()) return;
    int sw = graphics_get_width(), sh = graphics_get_height();

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > sw ? sw : (x + w);
    int y1 = (y + h) > sh ? sh : (y + h);

    if (x0 >= x1 || y0 >= y1) return;

    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            uint32_t cur = 0;
            if (graphics_get_pixel(px, py, &cur) == 0) {
                graphics_put_pixel(px, py, renderer_alpha_blend(color, cur, alpha));
            }
        }
    }
}

void renderer_fill_alpha_rounded_rect(int x, int y, int w, int h, int radius, uint32_t color, uint8_t alpha) {
    if (w <= 0 || h <= 0 || !graphics_is_active()) return;
    if (radius <= 0) {
        renderer_fill_alpha_rect(x, y, w, h, color, alpha);
        return;
    }

    int max_r = (w < h ? w : h) / 2;
    if (radius > max_r) radius = max_r;

    int r2 = radius * radius;
    int sw = graphics_get_width(), sh = graphics_get_height();

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > sw ? sw : (x + w);
    int y1 = (y + h) > sh ? sh : (y + h);

    for (int py = y0; py < y1; py++) {
        int dy = 0;
        if (py < y + radius) dy = (y + radius) - py;
        else if (py >= y + h - radius) dy = py - (y + h - radius - 1);

        for (int px = x0; px < x1; px++) {
            int dx = 0;
            if (px < x + radius) dx = (x + radius) - px;
            else if (px >= x + w - radius) dx = px - (x + w - radius - 1);

            if (dx > 0 && dy > 0 && (dx * dx + dy * dy > r2)) {
                continue; /* Outside rounded corner */
            }

            uint32_t cur = 0;
            if (graphics_get_pixel(px, py, &cur) == 0) {
                graphics_put_pixel(px, py, renderer_alpha_blend(color, cur, alpha));
            }
        }
    }
}

void renderer_draw_drop_shadow(int x, int y, int w, int h, int radius, int shadow_size, uint8_t max_alpha) {
    if (shadow_size <= 0 || !graphics_is_active()) return;
    if (max_alpha == 0) max_alpha = 48;

    for (int i = 0; i < shadow_size; i++) {
        uint8_t a = (uint8_t)((max_alpha * (shadow_size - i)) / (shadow_size * 2));
        int sx = x - (i + 1) + 2;
        int sy = y + 2;
        int sw = w + (i + 1) * 2;
        int sh = h + (i + 1) + 2;
        renderer_fill_alpha_rounded_rect(sx, sy, sw, sh, radius + i, 0x00000000, a);
    }
}

void renderer_draw_glass_panel(int x, int y, int w, int h, int radius,
                               uint32_t body_color, uint8_t body_alpha,
                               uint32_t border_color, int shadow_depth) {
    if (w <= 0 || h <= 0 || !graphics_is_active()) return;

    /* 1. Soft ambient drop shadow */
    if (shadow_depth > 0) {
        renderer_draw_drop_shadow(x, y, w, h, radius, shadow_depth, 40);
    }

    /* 2. Glass Body */
    renderer_fill_alpha_rounded_rect(x, y, w, h, radius, body_color, body_alpha);

    /* 3. Subtle top specular glint (1px translucent white highlight) */
    if (w > (radius * 2) + 4) {
        int glint_x = x + radius;
        int glint_w = w - radius * 2;
        renderer_fill_alpha_rect(glint_x, y + 1, glint_w, 1, 0xFFFFFFFF, 45);
    }

    /* 4. Refined Glass Border */
    if (border_color != 0) {
        graphics_draw_rounded_rect(x, y, w, h, radius, border_color);
    }
}

void renderer_draw_pill_button(int x, int y, int w, int h, const char* label,
                               uint32_t bg_color, uint32_t border_color, uint32_t text_color, int is_active) {
    int radius = h / 2;
    uint8_t alpha = is_active ? 230 : 160;

    renderer_fill_alpha_rounded_rect(x, y, w, h, radius, bg_color, alpha);
    graphics_draw_rounded_rect(x, y, w, h, radius, border_color);

    if (is_active) {
        renderer_fill_alpha_rect(x + radius, y + 1, w - radius * 2, 1, 0xFFFFFFFF, 60);
    }

    if (label && label[0]) {
        int tw = font_text_width(label);
        int tx = x + (w - tw) / 2;
        int ty = y + (h - font_glyph_height()) / 2;
        draw_text(tx, ty, label, text_color, FONT_TRANSPARENT, 0);
    }
}

void renderer_draw_badge(int x, int y, const char* text, uint32_t bg_color, uint32_t text_color) {
    if (!text || !text[0]) return;
    int tw = font_text_width(text);
    int pad_x = 6;
    int pad_y = 2;
    int bw = tw + pad_x * 2;
    int bh = font_glyph_height() + pad_y * 2;
    int radius = bh / 2;

    renderer_fill_alpha_rounded_rect(x, y, bw, bh, radius, bg_color, 220);
    draw_text(x + pad_x, y + pad_y, text, text_color, FONT_TRANSPARENT, 0);
}

void renderer_draw_meter(int x, int y, int w, int h, int percent,
                         uint32_t fill_color, uint32_t bg_color, uint32_t border_color) {
    if (w <= 0 || h <= 0) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    int radius = h / 2;
    renderer_fill_alpha_rounded_rect(x, y, w, h, radius, bg_color, 180);
    graphics_draw_rounded_rect(x, y, w, h, radius, border_color);

    int fill_w = (w * percent) / 100;
    if (fill_w > 2) {
        int r_fill = radius;
        if (fill_w < r_fill * 2) r_fill = fill_w / 2;
        renderer_fill_alpha_rounded_rect(x, y, fill_w, h, r_fill, fill_color, 240);
        /* Subtle specular top line on meter */
        renderer_fill_alpha_rect(x + r_fill, y + 1, (fill_w > r_fill * 2) ? (fill_w - r_fill * 2) : 1, 1, 0xFFFFFFFF, 80);
    }
}

void renderer_draw_card(int x, int y, int w, int h, int radius,
                        const char* header, uint32_t header_color,
                        uint32_t bg_color, uint32_t border_color) {
    renderer_draw_glass_panel(x, y, w, h, radius, bg_color, 200, border_color, 2);

    if (header && header[0]) {
        draw_text(x + 12, y + 10, header, header_color, FONT_TRANSPARENT, 0);
        /* Separator hairline */
        renderer_fill_alpha_rect(x + 8, y + 28, w - 16, 1, border_color, 120);
    }
}
