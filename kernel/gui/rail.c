/* ==============================================================================
 * Greenhouse OS - VERDANT System Rail (implementation)
 * ==============================================================================
 */

#include "rail.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "../pmm.h"
#include "../irq.h"

void rail_init(void) {
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
    int rx = 8;
    int ry = 4;
    int rw = sw - 16;
    int rh = 28;

    /* 1. Main Glass Rail Body */
    graphics_fill_glass_panel(rx, ry, rw, rh, 6, 0xFF0F1C2E, 0xFF08101A, 0xFF1E3550, 0xFF38BDF8);

    /* 2. Left Buttons */
    /* [ Berry ] Button */
    graphics_fill_rounded_rect(rx + 6, ry + 3, 84, 22, 4, 0xFF1F1122);
    graphics_draw_rounded_rect(rx + 6, ry + 3, 84, 22, 4, 0xFFF43F5E);
    draw_text(rx + 12, ry + 6, "[*] Berry", 0xFFFB7185, FONT_TRANSPARENT, 0);

    /* [ Apps ] Button */
    graphics_fill_rounded_rect(rx + 96, ry + 3, 76, 22, 4, 0xFF0B251F);
    graphics_draw_rounded_rect(rx + 96, ry + 3, 76, 22, 4, 0xFF10B981);
    draw_text(rx + 102, ry + 6, "[+] Apps", 0xFF6EE7B7, FONT_TRANSPARENT, 0);

    /* 3. Middle Surface Switcher Tabs */
    int tab_x = rx + 182;
    for (int i = 0; i < count && i < 4; i++) {
        surface_t* s = &surfaces[i];
        if (!s->visible && !s->is_minimized) continue;

        int is_focus = (s->id == (uint32_t)focused_id);
        uint32_t tab_bg = is_focus ? 0xFF162E25 : (s->is_minimized ? 0xFF0D1420 : 0xFF121E30);
        uint32_t tab_border = is_focus ? s->accent : 0xFF1E3A5F;

        graphics_fill_rounded_rect(tab_x, ry + 3, 108, 22, 4, tab_bg);
        graphics_draw_rounded_rect(tab_x, ry + 3, 108, 22, 4, tab_border);

        /* Tag pill inside tab */
        draw_text_clipped(tab_x + 6, ry + 6, 96, s->title, is_focus ? 0xFFFFFFFF : 0xFF94A3B8, FONT_TRANSPARENT, 0);
        tab_x += 114;
    }

    /* 4. Right Status Chips */
    /* [ Text Mode ] Exit button */
    int exit_x = rx + rw - 74;
    graphics_fill_rounded_rect(exit_x, ry + 3, 68, 22, 4, 0xFF2A1215);
    graphics_draw_rounded_rect(exit_x, ry + 3, 68, 22, 4, 0xFFEF4444);
    draw_text(exit_x + 8, ry + 6, "Text [X]", 0xFFFCA5A5, FONT_TRANSPARENT, 0);

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
    draw_text(up_x, ry + 6, "UP:", 0xFF64748B, FONT_TRANSPARENT, 0);
    draw_text(up_x + 24, ry + 6, up_buf, 0xFF38BDF8, FONT_TRANSPARENT, 0);

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
    draw_text(ram_x, ry + 6, "RAM:", 0xFF64748B, FONT_TRANSPARENT, 0);
    draw_text(ram_x + 32, ry + 6, ram_buf, 0xFF34D399, FONT_TRANSPARENT, 0);

    /* Clock / Ticks Indicator */
    int tick_x = ram_x - 90;
    char tick_buf[32];
    int tn = 0;
    tick_buf[0] = '\0';
    rail_put_uint(tick_buf, &tn, sizeof(tick_buf), ticks);

    draw_text(tick_x, ry + 6, "T:", 0xFF64748B, FONT_TRANSPARENT, 0);
    draw_text(tick_x + 16, ry + 6, tick_buf, 0xFFFBBF24, FONT_TRANSPARENT, 0);
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
    int ry = 4;
    int rw = sw - 16;
    int rh = 28;

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
