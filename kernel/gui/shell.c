/* ==============================================================================
 * Greenhouse OS — Modern Desktop Shell (Implementation)
 * ==============================================================================
 */

#include "shell.h"
#include "renderer.h"
#include "compositor.h"
#include "../pmm.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"

extern uint64_t timer_get_ticks(void);

static const shell_dock_item_t dock_items[SHELL_DOCK_ITEMS] = {
    { "Terminal", "TERM", "F3", GFX_COLOR_EMERALD_PRIMARY, 1 },
    { "Files",    "FILE", "F4", GFX_COLOR_CYAN_ACCENT,     2 },
    { "Monitor",  "MON",  "3",  GFX_COLOR_AMBER_WARN,      3 },
    { "Berry",    "BERY", "F2", GFX_COLOR_BERRY_ACCENT,    4 },
    { "Canvas",   "CANV", "5",  GFX_COLOR_MINT_ACCENT,     5 },
    { "Settings", "SET",  "6",  GFX_COLOR_TEXT_SECONDARY,  6 }
};

void shell_init(void) {
    /* Shell init */
}

const shell_dock_item_t* shell_get_dock_item(int index) {
    if (index < 0 || index >= SHELL_DOCK_ITEMS) return NULL;
    return &dock_items[index];
}

void shell_draw_topbar(surface_t* active_surface, surface_t** all_surfaces, int surface_count) {
    if (!graphics_is_active()) return;
    int sw = graphics_get_width();
    int tb_x = SHELL_TOPBAR_PAD;
    int tb_y = SHELL_TOPBAR_Y;
    int tb_w = sw - (SHELL_TOPBAR_PAD * 2);
    int tb_h = SHELL_TOPBAR_H;

    /* 1. Top Bar Glass Panel */
    renderer_draw_glass_panel(tb_x, tb_y, tb_w, tb_h, 8, GFX_COLOR_GLASS_SURFACE, 210, GFX_COLOR_GLASS_BORDER, 2);

    /* 2. Left Brand Badge */
    int cur_x = tb_x + 10;
    renderer_draw_pill_button(cur_x, tb_y + 4, 110, tb_h - 8, "Greenhouse", 0xFF143029, GFX_COLOR_EMERALD_PRIMARY, GFX_COLOR_MINT_ACCENT, 1);
    cur_x += 118;

    /* 3. Surface Switcher Tabs */
    for (int i = 0; i < surface_count; i++) {
        surface_t* s = all_surfaces[i];
        if (!s || !s->visible) continue;

        int is_act = (s == active_surface);
        int tab_w = font_text_width(s->title) + 20;
        if (tab_w < 70) tab_w = 70;
        if (cur_x + tab_w + 260 > tb_x + tb_w) break; /* Avoid encroaching on right telemetry */

        uint32_t bg = is_act ? 0xFF1E3A33 : 0xFF17252F;
        uint32_t border = is_act ? GFX_COLOR_EMERALD_PRIMARY : GFX_COLOR_GLASS_BORDER;
        uint32_t fg = is_act ? GFX_COLOR_TEXT_PRIMARY : GFX_COLOR_TEXT_SECONDARY;

        renderer_draw_pill_button(cur_x, tb_y + 4, tab_w, tb_h - 8, s->title, bg, border, fg, is_act);
        cur_x += tab_w + 6;
    }

    /* 4. Right Telemetry & Status Group */
    int right_x = tb_x + tb_w - 8;

    /* Exit / Console button */
    int btn_w = 74;
    right_x -= btn_w;
    renderer_draw_pill_button(right_x, tb_y + 4, btn_w, tb_h - 8, "Console", 0xFF2A1F2D, 0xFFF43F5E, 0xFFFB7185, 0);

    /* RAM telemetry */
    pmm_stats_t pmm = pmm_get_stats();
    uint64_t used_mb = (pmm.used_frames * 4096) / (1024 * 1024);
    uint64_t total_mb = (pmm.total_frames * 4096) / (1024 * 1024);

    char ram_buf[24];
    /* Simple formatter */
    ram_buf[0] = 'R'; ram_buf[1] = 'A'; ram_buf[2] = 'M'; ram_buf[3] = ' ';
    int idx = 4;
    if (used_mb >= 100) ram_buf[idx++] = (char)('0' + ((used_mb / 100) % 10));
    if (used_mb >= 10)  ram_buf[idx++] = (char)('0' + ((used_mb / 10) % 10));
    ram_buf[idx++] = (char)('0' + (used_mb % 10));
    ram_buf[idx++] = 'M'; ram_buf[idx++] = '/';
    if (total_mb >= 100) ram_buf[idx++] = (char)('0' + ((total_mb / 100) % 10));
    if (total_mb >= 10)  ram_buf[idx++] = (char)('0' + ((total_mb / 10) % 10));
    ram_buf[idx++] = (char)('0' + (total_mb % 10));
    ram_buf[idx++] = 'M';
    ram_buf[idx] = '\0';

    int ram_w = font_text_width(ram_buf) + 16;
    right_x -= (ram_w + 6);
    renderer_draw_pill_button(right_x, tb_y + 4, ram_w, tb_h - 8, ram_buf, 0xFF14242F, GFX_COLOR_GLASS_BORDER, GFX_COLOR_MINT_ACCENT, 0);

    /* Uptime / Ticks */
    uint64_t ticks = timer_get_ticks();
    uint64_t secs = ticks / 100; /* Approximate 100 Hz */
    uint64_t mins = secs / 60;
    secs %= 60;

    char up_buf[16];
    up_buf[0] = 'U'; up_buf[1] = 'p'; up_buf[2] = ' ';
    up_buf[3] = (char)('0' + ((mins / 10) % 10));
    up_buf[4] = (char)('0' + (mins % 10));
    up_buf[5] = ':';
    up_buf[6] = (char)('0' + ((secs / 10) % 10));
    up_buf[7] = (char)('0' + (secs % 10));
    up_buf[8] = '\0';

    int up_w = font_text_width(up_buf) + 16;
    right_x -= (up_w + 6);
    renderer_draw_pill_button(right_x, tb_y + 4, up_w, tb_h - 8, up_buf, 0xFF14242F, GFX_COLOR_GLASS_BORDER, GFX_COLOR_TEXT_SECONDARY, 0);
}

void shell_draw_dock(int hover_item, surface_t** all_surfaces, int surface_count) {
    if (!graphics_is_active()) return;
    int sw = graphics_get_width();

    int item_w = 54;
    int dock_w = (item_w * SHELL_DOCK_ITEMS) + 20;
    int dock_x = (sw - dock_w) / 2;
    int dock_y = SHELL_DOCK_Y;
    int dock_h = SHELL_DOCK_H;

    /* 1. Floating Dock Glass Body */
    renderer_draw_glass_panel(dock_x, dock_y, dock_w, dock_h, 16, GFX_COLOR_GLASS_SURFACE_ACT, 230, GFX_COLOR_GLASS_BORDER_ACT, 4);

    /* 2. Dock Application Items */
    for (int i = 0; i < SHELL_DOCK_ITEMS; i++) {
        int ix = dock_x + 10 + i * item_w;
        int iy = dock_y + 4;
        int is_hov = (i == hover_item);

        /* Check if open */
        int is_open = 0;
        for (int s = 0; s < surface_count; s++) {
            if (all_surfaces[s] && all_surfaces[s]->id == dock_items[i].surface_id && all_surfaces[s]->visible) {
                is_open = 1;
                break;
            }
        }

        /* Hover / Active Pill background */
        if (is_hov) {
            renderer_fill_alpha_rounded_rect(ix + 2, iy + 2, item_w - 4, dock_h - 12, 8, dock_items[i].color, 70);
            graphics_draw_rounded_rect(ix + 2, iy + 2, item_w - 4, dock_h - 12, 8, dock_items[i].color);
        }

        /* App Icon Badge */
        uint32_t icon_bg = is_hov ? dock_items[i].color : 0xFF192F3B;
        uint32_t icon_fg = is_hov ? 0xFFFFFFFF : dock_items[i].color;
        renderer_draw_badge(ix + 6, iy + 4, dock_items[i].icon, icon_bg, icon_fg);

        /* Running Indicator Dot */
        if (is_open) {
            renderer_fill_alpha_rounded_rect(ix + (item_w / 2) - 2, dock_y + dock_h - 6, 4, 4, 2, GFX_COLOR_EMERALD_PRIMARY, 255);
        }
    }
}

int shell_topbar_hit_test(int x, int y, int* out_tab_index) {
    if (out_tab_index) *out_tab_index = -1;
    if (y < SHELL_TOPBAR_Y || y >= SHELL_TOPBAR_Y + SHELL_TOPBAR_H) return 0;

    int sw = graphics_get_width();
    int tb_x = SHELL_TOPBAR_PAD;
    int tb_w = sw - (SHELL_TOPBAR_PAD * 2);

    if (x < tb_x || x >= tb_x + tb_w) return 0;

    /* Check Console Exit button */
    int btn_w = 74;
    int btn_x = tb_x + tb_w - 8 - btn_w;
    if (x >= btn_x && x <= btn_x + btn_w) {
        return 999; /* Console Exit clicked */
    }

    return 1; /* Clicked topbar */
}

int shell_dock_hit_test(int x, int y) {
    if (y < SHELL_DOCK_Y || y >= SHELL_DOCK_Y + SHELL_DOCK_H) return -1;

    int sw = graphics_get_width();
    int item_w = 54;
    int dock_w = (item_w * SHELL_DOCK_ITEMS) + 20;
    int dock_x = (sw - dock_w) / 2;

    if (x < dock_x + 10 || x >= dock_x + dock_w - 10) return -1;

    int idx = (x - (dock_x + 10)) / item_w;
    if (idx >= 0 && idx < SHELL_DOCK_ITEMS) return idx;
    return -1;
}
