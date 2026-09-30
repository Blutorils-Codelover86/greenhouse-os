/* ==============================================================================
 * Greenhouse OS - Light-Mode Surface System (implementation)
 * ==============================================================================
 */

#include "surface.h"
#include "morph.h"
#include "gh_theme.h"

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
    s->target_opacity = 255;

    /* Light Mode Theme */
    s->bg_color     = GH_COLOR_SURFACE;
    s->fg_color     = GH_COLOR_TEXT_PRIMARY;
    s->border_color = GH_COLOR_BORDER_LIGHT;
    s->title_bg     = GH_COLOR_SURFACE;
    s->title_fg     = GH_COLOR_TEXT_PRIMARY;
    s->accent       = accent ? accent : GH_COLOR_GREEN_LEAF_DEEP;
    s->client_color = GH_COLOR_BACKGROUND;

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

void surface_set_opacity(surface_t* s, uint8_t opacity, int animate) {
    if (!s) return;
    s->target_opacity = opacity;
    if (!animate) s->opacity = opacity;
}

int surface_includes(const surface_t* s, int px, int py) {
    if (!s || !s->visible || s->is_minimized) return 0;
    return (px >= s->x && px < s->x + s->w && py >= s->y && py < s->y + s->h);
}

void surface_step_animation(surface_t* s) {
    if (!s) return;
    morph_surface_transition(&s->x, &s->y, &s->w, &s->h, &s->opacity,
                             s->target_x, s->target_y, s->target_w, s->target_h,
                             s->target_opacity, 3);
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
    s->target_y = 46; /* Leave room for top bar */
    s->target_w = max_w - 24;
    s->target_h = (max_h > 120) ? (max_h - 108) : (max_h - 54); /* Room for dock */
    s->x = s->target_x;
    s->y = s->target_y;
    s->w = s->target_w;
    s->h = s->target_h;
    s->is_maximized = 1;
}

void surface_restore(surface_t* s) {
    if (!s) return;
    s->target_x = s->saved_x;
    s->target_y = s->saved_y;
    s->target_w = s->saved_w;
    s->target_h = s->saved_h;
    s->x = s->saved_x;
    s->y = s->saved_y;
    s->w = s->saved_w;
    s->h = s->saved_h;
    s->is_maximized = 0;
    s->is_minimized = 0;
    s->visible = 1;
}
