/* ==============================================================================
 * Greenhouse OS - VERDANT Surface System (implementation)
 * ==============================================================================
 */

#include "surface.h"
#include "morph.h"

static void surface_str_copy(char* dst, int cap, const char* src) {
    int i = 0;
    if (src) {
        for (; src[i] && i < cap - 1; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
}

void surface_init(surface_t* s, surface_id_t id, const char* title, const char* tag,
                  int x, int y, int w, int h, uint32_t accent) {
    if (!s) return;

    s->id = id;
    surface_str_copy(s->title, SURFACE_TITLE_LEN, title ? title : "Surface");
    surface_str_copy(s->tag, SURFACE_TAG_LEN, tag ? tag : "APP");

    s->x = x;
    s->y = y;
    s->w = (w > 120) ? w : 120;
    s->h = (h > 80) ? h : 80;

    s->target_x = s->x;
    s->target_y = s->y;
    s->target_w = s->w;
    s->target_h = s->h;

    s->min_w = 160;
    s->min_h = 100;
    s->visible = 1;
    s->is_minimized = 0;
    s->is_maximized = 0;
    s->saved_x = s->x;
    s->saved_y = s->y;
    s->saved_w = s->w;
    s->saved_h = s->h;
    s->flags = SURFACE_FLAG_RESIZABLE;
    s->z_order = 0;
    s->opacity = 255;

    /* Verdant Deep Glass Palette */
    s->bg_color     = 0xFF0D1522; /* Deep Glass Background */
    s->fg_color     = 0xFFE2E8F0; /* Bright Silver White */
    s->border_color = 0xFF1E293B; /* Subtle Slate Border */
    s->title_bg     = 0xFF0F1A2C; /* Glass Header */
    s->title_fg     = 0xFFF1F5F9; /* High Contrast Title */
    s->accent       = accent ? accent : 0xFF10B981; /* Default Emerald */
    s->client_color = 0xFF080D16; /* Deep Client Background */

    s->user_data    = 0;
    s->on_draw      = 0;
    s->on_event     = 0;
    s->on_close     = 0;
}

void surface_set_title(surface_t* s, const char* title) {
    if (!s) return;
    surface_str_copy(s->title, SURFACE_TITLE_LEN, title);
}

void surface_set_tag(surface_t* s, const char* tag) {
    if (!s) return;
    surface_str_copy(s->tag, SURFACE_TAG_LEN, tag);
}

void surface_set_colors(surface_t* s, uint32_t bg, uint32_t fg, uint32_t border) {
    if (!s) return;
    s->bg_color = bg;
    s->fg_color = fg;
    s->border_color = border;
}

void surface_set_client_color(surface_t* s, uint32_t color) {
    if (!s) return;
    s->client_color = color;
}

int surface_includes(const surface_t* s, int px, int py) {
    if (!s || !s->visible || s->is_minimized) return 0;
    return (px >= s->x && px < s->x + s->w && py >= s->y && py < s->y + s->h);
}

void surface_step_animation(surface_t* s) {
    if (!s) return;
    morph_rect(&s->x, &s->y, &s->w, &s->h,
               s->target_x, s->target_y, s->target_w, s->target_h, 3);
}

void surface_maximize(surface_t* s, int max_w, int max_h) {
    if (!s) return;
    if (s->is_maximized) {
        surface_restore(s);
        return;
    }
    s->saved_x = s->x;
    s->saved_y = s->y;
    s->saved_w = s->w;
    s->saved_h = s->h;
    s->target_x = 12;
    s->target_y = 42; /* Leave room for System Rail */
    s->target_w = max_w - 24;
    s->target_h = max_h - 54;
    s->is_maximized = 1;
}

void surface_restore(surface_t* s) {
    if (!s) return;
    s->target_x = s->saved_x;
    s->target_y = s->saved_y;
    s->target_w = s->saved_w;
    s->target_h = s->saved_h;
    s->is_maximized = 0;
    s->is_minimized = 0;
    s->visible = 1;
}
