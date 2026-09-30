/* ==============================================================================
 * Greenhouse OS - Light System Rail (implementation)
 * ==============================================================================
 */

#include "rail.h"
#include "morph.h"
#include "gh_theme.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "../pmm.h"
#include "../irq.h"

extern uint64_t timer_get_ticks(void);
extern uint32_t timer_get_frequency(void);

static int g_rail_target_y = 4;
static int g_rail_current_y = 4;
static int g_rail_locked = 1;

void rail_init(void) {
    g_rail_target_y = 4;
    g_rail_current_y = 4;
    g_rail_locked = 1;
}

void rail_request_emerge(void) {
    g_rail_target_y = 4;
}

void rail_toggle_locked(void) {
    g_rail_locked = !g_rail_locked;
    if (!g_rail_locked) {
        g_rail_target_y = -22;
    } else {
        g_rail_target_y = 4;
    }
}

int rail_is_emerged(void) {
    return (g_rail_current_y >= 0);
}

static void rail_put_uint(char* buf, int* n, int cap, uint64_t v) {
    char tmp[24];
    int tl = 0;
    do { tmp[tl++] = (char)('0' + (v % 10)); v /= 10; } while (v);
    while (tl && *n < cap - 1) buf[(*n)++] = tmp[--tl];
    buf[*n] = '\0';
}

void rail_draw(int sw, int sh, surface_t* surfaces, int count, int focused_id) {
    (void)sh;
    if (g_rail_current_y != g_rail_target_y) {
        g_rail_current_y = morph_step_ease(g_rail_current_y, g_rail_target_y, 4, 2);
    }

    int rx = 8;
    int ry = g_rail_current_y;
    int rw = sw - 16;
    int rh = 28;

    /* If collapsed, show subtle emergence strip at top */
    if (ry < 0) {
        graphics_fill_rounded_rect((sw - 120) / 2, 0, 120, 6, 3, GH_COLOR_GREEN_LEAF);
        return;
    }

    /* 1. Main Rail Card Panel */
    renderer_draw_card_panel(rx, ry, rw, rh, GH_RADIUS_MD,
                             GH_COLOR_SURFACE, 250, GH_COLOR_BORDER_LIGHT, 2);

    /* 2. Left Buttons */
    /* [ Berry ] Button */
    int berry_x = rx + 6;
    int berry_w = 84;
    int berry_h = 22;
    renderer_draw_button_styled(berry_x, ry + 3, berry_w, berry_h, "[Berry]",
                                RENDERER_BTN_OUTLINE, 0, 0);

    /* [ Apps ] Button */
    int apps_x = rx + 96;
    int apps_w = 76;
    int apps_h = 22;
    renderer_draw_button_styled(apps_x, ry + 3, apps_w, apps_h, "[Apps]",
                                RENDERER_BTN_OUTLINE, 0, 0);

    /* 3. Middle Surface Switcher Tabs */
    int tab_x = rx + 182;
    for (int i = 0; i < count && i < 4; i++) {
        surface_t* s = &surfaces[i];
        if (!s->visible && !s->is_minimized) continue;

        int is_focus = (s->id == (uint32_t)focused_id);
        int tab_w = 108;
        int tab_h = 22;

        /* Tab background */
        if (is_focus) {
            renderer_fill_alpha_rounded_rect(tab_x, ry + 3, tab_w, tab_h, GH_RADIUS_MD,
                                             GH_COLOR_SURFACE_HOVER, 255);
            graphics_draw_rounded_rect(tab_x, ry + 3, tab_w, tab_h, GH_RADIUS_MD, GH_COLOR_GREEN_LEAF);
        } else if (s->is_minimized) {
            renderer_fill_alpha_rounded_rect(tab_x, ry + 3, tab_w, tab_h, GH_RADIUS_MD,
                                             GH_COLOR_SURFACE, 200);
            graphics_draw_rounded_rect(tab_x, ry + 3, tab_w, tab_h, GH_RADIUS_MD, GH_COLOR_BORDER_LIGHT);
        } else {
            renderer_fill_alpha_rounded_rect(tab_x, ry + 3, tab_w, tab_h, GH_RADIUS_MD,
                                             GH_COLOR_BACKGROUND, 220);
            graphics_draw_rounded_rect(tab_x, ry + 3, tab_w, tab_h, GH_RADIUS_MD, GH_COLOR_BORDER_LIGHT);
        }

        /* Tag pill inside tab */
        draw_text_clipped(tab_x + 6, ry + 6, 96, s->title,
                          is_focus ? GH_COLOR_TEXT_PRIMARY : GH_COLOR_TEXT_SECONDARY,
                          FONT_TRANSPARENT, 0);
        tab_x += 114;
    }

    /* 4. Right Status Chips */
    /* [ Text Mode ] Exit button */
    int exit_x = rx + rw - 74;
    int exit_w = 68;
    int exit_h = 22;
    renderer_draw_button_styled(exit_x, ry + 3, exit_w, exit_h, "Text [X]",
                                RENDERER_BTN_OUTLINE, 0, 0);

    /* Uptime Readout */
    uint64_t ticks = timer_get_ticks();
    uint32_t hz = timer_get_frequency();
    if (hz == 0) hz = 100;
    uint64_t uptime_s = ticks / hz;

    char up_buf[32];
    int un = 0;
    up_buf[0] = '\0';
    rail_put_uint(up_buf, &un, sizeof(up_buf), uptime_s);
    up_buf[un++] = 's';
    up_buf[un] = '\0';

    int up_x = exit_x - 70;
    draw_text(up_x, ry + 6, "UP:", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    draw_text(up_x + 24, ry + 6, up_buf, GH_COLOR_GREEN_LEAF, FONT_TRANSPARENT, 0);

    /* RAM Readout */
    pmm_stats_t pmm = pmm_get_stats();
    uint64_t total_mb = pmm.total_memory_bytes / (1024 * 1024);
    uint64_t free_mb = (pmm.free_frames * 4096) / (1024 * 1024);
    uint64_t used_mb = (total_mb >= free_mb) ? (total_mb - free_mb) : 0;

    char ram_buf[32];
    int rn = 0;
    ram_buf[0] = '\0';
    rail_put_uint(ram_buf, &rn, sizeof(ram_buf), used_mb);
    ram_buf[rn++] = '/';
    rail_put_uint(ram_buf, &rn, sizeof(ram_buf), total_mb);
    ram_buf[rn++] = 'M';
    ram_buf[rn] = '\0';

    int ram_x = up_x - 110;
    draw_text(ram_x, ry + 6, "RAM:", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    draw_text(ram_x + 32, ry + 6, ram_buf, GH_COLOR_BLUE_INFO, FONT_TRANSPARENT, 0);

    /* Clock / Ticks Indicator */
    int tick_x = ram_x - 90;
    char tick_buf[32];
    int tn = 0;
    tick_buf[0] = '\0';
    rail_put_uint(tick_buf, &tn, sizeof(tick_buf), ticks);

    draw_text(tick_x, ry + 6, "T:", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    draw_text(tick_x + 16, ry + 6, tick_buf, GH_COLOR_GREEN_LEAF_DEEP, FONT_TRANSPARENT, 0);
}

int rail_handle_event(const input_event_t* ev, int sw, int sh,
                      surface_t* surfaces, int count,
                      int* out_action, uint32_t* out_surface_id) {
    (void)sh;
    if (!ev) return 0;
    if (out_action) *out_action = RAIL_ACTION_NONE;
    if (out_surface_id) *out_surface_id = SURFACE_ID_NONE;

    if (ev->type != INPUT_EVENT_MOUSE_BUTTON_DOWN) return 0;

    int rx = 8;
    int ry = g_rail_current_y;
    int rw = sw - 16;
    int rh = 28;

    if (ry < 0) {
        if (ev->x >= (sw - 120) / 2 && ev->x <= (sw + 120) / 2 && ev->y >= 0 && ev->y <= 12) {
            rail_request_emerge();
            return 1;
        }
        return 0;
    }

    if (ev->x < rx || ev->x >= rx + rw || ev->y < ry || ev->y >= ry + rh) {
        return 0;
    }

    /* Check Berry Button */
    if (ev->x >= rx + 6 && ev->x < rx + 90) {
        if (out_action) *out_action = RAIL_ACTION_BERRY;
        return 1;
    }

    /* Check Apps Button */
    if (ev->x >= rx + 96 && ev->x < rx + 172) {
        if (out_action) *out_action = RAIL_ACTION_LAUNCHER;
        return 1;
    }

    /* Check Text Exit Button */
    int exit_x = rx + rw - 74;
    if (ev->x >= exit_x && ev->x < exit_x + 68) {
        if (out_action) *out_action = RAIL_ACTION_EXIT;
        return 1;
    }

    /* Check Surface Tabs */
    int tab_x = rx + 182;
    for (int i = 0; i < count && i < 4; i++) {
        surface_t* s = &surfaces[i];
        if (ev->x >= tab_x && ev->x < tab_x + 108) {
            if (out_action) *out_action = RAIL_ACTION_SURFACE;
            if (out_surface_id) *out_surface_id = s->id;
            return 1;
        }
        tab_x += 114;
    }

    return 1;
}
