/* ==============================================================================
 * Greenhouse OS — Modern Light-Mode 2D Renderer (Implementation)
 * ==============================================================================
 */

#include "renderer.h"
#include "../heap.h"

/* ------------------------------------------------------------------------------
 * Fast Integer Math & Helpers
 * -------------------------------------------------------------------------- */

static inline int fast_isqrt(int val) {
    if (val <= 0) return 0;
    int x = val;
    int y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + val / x) / 2;
    }
    return x;
}

static inline int int_min(int a, int b) { return (a < b) ? a : b; }
static inline int int_max(int a, int b) { return (a > b) ? a : b; }
static inline int int_clamp(int val, int low, int high) {
    if (val < low) return low;
    if (val > high) return high;
    return val;
}

/* ------------------------------------------------------------------------------
 * Render Target & Clipping Management
 * -------------------------------------------------------------------------- */

static renderer_surface_t* g_render_target = NULL;

#define CLIP_STACK_MAX 16
static struct { int x, y, w, h; } g_clip_stack[CLIP_STACK_MAX];
static int g_clip_depth = 0;
static int g_has_custom_clip = 0;
static int g_cur_clip_x = 0, g_cur_clip_y = 0, g_cur_clip_w = 0, g_cur_clip_h = 0;

static void renderer_get_bounds(int* w, int* h) {
    if (g_render_target) {
        *w = g_render_target->width;
        *h = g_render_target->height;
    } else if (graphics_is_active()) {
        *w = graphics_get_width();
        *h = graphics_get_height();
    } else {
        *w = 0;
        *h = 0;
    }
}

void renderer_clear_clip(void) {
    int bw = 0, bh = 0;
    renderer_get_bounds(&bw, &bh);
    g_cur_clip_x = 0;
    g_cur_clip_y = 0;
    g_cur_clip_w = bw;
    g_cur_clip_h = bh;
    g_has_custom_clip = 0;
    if (!g_render_target && graphics_is_active()) {
        graphics_clear_clip();
    }
}

void renderer_set_clip(int x, int y, int w, int h) {
    int bw = 0, bh = 0;
    renderer_get_bounds(&bw, &bh);

    int x0 = int_max(0, x);
    int y0 = int_max(0, y);
    int x1 = int_min(bw, x + w);
    int y1 = int_min(bh, y + h);

    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;

    g_cur_clip_x = x0;
    g_cur_clip_y = y0;
    g_cur_clip_w = x1 - x0;
    g_cur_clip_h = y1 - y0;
    g_has_custom_clip = 1;

    if (!g_render_target && graphics_is_active()) {
        graphics_set_clip(g_cur_clip_x, g_cur_clip_y, g_cur_clip_w, g_cur_clip_h);
    }
}

void renderer_push_clip(int x, int y, int w, int h) {
    if (g_clip_depth < CLIP_STACK_MAX) {
        if (!g_has_custom_clip) {
            int bw = 0, bh = 0;
            renderer_get_bounds(&bw, &bh);
            g_clip_stack[g_clip_depth].x = 0;
            g_clip_stack[g_clip_depth].y = 0;
            g_clip_stack[g_clip_depth].w = bw;
            g_clip_stack[g_clip_depth].h = bh;
        } else {
            g_clip_stack[g_clip_depth].x = g_cur_clip_x;
            g_clip_stack[g_clip_depth].y = g_cur_clip_y;
            g_clip_stack[g_clip_depth].w = g_cur_clip_w;
            g_clip_stack[g_clip_depth].h = g_cur_clip_h;
        }
        g_clip_depth++;
    }

    /* Intersect new rect with current clip */
    int x0 = int_max(g_cur_clip_x, x);
    int y0 = int_max(g_cur_clip_y, y);
    int x1 = int_min(g_cur_clip_x + g_cur_clip_w, x + w);
    int y1 = int_min(g_cur_clip_y + g_cur_clip_h, y + h);

    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;

    renderer_set_clip(x0, y0, x1 - x0, y1 - y0);
}

void renderer_pop_clip(void) {
    if (g_clip_depth > 0) {
        g_clip_depth--;
        renderer_set_clip(g_clip_stack[g_clip_depth].x,
                          g_clip_stack[g_clip_depth].y,
                          g_clip_stack[g_clip_depth].w,
                          g_clip_stack[g_clip_depth].h);
    } else {
        renderer_clear_clip();
    }
}

int renderer_get_clip(int* x, int* y, int* w, int* h) {
    if (x) *x = g_cur_clip_x;
    if (y) *y = g_cur_clip_y;
    if (w) *w = g_cur_clip_w;
    if (h) *h = g_cur_clip_h;
    return (g_cur_clip_w > 0 && g_cur_clip_h > 0);
}

/* ------------------------------------------------------------------------------
 * Alpha Compositing
 * -------------------------------------------------------------------------- */

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

uint32_t renderer_blend_over(uint32_t src_with_alpha, uint32_t dst) {
    uint8_t a = (uint8_t)((src_with_alpha >> 24) & 0xFF);
    return renderer_alpha_blend(src_with_alpha, dst, a);
}

void renderer_blend_pixel(int x, int y, uint32_t color, uint8_t alpha) {
    if (alpha == 0) return;

    if (g_has_custom_clip) {
        if (x < g_cur_clip_x || x >= g_cur_clip_x + g_cur_clip_w ||
            y < g_cur_clip_y || y >= g_cur_clip_y + g_cur_clip_h) return;
    }

    if (g_render_target) {
        if (x < 0 || x >= g_render_target->width || y < 0 || y >= g_render_target->height) return;
        uint32_t* row = (uint32_t*)((uint8_t*)g_render_target->pixels + (uint64_t)y * g_render_target->pitch);
        if (alpha == 255) {
            row[x] = color | 0xFF000000;
        } else {
            row[x] = renderer_alpha_blend(color, row[x], alpha);
        }
        return;
    }

    if (!graphics_is_active()) return;
    int sw = graphics_get_width(), sh = graphics_get_height();
    if (x < 0 || x >= sw || y < 0 || y >= sh) return;

    if (graphics_get_bpp() == 32) {
        uint8_t* tgt = (uint8_t*)graphics_get_target();
        if (tgt) {
            uint32_t* row = (uint32_t*)(tgt + (uint64_t)y * graphics_get_pitch());
            if (alpha == 255) {
                row[x] = color | 0xFF000000;
            } else {
                row[x] = renderer_alpha_blend(color, row[x], alpha);
            }
            graphics_damage_rows(y, y);
            return;
        }
    }

    uint32_t current = 0;
    if (graphics_get_pixel(x, y, &current) == 0) {
        uint32_t blended = renderer_alpha_blend(color, current, alpha);
        graphics_put_pixel(x, y, blended);
    }
}

void renderer_draw_line(int x0, int y0, int x1, int y1, uint32_t color, uint8_t alpha) {
    if (alpha == 0) return;
    int dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 >= y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        renderer_blend_pixel(x0, y0, color, alpha);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* ------------------------------------------------------------------------------
 * Rectangle Drawing Primitives
 * -------------------------------------------------------------------------- */

void renderer_fill_alpha_rect(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    if (w <= 0 || h <= 0 || alpha == 0) return;

    int bw = 0, bh = 0;
    renderer_get_bounds(&bw, &bh);

    int clip_x = g_has_custom_clip ? g_cur_clip_x : 0;
    int clip_y = g_has_custom_clip ? g_cur_clip_y : 0;
    int clip_r = g_has_custom_clip ? (g_cur_clip_x + g_cur_clip_w) : bw;
    int clip_b = g_has_custom_clip ? (g_cur_clip_y + g_cur_clip_h) : bh;

    int x0 = int_max(clip_x, x);
    int y0 = int_max(clip_y, y);
    int x1 = int_min(clip_r, x + w);
    int y1 = int_min(clip_b, y + h);

    if (x0 >= x1 || y0 >= y1) return;

    if (alpha == 255) {
        if (g_render_target) {
            for (int py = y0; py < y1; py++) {
                uint32_t* row = (uint32_t*)((uint8_t*)g_render_target->pixels + (uint64_t)py * g_render_target->pitch);
                for (int px = x0; px < x1; px++) row[px] = color | 0xFF000000;
            }
        } else {
            graphics_fill_rect(x0, y0, x1 - x0, y1 - y0, color);
        }
        return;
    }

    if (g_render_target) {
        for (int py = y0; py < y1; py++) {
            uint32_t* row = (uint32_t*)((uint8_t*)g_render_target->pixels + (uint64_t)py * g_render_target->pitch);
            for (int px = x0; px < x1; px++) {
                row[px] = renderer_alpha_blend(color, row[px], alpha);
            }
        }
        return;
    }

    if (graphics_is_active() && graphics_get_bpp() == 32) {
        uint8_t* tgt = (uint8_t*)graphics_get_target();
        if (tgt) {
            uint32_t pitch = graphics_get_pitch();
            for (int py = y0; py < y1; py++) {
                uint32_t* row = (uint32_t*)(tgt + (uint64_t)py * pitch);
                for (int px = x0; px < x1; px++) {
                    row[px] = renderer_alpha_blend(color, row[px], alpha);
                }
            }
            graphics_damage_rows(y0, y1 - 1);
            return;
        }
    }

    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            renderer_blend_pixel(px, py, color, alpha);
        }
    }
}

void renderer_fill_alpha_rounded_rect(int x, int y, int w, int h, int radius, uint32_t color, uint8_t alpha) {
    renderer_fill_aa_rounded_rect(x, y, w, h, radius, color, alpha);
}

/* ------------------------------------------------------------------------------
 * Antialiased Rounded Rectangles & Circles (Subpixel Coverage)
 * -------------------------------------------------------------------------- */

void renderer_fill_aa_rounded_rect(int x, int y, int w, int h, int radius, uint32_t color, uint8_t alpha) {
    if (w <= 0 || h <= 0 || alpha == 0) return;
    if (radius <= 0) {
        renderer_fill_alpha_rect(x, y, w, h, color, alpha);
        return;
    }

    int max_r = int_min(w, h) / 2;
    if (radius > max_r) radius = max_r;

    int bw = 0, bh = 0;
    renderer_get_bounds(&bw, &bh);

    int clip_x = g_has_custom_clip ? g_cur_clip_x : 0;
    int clip_y = g_has_custom_clip ? g_cur_clip_y : 0;
    int clip_r = g_has_custom_clip ? (g_cur_clip_x + g_cur_clip_w) : bw;
    int clip_b = g_has_custom_clip ? (g_cur_clip_y + g_cur_clip_h) : bh;

    int x0 = int_max(clip_x, x);
    int y0 = int_max(clip_y, y);
    int x1 = int_min(clip_r, x + w);
    int y1 = int_min(clip_b, y + h);

    if (x0 >= x1 || y0 >= y1) return;

    /* 1. Fast Rectangular Fill for the Central Body (y + radius to y + h - radius) */
    int mid_y0 = int_max(y0, y + radius);
    int mid_y1 = int_min(y1, y + h - radius);
    if (mid_y0 < mid_y1) {
        renderer_fill_alpha_rect(x0, mid_y0, x1 - x0, mid_y1 - mid_y0, color, alpha);
    }

    int r_scaled = radius * 256;

    /* 2. Top Band: py from y0 to min(y1, y + radius) */
    int top_y1 = int_min(y1, y + radius);
    for (int py = y0; py < top_y1; py++) {
        int dy = (y + radius) - py;

        /* Left top corner */
        int left_x1 = int_min(x1, x + radius);
        for (int px = x0; px < left_x1; px++) {
            int dx = (x + radius) - px;
            int dist2 = dx * dx + dy * dy;
            int d_scaled = fast_isqrt(dist2 * 65536);
            int diff = r_scaled - d_scaled;
            if (diff <= -128) continue;
            uint8_t pix_alpha = alpha;
            if (diff < 128) {
                int cov = diff + 128;
                pix_alpha = (uint8_t)((alpha * cov) / 255);
            }
            renderer_blend_pixel(px, py, color, pix_alpha);
        }

        /* Center top span: fast fill */
        int center_x0 = int_max(x0, x + radius);
        int center_x1 = int_min(x1, x + w - radius);
        if (center_x0 < center_x1) {
            renderer_fill_alpha_rect(center_x0, py, center_x1 - center_x0, 1, color, alpha);
        }

        /* Right top corner */
        int right_x0 = int_max(x0, x + w - radius);
        for (int px = right_x0; px < x1; px++) {
            int dx = px - (x + w - radius - 1);
            int dist2 = dx * dx + dy * dy;
            int d_scaled = fast_isqrt(dist2 * 65536);
            int diff = r_scaled - d_scaled;
            if (diff <= -128) continue;
            uint8_t pix_alpha = alpha;
            if (diff < 128) {
                int cov = diff + 128;
                pix_alpha = (uint8_t)((alpha * cov) / 255);
            }
            renderer_blend_pixel(px, py, color, pix_alpha);
        }
    }

    /* 3. Bottom Band: py from max(y0, y + h - radius) to y1 */
    int bot_y0 = int_max(y0, y + h - radius);
    for (int py = bot_y0; py < y1; py++) {
        int dy = py - (y + h - radius - 1);

        /* Left bottom corner */
        int left_x1 = int_min(x1, x + radius);
        for (int px = x0; px < left_x1; px++) {
            int dx = (x + radius) - px;
            int dist2 = dx * dx + dy * dy;
            int d_scaled = fast_isqrt(dist2 * 65536);
            int diff = r_scaled - d_scaled;
            if (diff <= -128) continue;
            uint8_t pix_alpha = alpha;
            if (diff < 128) {
                int cov = diff + 128;
                pix_alpha = (uint8_t)((alpha * cov) / 255);
            }
            renderer_blend_pixel(px, py, color, pix_alpha);
        }

        /* Center bottom span: fast fill */
        int center_x0 = int_max(x0, x + radius);
        int center_x1 = int_min(x1, x + w - radius);
        if (center_x0 < center_x1) {
            renderer_fill_alpha_rect(center_x0, py, center_x1 - center_x0, 1, color, alpha);
        }

        /* Right bottom corner */
        int right_x0 = int_max(x0, x + w - radius);
        for (int px = right_x0; px < x1; px++) {
            int dx = px - (x + w - radius - 1);
            int dist2 = dx * dx + dy * dy;
            int d_scaled = fast_isqrt(dist2 * 65536);
            int diff = r_scaled - d_scaled;
            if (diff <= -128) continue;
            uint8_t pix_alpha = alpha;
            if (diff < 128) {
                int cov = diff + 128;
                pix_alpha = (uint8_t)((alpha * cov) / 255);
            }
            renderer_blend_pixel(px, py, color, pix_alpha);
        }
    }
}

void renderer_draw_aa_rounded_rect(int x, int y, int w, int h, int radius, int thickness, uint32_t color, uint8_t alpha) {
    if (w <= 0 || h <= 0 || thickness <= 0 || alpha == 0) return;

    int max_r = int_min(w, h) / 2;
    if (radius > max_r) radius = max_r;

    int inner_r = radius - thickness;
    if (inner_r < 0) inner_r = 0;

    int bw = 0, bh = 0;
    renderer_get_bounds(&bw, &bh);

    int clip_x = g_has_custom_clip ? g_cur_clip_x : 0;
    int clip_y = g_has_custom_clip ? g_cur_clip_y : 0;
    int clip_r = g_has_custom_clip ? (g_cur_clip_x + g_cur_clip_w) : bw;
    int clip_b = g_has_custom_clip ? (g_cur_clip_y + g_cur_clip_h) : bh;

    int x0 = int_max(clip_x, x);
    int y0 = int_max(clip_y, y);
    int x1 = int_min(clip_r, x + w);
    int y1 = int_min(clip_b, y + h);

    if (x0 >= x1 || y0 >= y1) return;

    /* 1. Fast Straight Edges */
    /* Top edge straight span */
    int top_x0 = int_max(x0, x + radius);
    int top_x1 = int_min(x1, x + w - radius);
    if (top_x0 < top_x1) {
        int th_top = int_min(thickness, y1 - y0);
        if (y0 <= y && y + th_top <= y1) {
            renderer_fill_alpha_rect(top_x0, y, top_x1 - top_x0, thickness, color, alpha);
        }
    }

    /* Bottom edge straight span */
    if (top_x0 < top_x1) {
        int by = y + h - thickness;
        if (by >= y0 && by + thickness <= y1) {
            renderer_fill_alpha_rect(top_x0, by, top_x1 - top_x0, thickness, color, alpha);
        }
    }

    /* Left edge straight span */
    int mid_y0 = int_max(y0, y + radius);
    int mid_y1 = int_min(y1, y + h - radius);
    if (mid_y0 < mid_y1) {
        if (x0 <= x && x + thickness <= x1) {
            renderer_fill_alpha_rect(x, mid_y0, thickness, mid_y1 - mid_y0, color, alpha);
        }
        int rx = x + w - thickness;
        if (rx >= x0 && rx + thickness <= x1) {
            renderer_fill_alpha_rect(rx, mid_y0, thickness, mid_y1 - mid_y0, color, alpha);
        }
    }

    /* 2. Antialiased Corners */
    int r_out_scaled = radius * 256;
    int r_in_scaled = inner_r * 256;

    /* Top corners */
    int top_y_end = int_min(y1, y + radius);
    for (int py = y0; py < top_y_end; py++) {
        int dy = (y + radius) - py;

        /* Top-left */
        int left_end = int_min(x1, x + radius);
        for (int px = x0; px < left_end; px++) {
            int dx = (x + radius) - px;
            int dist2 = dx * dx + dy * dy;
            int d_scaled = fast_isqrt(dist2 * 65536);
            int diff_out = r_out_scaled - d_scaled;
            int diff_in = r_in_scaled - d_scaled;
            if (diff_out <= -128) continue;
            int cov_out = int_clamp(diff_out + 128, 0, 255);
            int cov_in = int_clamp(diff_in + 128, 0, 255);
            int cov = cov_out - cov_in;
            if (cov > 0) {
                uint8_t pix_alpha = (uint8_t)((alpha * cov) / 255);
                renderer_blend_pixel(px, py, color, pix_alpha);
            }
        }

        /* Top-right */
        int right_start = int_max(x0, x + w - radius);
        for (int px = right_start; px < x1; px++) {
            int dx = px - (x + w - radius - 1);
            int dist2 = dx * dx + dy * dy;
            int d_scaled = fast_isqrt(dist2 * 65536);
            int diff_out = r_out_scaled - d_scaled;
            int diff_in = r_in_scaled - d_scaled;
            if (diff_out <= -128) continue;
            int cov_out = int_clamp(diff_out + 128, 0, 255);
            int cov_in = int_clamp(diff_in + 128, 0, 255);
            int cov = cov_out - cov_in;
            if (cov > 0) {
                uint8_t pix_alpha = (uint8_t)((alpha * cov) / 255);
                renderer_blend_pixel(px, py, color, pix_alpha);
            }
        }
    }

    /* Bottom corners */
    int bot_y_start = int_max(y0, y + h - radius);
    for (int py = bot_y_start; py < y1; py++) {
        int dy = py - (y + h - radius - 1);

        /* Bottom-left */
        int left_end = int_min(x1, x + radius);
        for (int px = x0; px < left_end; px++) {
            int dx = (x + radius) - px;
            int dist2 = dx * dx + dy * dy;
            int d_scaled = fast_isqrt(dist2 * 65536);
            int diff_out = r_out_scaled - d_scaled;
            int diff_in = r_in_scaled - d_scaled;
            if (diff_out <= -128) continue;
            int cov_out = int_clamp(diff_out + 128, 0, 255);
            int cov_in = int_clamp(diff_in + 128, 0, 255);
            int cov = cov_out - cov_in;
            if (cov > 0) {
                uint8_t pix_alpha = (uint8_t)((alpha * cov) / 255);
                renderer_blend_pixel(px, py, color, pix_alpha);
            }
        }

        /* Bottom-right */
        int right_start = int_max(x0, x + w - radius);
        for (int px = right_start; px < x1; px++) {
            int dx = px - (x + w - radius - 1);
            int dist2 = dx * dx + dy * dy;
            int d_scaled = fast_isqrt(dist2 * 65536);
            int diff_out = r_out_scaled - d_scaled;
            int diff_in = r_in_scaled - d_scaled;
            if (diff_out <= -128) continue;
            int cov_out = int_clamp(diff_out + 128, 0, 255);
            int cov_in = int_clamp(diff_in + 128, 0, 255);
            int cov = cov_out - cov_in;
            if (cov > 0) {
                uint8_t pix_alpha = (uint8_t)((alpha * cov) / 255);
                renderer_blend_pixel(px, py, color, pix_alpha);
            }
        }
    }
}

void renderer_fill_aa_circle(int cx, int cy, int radius, uint32_t color, uint8_t alpha) {
    if (radius <= 0 || alpha == 0) return;
    int x = cx - radius;
    int y = cy - radius;
    int size = radius * 2;
    renderer_fill_aa_rounded_rect(x, y, size, size, radius, color, alpha);
}

void renderer_draw_aa_circle(int cx, int cy, int radius, int thickness, uint32_t color, uint8_t alpha) {
    if (radius <= 0 || thickness <= 0 || alpha == 0) return;
    int x = cx - radius;
    int y = cy - radius;
    int size = radius * 2;
    renderer_draw_aa_rounded_rect(x, y, size, size, radius, thickness, color, alpha);
}

/* ------------------------------------------------------------------------------
 * Linear and Radial Gradients
 * -------------------------------------------------------------------------- */

void renderer_fill_gradient_v(int x, int y, int w, int h, uint32_t top_color, uint32_t bottom_color, uint8_t alpha) {
    if (w <= 0 || h <= 0 || alpha == 0) return;

    int t_r = (top_color >> 16) & 0xFF;
    int t_g = (top_color >> 8) & 0xFF;
    int t_b = top_color & 0xFF;

    int b_r = (bottom_color >> 16) & 0xFF;
    int b_g = (bottom_color >> 8) & 0xFF;
    int b_b = bottom_color & 0xFF;

    for (int i = 0; i < h; i++) {
        int factor = (i * 256) / h;
        int inv = 256 - factor;

        uint8_t r = (uint8_t)((t_r * inv + b_r * factor) / 256);
        uint8_t g = (uint8_t)((t_g * inv + b_g * factor) / 256);
        uint8_t b = (uint8_t)((t_b * inv + b_b * factor) / 256);
        uint32_t col = gui_rgb(r, g, b);

        renderer_fill_alpha_rect(x, y + i, w, 1, col, alpha);
    }
}

void renderer_fill_gradient_h(int x, int y, int w, int h, uint32_t left_color, uint32_t right_color, uint8_t alpha) {
    if (w <= 0 || h <= 0 || alpha == 0) return;

    int l_r = (left_color >> 16) & 0xFF;
    int l_g = (left_color >> 8) & 0xFF;
    int l_b = left_color & 0xFF;

    int r_r = (right_color >> 16) & 0xFF;
    int r_g = (right_color >> 8) & 0xFF;
    int r_b = right_color & 0xFF;

    for (int i = 0; i < w; i++) {
        int factor = (i * 256) / w;
        int inv = 256 - factor;

        uint8_t r = (uint8_t)((l_r * inv + r_r * factor) / 256);
        uint8_t g = (uint8_t)((l_g * inv + r_g * factor) / 256);
        uint8_t b = (uint8_t)((l_b * inv + r_b * factor) / 256);
        uint32_t col = gui_rgb(r, g, b);

        renderer_fill_alpha_rect(x + i, y, 1, h, col, alpha);
    }
}

void renderer_fill_gradient_linear(int x, int y, int w, int h, uint32_t start_col, uint32_t end_col, int angle_deg, uint8_t alpha) {
    if (angle_deg == 90 || angle_deg == 270) {
        renderer_fill_gradient_h(x, y, w, h, (angle_deg == 90) ? start_col : end_col, (angle_deg == 90) ? end_col : start_col, alpha);
        return;
    }
    if (angle_deg == 0 || angle_deg == 180) {
        renderer_fill_gradient_v(x, y, w, h, (angle_deg == 0) ? start_col : end_col, (angle_deg == 0) ? end_col : start_col, alpha);
        return;
    }

    /* 45-degree diagonal botanical gradient */
    int s_r = (start_col >> 16) & 0xFF;
    int s_g = (start_col >> 8) & 0xFF;
    int s_b = start_col & 0xFF;

    int e_r = (end_col >> 16) & 0xFF;
    int e_g = (end_col >> 8) & 0xFF;
    int e_b = end_col & 0xFF;

    int total_diag = w + h;
    if (total_diag <= 0) return;

    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            int factor = ((px + py) * 256) / total_diag;
            int inv = 256 - factor;

            uint8_t r = (uint8_t)((s_r * inv + e_r * factor) / 256);
            uint8_t g = (uint8_t)((s_g * inv + e_g * factor) / 256);
            uint8_t b = (uint8_t)((s_b * inv + e_b * factor) / 256);

            renderer_blend_pixel(x + px, y + py, gui_rgb(r, g, b), alpha);
        }
    }
}

void renderer_fill_radial_gradient(int cx, int cy, int radius, uint32_t inner_color, uint32_t outer_color, uint8_t alpha) {
    if (radius <= 0 || alpha == 0) return;

    int in_r = (inner_color >> 16) & 0xFF;
    int in_g = (inner_color >> 8) & 0xFF;
    int in_b = inner_color & 0xFF;

    int out_r = (outer_color >> 16) & 0xFF;
    int out_g = (outer_color >> 8) & 0xFF;
    int out_b = outer_color & 0xFF;

    int r2 = radius * radius;

    int bw = 0, bh = 0;
    renderer_get_bounds(&bw, &bh);

    int clip_x = g_has_custom_clip ? g_cur_clip_x : 0;
    int clip_y = g_has_custom_clip ? g_cur_clip_y : 0;
    int clip_r = g_has_custom_clip ? (g_cur_clip_x + g_cur_clip_w) : bw;
    int clip_b = g_has_custom_clip ? (g_cur_clip_y + g_cur_clip_h) : bh;

    int y0 = int_max(clip_y, cy - radius);
    int y1 = int_min(clip_b, cy + radius + 1);

    for (int py = y0; py < y1; py++) {
        int dy = py - cy;
        int dy2 = dy * dy;
        if (dy2 > r2) continue;

        int dx_max = fast_isqrt(r2 - dy2);
        int x0 = int_max(clip_x, cx - dx_max);
        int x1 = int_min(clip_r, cx + dx_max + 1);

        for (int px = x0; px < x1; px++) {
            int dx = px - cx;
            int dist2 = dx * dx + dy2;
            int d = fast_isqrt(dist2);

            if (d > radius) continue;

            int factor = (d * 256) / radius;
            int inv = 256 - factor;

            uint8_t r = (uint8_t)((in_r * inv + out_r * factor) / 256);
            uint8_t g = (uint8_t)((in_g * inv + out_g * factor) / 256);
            uint8_t b = (uint8_t)((in_b * inv + out_b * factor) / 256);

            renderer_blend_pixel(px, py, gui_rgb(r, g, b), alpha);
        }
    }
}

/* ------------------------------------------------------------------------------
 * Soft Ambient Drop Shadows (Multi-tier Progressive Decay)
 * -------------------------------------------------------------------------- */

void renderer_draw_soft_shadow(int x, int y, int w, int h, int radius, int blur, int offset_x, int offset_y, uint32_t shadow_color, uint8_t max_alpha) {
    if (blur <= 0 || max_alpha == 0 || w <= 0 || h <= 0) return;

    uint32_t s_rgb = shadow_color & 0x00FFFFFF;

    /* Progressive expansion perimeter shells with quadratic falloff.
     * Drawing perimeter bands rather than filling millions of inner pixels
     * provides a massive speedup while preserving smooth ambient diffusion. */
    int num_shells = blur > 4 ? 4 : blur;
    int step_size = (blur + num_shells - 1) / num_shells;
    if (step_size < 1) step_size = 1;

    for (int s = 0; s < num_shells; s++) {
        int dist = (s + 1) * step_size;
        if (dist > blur) dist = blur;

        int remaining = blur - dist + 1;
        uint8_t a = (uint8_t)((max_alpha * remaining * remaining) / (blur * blur));
        if (a == 0) continue;

        int sx = x + offset_x - dist;
        int sy = y + offset_y - dist;
        int sw = w + dist * 2;
        int sh = h + dist * 2;
        int sr = radius + dist;

        renderer_draw_aa_rounded_rect(sx, sy, sw, sh, sr, step_size, s_rgb, a);
    }
}

void renderer_draw_drop_shadow(int x, int y, int w, int h, int radius, int shadow_size, uint8_t max_alpha) {
    if (shadow_size <= 0) return;
    if (max_alpha == 0) max_alpha = 48;
    renderer_draw_soft_shadow(x, y, w, h, radius, shadow_size, 0, 2, 0x12261C, max_alpha);
}

/* ------------------------------------------------------------------------------
 * Physical Frosted Glass / Light Card Panels
 * -------------------------------------------------------------------------- */

void renderer_draw_glass_panel(int x, int y, int w, int h, int radius,
                               uint32_t body_color, uint8_t body_alpha,
                               uint32_t border_color, int shadow_depth) {
    if (w <= 0 || h <= 0) return;

    /* 1. Soft Ambient Drop Shadow */
    if (shadow_depth > 0) {
        int blur = shadow_depth * 3;
        int offset_y = shadow_depth + 1;
        renderer_draw_soft_shadow(x, y, w, h, radius, blur, 0, offset_y, 0x102018, 36);
    }

    /* 2. Glass Body */
    renderer_fill_aa_rounded_rect(x, y, w, h, radius, body_color, body_alpha);

    /* 3. Subtle 1px Top Specular Glint */
    if (w > (radius * 2) + 4) {
        int glint_x = x + radius;
        int glint_w = w - radius * 2;
        renderer_fill_alpha_rect(glint_x, y + 1, glint_w, 1, GH_COLOR_WHITE, 90);
    }

    /* 4. Refined Botanical Border */
    if (border_color != 0) {
        renderer_draw_aa_rounded_rect(x, y, w, h, radius, 1, border_color, 180);
    }
}

void renderer_draw_card_panel(int x, int y, int w, int h, int radius,
                              uint32_t body_color, uint8_t body_alpha,
                              uint32_t border_color, int shadow_depth) {
    renderer_draw_glass_panel(x, y, w, h, radius, body_color, body_alpha, border_color, shadow_depth);
}

void renderer_draw_card(int x, int y, int w, int h, int radius,
                        const char* header, uint32_t header_color,
                        uint32_t bg_color, uint32_t border_color) {
    renderer_draw_glass_panel(x, y, w, h, radius, bg_color, 240, border_color, 2);

    if (header && header[0]) {
        draw_text(x + 14, y + 10, header, header_color, FONT_TRANSPARENT, 0);
        /* Hairline divider */
        renderer_fill_alpha_rect(x + 8, y + 28, w - 16, 1, border_color, 120);
    }
}

/* ------------------------------------------------------------------------------
 * Desktop Background (Botanical Light Mode)
 * -------------------------------------------------------------------------- */

void renderer_draw_light_background(int w, int h) {
    if (w <= 0 || h <= 0) return;

    /* Base serene botanical gradient: off-white to calm soft sage-mist */
    renderer_fill_gradient_v(0, 0, w, h, GH_COLOR_BACKGROUND, GH_COLOR_BG_GRAD_END, 255);

    /* Ambient central botanical bloom */
    int cx = w / 2;
    int cy = h / 2 - 30;
    int radius = w / 2;
    renderer_fill_radial_gradient(cx, cy, radius, GH_COLOR_BG_BLOOM, 0x00000000, 45);

    /* Subtle top horizon ambient highlight */
    renderer_fill_alpha_rect(0, 0, w, 1, 0xFFFFFFFF, 120);
}

/* ------------------------------------------------------------------------------
 * Cached Surfaces (Offscreen 32-bit ARGB Buffers)
 * -------------------------------------------------------------------------- */

renderer_surface_t* renderer_surface_create(int w, int h) {
    if (w <= 0 || h <= 0) return NULL;

    renderer_surface_t* s = (renderer_surface_t*)kmalloc(sizeof(renderer_surface_t));
    if (!s) return NULL;

    size_t byte_size = (size_t)w * (size_t)h * sizeof(uint32_t);
    s->pixels = (uint32_t*)kmalloc(byte_size);
    if (!s->pixels) {
        kfree(s);
        return NULL;
    }

    s->width = w;
    s->height = h;
    s->pitch = w * sizeof(uint32_t);

    renderer_surface_clear(s, 0x00000000);
    return s;
}

void renderer_surface_free(renderer_surface_t* s) {
    if (!s) return;
    if (g_render_target == s) {
        g_render_target = NULL;
    }
    if (s->pixels) {
        kfree(s->pixels);
        s->pixels = NULL;
    }
    kfree(s);
}

void renderer_surface_set_target(renderer_surface_t* s) {
    g_render_target = s;
    renderer_clear_clip();
}

renderer_surface_t* renderer_surface_get_target(void) {
    return g_render_target;
}

void renderer_surface_clear(renderer_surface_t* s, uint32_t color) {
    if (!s || !s->pixels) return;
    size_t count = (size_t)s->width * (size_t)s->height;
    for (size_t i = 0; i < count; i++) {
        s->pixels[i] = color;
    }
}

void renderer_surface_blit(int dst_x, int dst_y, const renderer_surface_t* src, int src_x, int src_y, int w, int h, uint8_t alpha) {
    if (!src || !src->pixels || w <= 0 || h <= 0 || alpha == 0) return;

    if (alpha == 255 && !g_render_target && graphics_is_active() && graphics_get_bpp() == 32) {
        uint8_t* tgt = (uint8_t*)graphics_get_target();
        if (tgt && dst_x == 0 && src_x == 0 && w == graphics_get_width() &&
            dst_y == 0 && src_y == 0 && h == graphics_get_height() &&
            (int)src->pitch == graphics_get_pitch()) {
            /* Full screen 64-bit block transfer (e.g. wallpaper cache) */
            uint64_t* d = (uint64_t*)tgt;
            const uint64_t* s = (const uint64_t*)src->pixels;
            size_t qwords = ((size_t)h * src->pitch) / sizeof(uint64_t);
            for (size_t i = 0; i < qwords; i++) d[i] = s[i];
            graphics_damage_rows(0, h - 1);
            return;
        }

        if (tgt) {
            uint32_t dpitch = graphics_get_pitch();
            int sw = graphics_get_width();
            int sh = graphics_get_height();
            for (int y = 0; y < h; y++) {
                int sy = src_y + y;
                if (sy < 0 || sy >= src->height) continue;
                int dy = dst_y + y;
                if (dy < 0 || dy >= sh) continue;
                uint32_t* drow = (uint32_t*)(tgt + (uint64_t)dy * dpitch);
                const uint32_t* srow = (const uint32_t*)((const uint8_t*)src->pixels + (uint64_t)sy * src->pitch);
                for (int x = 0; x < w; x++) {
                    int sx = src_x + x;
                    int dx = dst_x + x;
                    if (sx >= 0 && sx < src->width && dx >= 0 && dx < sw) {
                        drow[dx] = srow[sx];
                    }
                }
            }
            graphics_damage_rows(dst_y, dst_y + h - 1);
            return;
        }
    }

    for (int y = 0; y < h; y++) {
        int sy = src_y + y;
        if (sy < 0 || sy >= src->height) continue;
        int dy = dst_y + y;

        const uint32_t* src_row = (const uint32_t*)((const uint8_t*)src->pixels + (uint64_t)sy * src->pitch);

        for (int x = 0; x < w; x++) {
            int sx = src_x + x;
            if (sx < 0 || sx >= src->width) continue;
            int dx = dst_x + x;

            uint32_t pix = src_row[sx];
            uint8_t pix_a = (uint8_t)((pix >> 24) & 0xFF);
            if (pix_a == 0) continue;

            uint8_t effective_a = (alpha == 255) ? pix_a : (uint8_t)((pix_a * alpha) / 255);
            renderer_blend_pixel(dx, dy, pix, effective_a);
        }
    }
}

/* ------------------------------------------------------------------------------
 * UI Components
 * -------------------------------------------------------------------------- */

void renderer_draw_button(int x, int y, int w, int h, const char* label,
                          uint32_t bg_color, uint32_t border_color, uint32_t text_color,
                          int is_hovered, int is_pressed, int is_primary) {
    (void)is_primary;
    if (w <= 0 || h <= 0) return;

    uint32_t actual_bg = bg_color;
    uint32_t actual_border = border_color;
    uint8_t alpha = 240;

    if (bg_color == GH_COLOR_TRANSPARENT) {
        if (is_pressed) {
            actual_bg = 0xFFEEEEEE;
            alpha = 180;
        } else if (is_hovered) {
            actual_bg = GH_COLOR_SURFACE_HOVER;
            alpha = 180;
        } else {
            alpha = 0;
        }
    } else {
        if (is_pressed) {
            actual_bg = renderer_alpha_blend(0x000000, bg_color, 40);
            alpha = 255;
        } else if (is_hovered) {
            actual_bg = renderer_alpha_blend(0xFFFFFF, bg_color, 30);
            alpha = 250;
        }
    }

    int radius = GH_RADIUS_BUTTON;
    if (alpha > 0) {
        renderer_fill_aa_rounded_rect(x, y, w, h, radius, actual_bg, alpha);
    }
    if (actual_border != GH_COLOR_TRANSPARENT) {
        renderer_draw_aa_rounded_rect(x, y, w, h, radius, 1, actual_border, 200);
    }

    /* Subtle top highlight on primary buttons */
    if (w > radius * 2 + 4) {
        renderer_fill_alpha_rect(x + radius, y + 1, w - radius * 2, 1, 0xFFFFFFFF, is_pressed ? 20 : 60);
    }

    if (label && label[0]) {
        int tw = font_text_width(label);
        int tx = x + (w - tw) / 2;
        int ty = y + (h - font_glyph_height()) / 2;
        draw_text(tx, ty, label, text_color, FONT_TRANSPARENT, 0);
    }
}

void renderer_draw_pill_button(int x, int y, int w, int h, const char* label,
                               uint32_t bg_color, uint32_t border_color, uint32_t text_color, int is_active) {
    int radius = h / 2;
    uint8_t alpha = is_active ? 245 : 180;

    renderer_fill_aa_rounded_rect(x, y, w, h, radius, bg_color, alpha);
    renderer_draw_aa_rounded_rect(x, y, w, h, radius, 1, border_color, 200);

    if (is_active) {
        renderer_fill_alpha_rect(x + radius, y + 1, w - radius * 2, 1, 0xFFFFFFFF, 80);
    }

    if (label && label[0]) {
        int tw = font_text_width(label);
        int tx = x + (w - tw) / 2;
        int ty = y + (h - font_glyph_height()) / 2;
        draw_text(tx, ty, label, text_color, FONT_TRANSPARENT, 0);
    }
}

void renderer_draw_badge(int x, int y, const char* text, uint32_t bg_color, uint32_t text_color, int radius) {
    if (!text || !text[0]) return;
    int tw = font_text_width(text);
    int pad_x = 7;
    int pad_y = 2;
    int bw = tw + pad_x * 2;
    int bh = font_glyph_height() + pad_y * 2;

    renderer_fill_aa_rounded_rect(x, y, bw, bh, radius, bg_color, 240);
    renderer_draw_aa_rounded_rect(x, y, bw, bh, radius, 1, GH_COLOR_BORDER_LIGHT, 160);
    draw_text(x + pad_x, y + pad_y, text, text_color, FONT_TRANSPARENT, 0);
}

void renderer_draw_meter(int x, int y, int w, int h, int percent,
                         uint32_t fill_color, uint32_t bg_color, uint32_t border_color,
                         int radius) {
    if (w <= 0 || h <= 0) return;
    percent = int_clamp(percent, 0, 100);

    renderer_fill_aa_rounded_rect(x, y, w, h, radius, bg_color, 200);
    renderer_draw_aa_rounded_rect(x, y, w, h, radius, 1, border_color, 160);

    int fill_w = (w * percent) / 100;
    if (fill_w > 2) {
        int r_fill = radius;
        if (fill_w < r_fill * 2) r_fill = fill_w / 2;
        renderer_fill_aa_rounded_rect(x, y, fill_w, h, r_fill, fill_color, 245);
        /* Subtle specular top line on meter */
        renderer_fill_alpha_rect(x + r_fill, y + 1, (fill_w > r_fill * 2) ? (fill_w - r_fill * 2) : 1, 1, 0xFFFFFFFF, 90);
    }
}

void renderer_draw_input_field(int x, int y, int w, int h, const char* text,
                               uint32_t bg_color, uint32_t border_color, uint32_t text_color,
                               uint32_t placeholder_color, int is_focused, int has_text) {
    (void)placeholder_color;
    if (w <= 0 || h <= 0) return;

    int radius = GH_RADIUS_INPUT;
    uint32_t actual_border = border_color;
    uint8_t bg_alpha = 250;

    if (is_focused) {
        actual_border = GH_COLOR_GREEN_LEAF;
        bg_alpha = 255;
    }

    renderer_fill_aa_rounded_rect(x, y, w, h, radius, bg_color, bg_alpha);
    renderer_draw_aa_rounded_rect(x, y, w, h, radius, is_focused ? 2 : 1, actual_border, 255);

    if (text && text[0] && has_text) {
        int tx = x + GH_SPACE_2;
        int ty = y + (h - font_glyph_height()) / 2;
        draw_text(tx, ty, text, text_color, FONT_TRANSPARENT, 0);
    }
}

void renderer_draw_separator(int x, int y, int w, uint32_t color, uint8_t alpha) {
    if (w <= 0) return;
    renderer_fill_alpha_rect(x, y, w, 1, color, alpha);
}

void renderer_draw_avatar(int x, int y, int size, const char* initials,
                          uint32_t bg_color, uint32_t text_color) {
    if (size <= 0) return;

    int radius = size / 2;
    renderer_fill_aa_circle(x + radius, y + radius, radius, bg_color, 250);
    renderer_draw_aa_circle(x + radius, y + radius, radius, 1, GH_COLOR_BORDER_LIGHT, 160);

    if (initials && initials[0]) {
        int tw = font_text_width(initials);
        int tx = x + (size - tw) / 2;
        int ty = y + (size - font_glyph_height()) / 2;
        draw_text(tx, ty, initials, text_color, FONT_TRANSPARENT, 0);
    }
}

void renderer_draw_button_styled(int x, int y, int w, int h, const char* label,
                                 int style, int is_hovered, int is_pressed) {
    switch (style) {
        case RENDERER_BTN_PRIMARY:
            renderer_draw_button(x, y, w, h, label,
                                 GH_COLOR_GREEN_FOREST, GH_COLOR_GREEN_FOREST, GH_COLOR_WHITE,
                                 is_hovered, is_pressed, 1);
            break;
        case RENDERER_BTN_SECONDARY:
            renderer_draw_button(x, y, w, h, label,
                                 GH_COLOR_SURFACE, GH_COLOR_BORDER_LIGHT, GH_COLOR_TEXT_PRIMARY,
                                 is_hovered, is_pressed, 0);
            break;
        case RENDERER_BTN_GHOST:
            renderer_draw_button(x, y, w, h, label,
                                 GH_COLOR_TRANSPARENT, GH_COLOR_TRANSPARENT, GH_COLOR_TEXT_PRIMARY,
                                 is_hovered, is_pressed, 0);
            break;
        case RENDERER_BTN_OUTLINE:
            renderer_draw_button(x, y, w, h, label,
                                 GH_COLOR_TRANSPARENT, GH_COLOR_BORDER_LIGHT, GH_COLOR_TEXT_PRIMARY,
                                 is_hovered, is_pressed, 0);
            break;
        case RENDERER_BTN_BRAND:
            renderer_draw_button(x, y, w, h, label,
                                 GH_COLOR_GREEN_LEAF, GH_COLOR_GREEN_LEAF, GH_COLOR_WHITE,
                                 is_hovered, is_pressed, 1);
            break;
        case RENDERER_BTN_DESTRUCTIVE:
            renderer_draw_button(x, y, w, h, label,
                                 GH_COLOR_DESTRUCTIVE, GH_COLOR_DESTRUCTIVE, GH_COLOR_DESTRUCTIVE_FG,
                                 is_hovered, is_pressed, 1);
            break;
        default:
            renderer_draw_button(x, y, w, h, label,
                                 GH_COLOR_SURFACE, GH_COLOR_BORDER_LIGHT, GH_COLOR_TEXT_PRIMARY,
                                 is_hovered, is_pressed, 0);
            break;
    }
}
