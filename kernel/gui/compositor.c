/* ==============================================================================
 * Greenhouse OS — Modern Liquid-Glass Compositor (Implementation)
 * ==============================================================================
 */

#include "compositor.h"
#include "surface.h"
#include "renderer.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"

void compositor_init(void) {
    /* Compositor initialization */
}

void compositor_draw_wallpaper(int width, int height) {
    if (width <= 0 || height <= 0 || !graphics_is_active()) return;

    /* 1. Calm, deep atmospheric gradient */
    graphics_fill_gradient_v(0, 0, width, height, GFX_COLOR_WALLPAPER_TOP, GFX_COLOR_WALLPAPER_BOT);

    /* 2. Soft organic ambient depth aura (centered, very low opacity) */
    int cx = width / 2;
    int cy = height / 2 - 40;
    for (int r = 240; r > 0; r -= 40) {
        uint8_t a = (uint8_t)((240 - r) / 35);
        renderer_fill_alpha_rounded_rect(cx - r, cy - (r * 2 / 3), r * 2, r * 4 / 3, r, GFX_COLOR_EMERALD_PRIMARY, a);
    }
}

void compositor_draw_surface_frame(int x, int y, int w, int h,
                                   const char* title, const char* subtitle,
                                   int is_active, int is_maximized) {
    (void)is_maximized;
    if (w <= 0 || h <= 0 || !graphics_is_active()) return;

    int radius = COMPOSITOR_CORNER_RADIUS;
    uint32_t body_color = is_active ? GFX_COLOR_GLASS_SURFACE_ACT : GFX_COLOR_GLASS_SURFACE;
    uint8_t body_alpha = is_active ? 220 : 190;
    uint32_t border_color = is_active ? GFX_COLOR_GLASS_BORDER_ACT : GFX_COLOR_GLASS_BORDER;

    /* 1. Glass Panel Body with Soft Shadow & Highlight */
    renderer_draw_glass_panel(x, y, w, h, radius, body_color, body_alpha, border_color, 4);

    /* 2. Title Bar Region */
    int th = COMPOSITOR_TITLE_HEIGHT;
    /* Hairline divider below titlebar */
    renderer_fill_alpha_rect(x + 8, y + th, w - 16, 1, border_color, 120);

    /* 3. Window Header Accent Pill & Title */
    int title_x = x + 12;
    int title_y = y + 7;

    /* Accent dot */
    uint32_t dot_color = is_active ? GFX_COLOR_EMERALD_PRIMARY : GFX_COLOR_TEXT_MUTED;
    renderer_fill_alpha_rounded_rect(title_x, title_y + 4, 8, 8, 4, dot_color, 240);
    title_x += 14;

    /* Title text */
    if (title && title[0]) {
        uint32_t t_color = is_active ? GFX_COLOR_TEXT_PRIMARY : GFX_COLOR_TEXT_SECONDARY;
        draw_text(title_x, title_y, title, t_color, FONT_TRANSPARENT, 0);
        title_x += font_text_width(title) + 10;
    }

    /* Subtitle / Badge */
    if (subtitle && subtitle[0] && (title_x + font_text_width(subtitle) + 80 < x + w)) {
        renderer_draw_badge(title_x, title_y - 1, subtitle, 0xFF1E3A34, GFX_COLOR_MINT_ACCENT);
    }

    /* 4. Modern Window Controls (Pill style on top right) */
    int ctrl_right = x + w - 10;
    int ctrl_y = y + 8;
    int btn_size = 14;

    /* Close Button (Soft Red) */
    int close_x = ctrl_right - btn_size;
    renderer_fill_alpha_rounded_rect(close_x, ctrl_y, btn_size, btn_size, 7, 0xFFEF4444, is_active ? 220 : 140);
    /* Close glyph 'x' */
    draw_char(close_x + 3, ctrl_y - 1, 'x', 0xFFFFFFFF, FONT_TRANSPARENT, 0);

    /* Maximize Button (Soft Green) */
    int max_x = close_x - btn_size - 6;
    renderer_fill_alpha_rounded_rect(max_x, ctrl_y, btn_size, btn_size, 7, 0xFF10B981, is_active ? 220 : 140);
    draw_char(max_x + 3, ctrl_y - 1, '+', 0xFFFFFFFF, FONT_TRANSPARENT, 0);

    /* Minimize Button (Soft Amber) */
    int min_x = max_x - btn_size - 6;
    renderer_fill_alpha_rounded_rect(min_x, ctrl_y, btn_size, btn_size, 7, 0xFFF59E0B, is_active ? 220 : 140);
    draw_char(min_x + 3, ctrl_y - 1, '-', 0xFFFFFFFF, FONT_TRANSPARENT, 0);

    /* 5. Resize Handle Accent (Bottom right corner) */
    int rz_x = x + w - 8;
    int rz_y = y + h - 8;
    renderer_blend_pixel(rz_x, rz_y, border_color, 200);
    renderer_blend_pixel(rz_x - 3, rz_y, border_color, 160);
    renderer_blend_pixel(rz_x, rz_y - 3, border_color, 160);
    renderer_blend_pixel(rz_x - 6, rz_y, border_color, 120);
    renderer_blend_pixel(rz_x - 3, rz_y - 3, border_color, 120);
    renderer_blend_pixel(rz_x, rz_y - 6, border_color, 120);
}

int compositor_hit_test(int win_x, int win_y, int win_w, int win_h, int click_x, int click_y) {
    if (click_x < win_x || click_x >= win_x + win_w ||
        click_y < win_y || click_y >= win_y + win_h) {
        return COMPOSITOR_ZONE_NONE;
    }

    /* Check Titlebar controls */
    int th = COMPOSITOR_TITLE_HEIGHT;
    if (click_y >= win_y && click_y < win_y + th) {
        int ctrl_right = win_x + win_w - 10;
        int btn_size = 14;

        int close_x = ctrl_right - btn_size;
        if (click_x >= close_x - 2 && click_x <= close_x + btn_size + 2) return COMPOSITOR_ZONE_CLOSE;

        int max_x = close_x - btn_size - 6;
        if (click_x >= max_x - 2 && click_x <= max_x + btn_size + 2) return COMPOSITOR_ZONE_MAXIMIZE;

        int min_x = max_x - btn_size - 6;
        if (click_x >= min_x - 2 && click_x <= min_x + btn_size + 2) return COMPOSITOR_ZONE_MINIMIZE;

        return COMPOSITOR_ZONE_TITLE;
    }

    /* Check bottom-right resize corner */
    if (click_x >= win_x + win_w - 16 && click_y >= win_y + win_h - 16) {
        return COMPOSITOR_ZONE_RESIZE;
    }

    return COMPOSITOR_ZONE_CLIENT;
}

void compositor_client_rect(const surface_t* s, int* cx, int* cy, int* cw, int* ch) {
    if (!s) return;
    int decor = (s->flags & SURFACE_FLAG_NO_DECOR) ? 0 : COMPOSITOR_BORDER_SIZE;
    int th = (s->flags & SURFACE_FLAG_NO_DECOR) ? 0 : COMPOSITOR_TITLE_HEIGHT;
    if (cx) *cx = s->x + decor + 2;
    if (cy) *cy = s->y + decor + th + 1;
    if (cw) *cw = s->w - (decor * 2) - 4;
    if (ch) *ch = s->h - (decor * 2) - th - 3;
}
