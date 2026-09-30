/* ==============================================================================
 * Greenhouse OS — Modern Desktop Shell (Implementation)
 * ==============================================================================
 * Synthesizes macOS menubar elegance with Windows taskbar & system tray
 * practicality into a refined botanical Greenhouse interface.
 * ==============================================================================
 */

#include "shell.h"
#include "renderer.h"
#include "compositor.h"
#include "gh_theme.h"
#include "../pmm.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"

extern uint64_t timer_get_ticks(void);

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
} shell_rtc_dt_t;

extern void rtc_get_datetime(shell_rtc_dt_t* dt);

static const shell_dock_item_t dock_items[SHELL_DOCK_ITEMS] = {
    { "Terminal", ">_", "F3", GH_COLOR_GREEN_LEAF,     1 },
    { "Files",    "FL", "F4", GH_COLOR_BLUE_INFO,       2 },
    { "Monitor",  "SY", "3",  GH_COLOR_AMBER_WARN,      3 },
    { "Berry",    "*",  "F2", GH_BERRY_ACCENT,          4 },
    { "Canvas",   "[]", "5",  GH_COLOR_GREEN_LEAF_DEEP, 5 },
    { "Settings", "CF", "6",  GH_COLOR_TEXT_SECONDARY,  6 }
};

void shell_init(void) {
    /* Shell initialized */
}

const shell_dock_item_t* shell_get_dock_item(int index) {
    if (index < 0 || index >= SHELL_DOCK_ITEMS) return NULL;
    return &dock_items[index];
}

static void shell_format_time(char* buf, int cap) {
    if (!buf || cap < 6) return;
    shell_rtc_dt_t dt;
    dt.hour = 0; dt.minute = 0; dt.second = 0;
    rtc_get_datetime(&dt);
    if (dt.hour > 23 || dt.minute > 59) {
        uint64_t ticks = timer_get_ticks();
        uint64_t sec = ticks / 100;
        dt.hour = (sec / 3600) % 24;
        dt.minute = (sec / 60) % 60;
    }
    buf[0] = (char)('0' + ((dt.hour / 10) % 10));
    buf[1] = (char)('0' + (dt.hour % 10));
    buf[2] = ':';
    buf[3] = (char)('0' + ((dt.minute / 10) % 10));
    buf[4] = (char)('0' + (dt.minute % 10));
    buf[5] = '\0';
}

static void shell_format_ram(char* buf, int cap) {
    if (!buf || cap < 8) return;
    pmm_stats_t st = pmm_get_stats();
    uint64_t used_mb = (st.used_frames * 4096) / (1024 * 1024);
    if (used_mb == 0) used_mb = 1;
    buf[0] = 'R'; buf[1] = 'A'; buf[2] = 'M'; buf[3] = ' ';
    int pos = 4;
    char tmp[16]; int tl = 0;
    do { tmp[tl++] = (char)('0' + (used_mb % 10)); used_mb /= 10; } while (used_mb && tl < 15);
    while (tl && pos < cap - 2) buf[pos++] = tmp[--tl];
    buf[pos++] = 'M';
    buf[pos] = '\0';
}

void shell_draw_topbar(surface_t* active_surface, surface_t** all_surfaces, int surface_count) {
    if (!graphics_is_active()) return;
    int sw = graphics_get_width();
    int tb_x = SHELL_TOPBAR_PAD;
    int tb_y = SHELL_TOPBAR_Y;
    int tb_w = sw - (SHELL_TOPBAR_PAD * 2);
    int tb_h = SHELL_TOPBAR_H;

    /* 1. Frosted Glass Top Bar Panel with soft drop shadow */
    renderer_draw_card_panel(tb_x, tb_y, tb_w, tb_h, 8, GH_COLOR_SURFACE, 235, GH_COLOR_BORDER_LIGHT, 2);

    /* 2. Left Section: Greenhouse Seed Brand Button (Windows Start / macOS Apple Menu) */
    int cur_x = tb_x + 8;
    int brand_w = 138;
    int brand_h = tb_h - 8;
    int brand_y = tb_y + 4;

    /* Brand pill card with subtle hover accent */
    renderer_fill_aa_rounded_rect(cur_x, brand_y, brand_w, brand_h, 6, GH_COLOR_SURFACE_HOVER, 240);
    renderer_draw_aa_rounded_rect(cur_x, brand_y, brand_w, brand_h, 6, 1, GH_COLOR_GREEN_LEAF, 200);

    /* Seed leaf emblem badge */
    renderer_fill_aa_rounded_rect(cur_x + 4, brand_y + 3, 22, 18, 4, GH_COLOR_GREEN_LEAF, 255);
    draw_text(cur_x + 6, brand_y + 4, "GH", GH_COLOR_WHITE, FONT_TRANSPARENT, 0);

    /* Seed text and shortcut badge */
    draw_text(cur_x + 30, brand_y + 4, "Seed", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
    renderer_fill_aa_rounded_rect(cur_x + 68, brand_y + 4, 46, 16, 4, GH_COLOR_GREEN_PILL_BG, 255);
    draw_text(cur_x + 72, brand_y + 4, "[Win]", GH_COLOR_GREEN_PILL_FG, FONT_TRANSPARENT, 0);
    cur_x += brand_w + 12;

    /* 3. Center Section: Surface Switcher Tabs (Windows Taskbar Convention) */
    int max_tab_x = tb_x + tb_w - 228; /* Reserve space for system tray */
    for (int i = 0; i < surface_count; i++) {
        surface_t* s = all_surfaces[i];
        if (!s || !s->visible) continue;

        int is_act = (s == active_surface);
        int tab_w = font_text_width(s->title) + 28;
        if (tab_w < 78) tab_w = 78;
        if (cur_x + tab_w > max_tab_x) break;

        uint32_t bg = is_act ? GH_COLOR_SURFACE_HOVER : GH_COLOR_SURFACE;
        uint32_t border = is_act ? GH_COLOR_GREEN_LEAF : GH_COLOR_BORDER_LIGHT;
        uint32_t fg = is_act ? GH_COLOR_TEXT_PRIMARY : (s->is_minimized ? GH_COLOR_TEXT_MUTED : GH_COLOR_TEXT_SECONDARY);

        /* Tab Card */
        renderer_fill_aa_rounded_rect(cur_x, brand_y, tab_w, brand_h, 6, bg, is_act ? 245 : 180);
        renderer_draw_aa_rounded_rect(cur_x, brand_y, tab_w, brand_h, 6, 1, border, is_act ? 240 : 150);

        /* Status Dot */
        uint32_t dot_color = is_act ? GH_COLOR_GREEN_SPROUT : (s->is_minimized ? GH_COLOR_TEXT_FAINT : GH_COLOR_TEXT_SECONDARY);
        renderer_fill_aa_circle(cur_x + 8, brand_y + 12, 3, dot_color, 255);

        /* Title */
        draw_text(cur_x + 16, brand_y + 4, s->title, fg, FONT_TRANSPARENT, 0);

        cur_x += tab_w + 6;
    }

    /* 4. Right Section: System Tray (Windows Tray practicality + macOS cleanliness) */
    int tray_right = tb_x + tb_w - 8;

    /* 4a. Logout Exit Pill */
    int con_w = 68;
    int con_x = tray_right - con_w;
    renderer_draw_button(con_x, brand_y, con_w, brand_h, "Logout",
                         GH_COLOR_SURFACE_HOVER, GH_COLOR_BORDER_LIGHT, GH_COLOR_TEXT_PRIMARY,
                         0, 0, 0);


    /* 4b. Clock Pill */
    char time_str[8];
    shell_format_time(time_str, sizeof(time_str));
    int clk_w = 54;
    int clk_x = con_x - 6 - clk_w;
    renderer_fill_aa_rounded_rect(clk_x, brand_y, clk_w, brand_h, 6, GH_COLOR_SURFACE_HOVER, 180);
    renderer_draw_aa_rounded_rect(clk_x, brand_y, clk_w, brand_h, 6, 1, GH_COLOR_BORDER_LIGHT, 150);
    draw_text(clk_x + 8, brand_y + 4, time_str, GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);

    /* 4c. RAM Telemetry Pill */
    char ram_str[16];
    shell_format_ram(ram_str, sizeof(ram_str));
    int ram_w = 76;
    int ram_x = clk_x - 6 - ram_w;
    renderer_fill_aa_rounded_rect(ram_x, brand_y, ram_w, brand_h, 6, GH_COLOR_SURFACE_HOVER, 180);
    renderer_draw_aa_rounded_rect(ram_x, brand_y, ram_w, brand_h, 6, 1, GH_COLOR_BORDER_LIGHT, 150);
    renderer_fill_aa_circle(ram_x + 8, brand_y + 12, 3, GH_COLOR_GREEN_LEAF, 255);
    draw_text(ram_x + 16, brand_y + 4, ram_str, GH_COLOR_TEXT_SECONDARY, FONT_TRANSPARENT, 0);
}

static void shell_draw_dock_icon(int cx, int cy, int item_index, uint32_t col) {
    switch (item_index) {
        case 0: { /* Terminal: modern prompt '>' + cursor '_' */
            int px = cx - 7;
            int py = cy - 4;
            for (int k = 0; k < 5; k++) {
                renderer_blend_pixel(px + k, py + k, col, 255);
                renderer_blend_pixel(px + k + 1, py + k, col, 200);
                renderer_blend_pixel(px + k, py + 8 - k, col, 255);
                renderer_blend_pixel(px + k + 1, py + 8 - k, col, 200);
            }
            renderer_fill_alpha_rect(px + 7, py + 7, 7, 2, col, 255);
            break;
        }
        case 1: { /* Files: clean folder */
            int fx = cx - 8;
            int fy = cy - 6;
            renderer_fill_alpha_rect(fx, fy, 6, 2, col, 255);
            renderer_fill_aa_rounded_rect(fx, fy + 2, 16, 10, 2, col, 255);
            renderer_fill_alpha_rect(fx + 2, fy + 4, 12, 1, 0xFFFFFFFF, 170);
            break;
        }
        case 2: { /* Monitor: activity bars */
            int mx = cx - 7;
            int my = cy + 5;
            renderer_fill_aa_rounded_rect(mx, my - 6, 3, 6, 1, col, 255);
            renderer_fill_aa_rounded_rect(mx + 5, my - 11, 3, 11, 1, col, 255);
            renderer_fill_aa_rounded_rect(mx + 10, my - 8, 3, 8, 1, col, 255);
            break;
        }
        case 3: { /* Berry: sparkling 4-point star rosette */
            renderer_fill_aa_circle(cx, cy, 3, col, 255);
            renderer_blend_pixel(cx, cy - 5, col, 255);
            renderer_blend_pixel(cx, cy + 5, col, 255);
            renderer_blend_pixel(cx - 5, cy, col, 255);
            renderer_blend_pixel(cx + 5, cy, col, 255);
            renderer_blend_pixel(cx + 3, cy - 4, GH_COLOR_GREEN_LEAF, 255);
            renderer_blend_pixel(cx + 4, cy - 3, GH_COLOR_GREEN_LEAF, 255);
            break;
        }
        case 4: { /* Canvas: framed palette with paint swatch */
            int kx = cx - 8;
            int ky = cy - 6;
            renderer_draw_aa_rounded_rect(kx, ky, 16, 12, 2, 1, col, 255);
            renderer_fill_aa_circle(kx + 11, ky + 4, 2, GH_COLOR_AMBER_WARN, 255);
            for (int r = 0; r < 4; r++) {
                renderer_fill_alpha_rect(kx + 3 + r, ky + 8 - r, (4 - r) * 2, 1, col, 220);
            }
            break;
        }
        case 5: { /* Settings: macOS/Windows control center slider tracks */
            int sx = cx - 7;
            renderer_fill_alpha_rect(sx, cy - 4, 14, 2, col, 160);
            renderer_fill_aa_rounded_rect(sx + 3, cy - 6, 4, 6, 2, col, 255);
            renderer_fill_alpha_rect(sx, cy + 3, 14, 2, col, 160);
            renderer_fill_aa_rounded_rect(sx + 8, cy + 1, 4, 6, 2, col, 255);
            break;
        }
        default: {
            int glyph_w = font_text_width(dock_items[item_index].icon);
            int gx = cx - glyph_w / 2;
            draw_text(gx, cy - 4, dock_items[item_index].icon, col, FONT_TRANSPARENT, 0);
            break;
        }
    }
}

void shell_draw_dock(int hover_item, surface_t** all_surfaces, int surface_count) {
    if (!graphics_is_active()) return;
    int sw = graphics_get_width();

    int item_w = 52;
    int dock_w = (item_w * SHELL_DOCK_ITEMS) + 24;
    int dock_x = (sw - dock_w) / 2;
    int dock_y = SHELL_DOCK_Y;
    int dock_h = SHELL_DOCK_H;

    /* 1. Floating Dock Card Panel */
    renderer_draw_card_panel(dock_x, dock_y, dock_w, dock_h, 16, GH_COLOR_SURFACE, 240, GH_COLOR_BORDER_LIGHT, 3);

    /* 2. Dock Application Tiles */
    for (int i = 0; i < SHELL_DOCK_ITEMS; i++) {
        int ix = dock_x + 12 + i * item_w;
        int iy = dock_y + 6;
        int is_hov = (i == hover_item);

        /* Check if app surface is currently running */
        int is_open = 0;
        for (int s = 0; s < surface_count; s++) {
            if (all_surfaces[s] && all_surfaces[s]->id == dock_items[i].surface_id && all_surfaces[s]->visible) {
                is_open = 1;
                break;
            }
        }

        /* Hover lift effect: lift by 2px when hovered */
        int tile_y = is_hov ? (iy - 2) : iy;
        int tile_w = item_w - 12;
        int tile_h = 32;

        /* App Tile Background */
        uint32_t tile_bg = is_hov ? GH_COLOR_SURFACE_HOVER : GH_COLOR_SURFACE;
        uint32_t tile_border = is_hov ? GH_COLOR_GREEN_LEAF : GH_COLOR_BORDER_LIGHT;
        uint32_t glyph_fg = is_hov ? GH_COLOR_GREEN_SPROUT : dock_items[i].color;

        renderer_fill_aa_rounded_rect(ix + 6, tile_y, tile_w, tile_h, 8, tile_bg, is_hov ? 245 : 200);
        renderer_draw_aa_rounded_rect(ix + 6, tile_y, tile_w, tile_h, 8, 1, tile_border, is_hov ? 255 : 160);

        /* Draw Vector Micro-Icon */
        shell_draw_dock_icon(ix + 6 + (tile_w / 2), tile_y + (tile_h / 2), i, glyph_fg);

        /* Running Indicator Dot */
        if (is_open) {
            renderer_fill_aa_circle(ix + 6 + (tile_w / 2), dock_y + dock_h - 5, 2, GH_COLOR_GREEN_SPROUT, 255);
        }

        /* Tooltip when hovered */
        if (is_hov) {
            int tt_w = font_text_width(dock_items[i].name) + 16;
            int tt_h = 20;
            int tt_x = ix + 6 + (tile_w - tt_w) / 2;
            int tt_y = dock_y - 26;

            renderer_draw_card_panel(tt_x, tt_y, tt_w, tt_h, 6, GH_COLOR_SURFACE, 250, GH_COLOR_BORDER_LIGHT, 1);
            draw_text(tt_x + 8, tt_y + 2, dock_items[i].name, GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
        }
    }
}

int shell_topbar_hit_test(int x, int y, surface_t** all_surfaces, int surface_count, int* out_tab_index) {
    if (out_tab_index) *out_tab_index = -1;
    if (y < SHELL_TOPBAR_Y || y >= SHELL_TOPBAR_Y + SHELL_TOPBAR_H) return SHELL_HIT_NONE;

    int sw = graphics_get_width();
    int tb_x = SHELL_TOPBAR_PAD;
    int tb_w = sw - (SHELL_TOPBAR_PAD * 2);

    if (x < tb_x || x >= tb_x + tb_w) return SHELL_HIT_NONE;

    /* 1. Left Brand Pill */
    int brand_x = tb_x + 8;
    int brand_w = 138;
    if (x >= brand_x && x <= brand_x + brand_w) {
        return SHELL_HIT_BRAND;
    }

    /* 2. Right System Tray */
    int tray_right = tb_x + tb_w - 8;

    /* Console button */
    int con_w = 68;
    int con_x = tray_right - con_w;
    if (x >= con_x && x <= con_x + con_w) {
        return SHELL_HIT_CONSOLE;
    }

    /* Clock */
    int clk_w = 54;
    int clk_x = con_x - 6 - clk_w;
    if (x >= clk_x && x <= clk_x + clk_w) {
        return SHELL_HIT_BACKGROUND;
    }

    /* RAM Telemetry (clicking opens SysMon) */
    int ram_w = 76;
    int ram_x = clk_x - 6 - ram_w;
    if (x >= ram_x && x <= ram_x + ram_w) {
        return SHELL_HIT_SYSMON;
    }

    /* 3. Surface Switcher Tabs */
    if (all_surfaces && surface_count > 0) {
        int cur_x = tb_x + 8 + 138 + 12;
        int max_tab_x = tb_x + tb_w - 228;

        for (int i = 0; i < surface_count; i++) {
            surface_t* s = all_surfaces[i];
            if (!s || !s->visible) continue;

            int tab_w = font_text_width(s->title) + 28;
            if (tab_w < 78) tab_w = 78;
            if (cur_x + tab_w > max_tab_x) break;

            if (x >= cur_x && x <= cur_x + tab_w) {
                if (out_tab_index) *out_tab_index = i;
                return SHELL_HIT_TAB;
            }
            cur_x += tab_w + 6;
        }
    }

    return SHELL_HIT_BACKGROUND;
}

int shell_dock_hit_test(int x, int y) {
    if (y < SHELL_DOCK_Y || y >= SHELL_DOCK_Y + SHELL_DOCK_H) return -1;

    int sw = graphics_get_width();
    int item_w = 52;
    int dock_w = (item_w * SHELL_DOCK_ITEMS) + 24;
    int dock_x = (sw - dock_w) / 2;

    if (x < dock_x + 12 || x >= dock_x + dock_w - 12) return -1;

    int idx = (x - (dock_x + 12)) / item_w;
    if (idx >= 0 && idx < SHELL_DOCK_ITEMS) return idx;
    return -1;
}
