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

static renderer_surface_t* g_wallpaper_cache = NULL;
static int g_wallpaper_cache_w = 0;
static int g_wallpaper_cache_h = 0;

static void compositor_render_background(int width, int height) {
    /* 1. Luminous botanical porcelain gradient canvas (berrygreenhouse.vercel.app aesthetic) */
    renderer_fill_gradient_v(0, 0, width, height, GH_COLOR_BACKGROUND, GH_COLOR_BG_GRAD_END, 255);

    /* 2. Soft luminous ambient emerald halo bloom in the upper-center */
    int cx = width / 2;
    int cy = height / 2 - 30;
    int radius = width / 2;
    renderer_fill_radial_gradient(cx, cy, radius, GH_COLOR_BG_BLOOM, 0x00000000, 55);

    /* 3. Subtle ambient inner halo bloom */
    renderer_fill_radial_gradient(cx, cy, radius / 2, GH_COLOR_GREEN_HALO, 0x00000000, 40);

    /* 4. Architectural Botanical Conservatory Rings (Geodesic Dome / Observatory Motif) */
    uint32_t ring_col = 0x00A4C0B0;
    renderer_draw_aa_circle(cx, cy, 90, 1, ring_col, 50);
    renderer_draw_aa_circle(cx, cy, 180, 1, ring_col, 45);
    renderer_draw_aa_circle(cx, cy, 290, 1, ring_col, 38);
    renderer_draw_aa_circle(cx, cy, 420, 1, ring_col, 32);
    renderer_draw_aa_circle(cx, cy, 570, 1, ring_col, 26);
    renderer_draw_aa_circle(cx, cy, 740, 1, ring_col, 20);

    /* 5. Architectural Conservatory Perspective Ribs (Subtle radial structural lines) */
    int rib_len = radius + 100;
    renderer_draw_line(cx - rib_len, cy, cx + rib_len, cy, ring_col, 22);
    renderer_draw_line(cx, cy - rib_len, cx, cy + rib_len, ring_col, 22);
    int d = (rib_len * 707) / 1000;
    renderer_draw_line(cx - d, cy - d, cx + d, cy + d, ring_col, 20);
    renderer_draw_line(cx - d, cy + d, cx + d, cy - d, ring_col, 20);

    /* 6. Central Botanical Rosette Accent */
    renderer_draw_aa_circle(cx - 30, cy, 40, 1, GH_COLOR_GREEN_LEAF, 40);
    renderer_draw_aa_circle(cx + 30, cy, 40, 1, GH_COLOR_GREEN_LEAF, 40);
    renderer_draw_aa_circle(cx, cy - 30, 40, 1, GH_COLOR_GREEN_LEAF, 40);
    renderer_draw_aa_circle(cx, cy + 30, 40, 1, GH_COLOR_GREEN_LEAF, 40);
    renderer_fill_aa_circle(cx, cy, 5, GH_COLOR_GREEN_LEAF, 120);

    /* 7. Fine 1px subtle horizon specular rim */
    renderer_fill_alpha_rect(0, 0, width, 1, GH_COLOR_WHITE, 90);
}

void compositor_draw_wallpaper(int width, int height) {
    if (width <= 0 || height <= 0 || !graphics_is_active()) return;

    if (!g_wallpaper_cache || g_wallpaper_cache_w != width || g_wallpaper_cache_h != height) {
        if (g_wallpaper_cache) {
            renderer_surface_free(g_wallpaper_cache);
            g_wallpaper_cache = NULL;
        }
        g_wallpaper_cache = renderer_surface_create(width, height);
        if (g_wallpaper_cache) {
            g_wallpaper_cache_w = width;
            g_wallpaper_cache_h = height;
            renderer_surface_set_target(g_wallpaper_cache);
            compositor_render_background(width, height);
            renderer_surface_set_target(NULL);
        }
    }

    if (g_wallpaper_cache) {
        renderer_surface_blit(0, 0, g_wallpaper_cache, 0, 0, width, height, 255);
    } else {
        compositor_render_background(width, height);
    }
}

void compositor_draw_surface_frame(int x, int y, int w, int h,
                                    const char* title, const char* subtitle,
                                    int is_active, int is_maximized) {
    if (w <= 0 || h <= 0 || !graphics_is_active()) return;

    int radius = COMPOSITOR_CORNER_RADIUS;
    uint32_t body_color = GH_COLOR_SURFACE;
    uint8_t body_alpha = 246;
    uint32_t border_color = is_active ? GH_COLOR_BORDER_FOCUS : GH_COLOR_BORDER_LIGHT;

    /* 1. Subtle Multi-tier Ambient Drop Shadow (Light Mode soft diffuse elevation) */
    if (is_active) {
        renderer_draw_soft_shadow(x, y, w, h, radius, 8, 0, 4, 0x102018, 36);
    } else {
        renderer_draw_soft_shadow(x, y, w, h, radius, 4, 0, 2, 0x102018, 22);
    }

    /* 2. Window Frame Body & Refined Border */
    renderer_fill_aa_rounded_rect(x, y, w, h, radius, body_color, body_alpha);
    renderer_draw_aa_rounded_rect(x, y, w, h, radius, 1, border_color, is_active ? 255 : 180);

    /* 3. Subtle Top Specular Glint */
    if (w > (radius * 2) + 4) {
        renderer_fill_alpha_rect(x + radius, y + 1, w - radius * 2, 1, GH_COLOR_WHITE, is_active ? 140 : 80);
    }

    /* 4. Title Bar Region */
    int th = COMPOSITOR_TITLE_HEIGHT;
    /* Hairline divider below titlebar */
    renderer_fill_alpha_rect(x + 4, y + th, w - 8, 1, GH_COLOR_BORDER_LIGHT, 160);

    /* 5. Window Header Accent Pill & Title */
    int title_x = x + 12;
    int title_y = y + 8;

    /* Status dot: vibrant emerald when active, faint sage when inactive */
    renderer_fill_aa_circle(title_x + 4, title_y + 8, 4, is_active ? GH_COLOR_GREEN_LEAF : GH_COLOR_TEXT_FAINT, 255);
    title_x += 16;

    /* Title text in botanical ink */
    if (title && title[0]) {
        uint32_t t_color = is_active ? GH_COLOR_TEXT_PRIMARY : GH_COLOR_TEXT_SECONDARY;
        draw_text(title_x, title_y, title, t_color, FONT_TRANSPARENT, 0);
        title_x += font_text_width(title) + 10;
    }

    /* Subtitle / Badge */
    if (subtitle && subtitle[0] && (title_x + font_text_width(subtitle) + 90 < x + w)) {
        renderer_draw_badge(title_x, title_y - 1, subtitle, GH_COLOR_GREEN_PILL_BG, GH_COLOR_GREEN_PILL_FG, 4);
    }

    /* 6. Modern Window Controls (Right side, pill buttons with Light Mode hover) */
    int btn_w = 20;
    int btn_h = 18;
    int ctrl_right = x + w - 12;
    int ctrl_y = y + 7;

    int close_x = ctrl_right - btn_w;
    int max_x = close_x - btn_w - 4;
    int min_x = max_x - btn_w - 4;

    /* Minimize Button */
    renderer_fill_aa_rounded_rect(min_x, ctrl_y, btn_w, btn_h, 4, GH_COLOR_SURFACE_HOVER, 220);
    renderer_draw_aa_rounded_rect(min_x, ctrl_y, btn_w, btn_h, 4, 1, GH_COLOR_BORDER_LIGHT, 180);
    renderer_fill_alpha_rect(min_x + 5, ctrl_y + 8, 10, 2, GH_COLOR_TEXT_SECONDARY, 240);

    /* Maximize / Restore Button */
    renderer_fill_aa_rounded_rect(max_x, ctrl_y, btn_w, btn_h, 4, GH_COLOR_SURFACE_HOVER, 220);
    renderer_draw_aa_rounded_rect(max_x, ctrl_y, btn_w, btn_h, 4, 1, GH_COLOR_BORDER_LIGHT, 180);
    if (is_maximized) {
        renderer_draw_aa_rounded_rect(max_x + 4, ctrl_y + 5, 8, 8, 2, 1, GH_COLOR_TEXT_SECONDARY, 240);
        renderer_draw_aa_rounded_rect(max_x + 7, ctrl_y + 3, 8, 8, 2, 1, GH_COLOR_TEXT_SECONDARY, 240);
    } else {
        renderer_draw_aa_rounded_rect(max_x + 5, ctrl_y + 4, 10, 10, 2, 1, GH_COLOR_TEXT_SECONDARY, 240);
    }

    /* Close Button (Gentle coral red accent in light mode) */
    renderer_fill_aa_rounded_rect(close_x, ctrl_y, btn_w, btn_h, 4, is_active ? 0xFFFEE2E2 : GH_COLOR_SURFACE_HOVER, 240);
    renderer_draw_aa_rounded_rect(close_x, ctrl_y, btn_w, btn_h, 4, 1, is_active ? 0xFFFECACA : GH_COLOR_BORDER_LIGHT, 220);
    draw_char(close_x + 6, ctrl_y + 1, 'x', is_active ? GH_COLOR_RED_ERROR : GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);

    /* 7. Resize Handle Accent (Bottom-right corner) */
    int rz_x = x + w - 8;
    int rz_y = y + h - 8;
    renderer_blend_pixel(rz_x, rz_y, GH_COLOR_BORDER_LIGHT, 220);
    renderer_blend_pixel(rz_x - 3, rz_y, GH_COLOR_BORDER_LIGHT, 180);
    renderer_blend_pixel(rz_x, rz_y - 3, GH_COLOR_BORDER_LIGHT, 180);
    renderer_blend_pixel(rz_x - 6, rz_y, GH_COLOR_BORDER_LIGHT, 140);
    renderer_blend_pixel(rz_x - 3, rz_y - 3, GH_COLOR_BORDER_LIGHT, 140);
    renderer_blend_pixel(rz_x, rz_y - 6, GH_COLOR_BORDER_LIGHT, 140);
}

int compositor_hit_test(int win_x, int win_y, int win_w, int win_h, int click_x, int click_y) {
    if (click_x < win_x || click_x >= win_x + win_w ||
        click_y < win_y || click_y >= win_y + win_h) {
        return COMPOSITOR_ZONE_NONE;
    }

    /* Check Titlebar controls */
    int th = COMPOSITOR_TITLE_HEIGHT;
    if (click_y >= win_y && click_y < win_y + th) {
        int btn_w = 20;
        int ctrl_right = win_x + win_w - 12;

        int close_x = ctrl_right - btn_w;
        if (click_x >= close_x - 2 && click_x <= close_x + btn_w + 2) return COMPOSITOR_ZONE_CLOSE;

        int max_x = close_x - btn_w - 4;
        if (click_x >= max_x - 2 && click_x <= max_x + btn_w + 2) return COMPOSITOR_ZONE_MAXIMIZE;

        int min_x = max_x - btn_w - 4;
        if (click_x >= min_x - 2 && click_x <= min_x + btn_w + 2) return COMPOSITOR_ZONE_MINIMIZE;

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
