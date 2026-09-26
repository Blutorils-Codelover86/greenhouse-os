/* ==============================================================================
 * Greenhouse OS - VERDANT Radial Launcher (implementation)
 * ==============================================================================
 */

#include "launcher.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"

static int g_launcher_visible = 0;
static int g_selected_index = 0;

static launcher_node_t g_nodes[LAUNCHER_NODE_COUNT] = {
    { LAUNCHER_APP_TERMINAL, '1', "Terminal",       "TERM",  "Greenhouse interactive shell and CLI",       0xFF10B981,    0, -135, 140, 44 },
    { LAUNCHER_APP_FILES,    '2', "File Browser",   "FILES", "Browse FAT32 (C:) and RAMFS (R:) drives",    0xFF38BDF8,  145,  -65, 150, 44 },
    { LAUNCHER_APP_SYSMON,   '3', "System Monitor", "SYS",   "Real-time CPU, RAM, heap, and processes",    0xFFF59E0B,  145,   65, 155, 44 },
    { LAUNCHER_APP_BERRY,    '4', "Berry AI",       "BERRY", "Interactive system assistant and voice AI",  0xFFF43F5E,    0,  135, 150, 44 },
    { LAUNCHER_APP_CANVAS,   '5', "GFX Canvas",     "GFX",   "2D graphics demonstration and primitives",   0xFFA855F7, -145,   65, 145, 44 },
    { LAUNCHER_APP_SETTINGS, '6', "System Info",    "INFO",  "Display properties and compositor report",   0xFF34D399, -145,  -65, 145, 44 }
};

void launcher_init(void) {
    g_launcher_visible = 0;
    g_selected_index = 0;
}

void launcher_show(void) {
    g_launcher_visible = 1;
}

void launcher_hide(void) {
    g_launcher_visible = 0;
}

void launcher_toggle(void) {
    g_launcher_visible = !g_launcher_visible;
}

int launcher_is_visible(void) {
    return g_launcher_visible;
}

void launcher_draw(int sw, int sh) {
    if (!g_launcher_visible) return;

    int cx = sw / 2;
    int cy = sh / 2 + 10;

    /* 1. Backdrop overlay scrim */
    for (int y = 0; y < sh; y += 2) {
        graphics_draw_hline(0, y, sw, 0xFF03070E);
    }

    /* 2. Orbital Guides */
    graphics_draw_circle(cx, cy, 145, 0xFF0E2238);
    graphics_draw_circle(cx, cy, 146, 0xFF091624);

    /* 3. Central Hub */
    int hub_r = 54;
    graphics_fill_circle(cx, cy, hub_r, 0xFF0A1220);
    graphics_draw_circle(cx, cy, hub_r, 0xFF10B981);
    graphics_draw_circle(cx, cy, hub_r - 2, 0xFF064E3B);

    /* Central Berry Core badge */
    draw_text_centered(cx - 40, cy - 24, 80, "[ BERRY ]", 0xFFF43F5E, FONT_TRANSPARENT, 1);
    draw_text_centered(cx - 48, cy - 4, 96, "VERDANT", 0xFFE2E8F0, FONT_TRANSPARENT, 1);
    draw_text_centered(cx - 40, cy + 14, 80, "LAUNCHER", 0xFF10B981, FONT_TRANSPARENT, 0);

    /* 4. Draw Radial Nodes */
    for (int i = 0; i < LAUNCHER_NODE_COUNT; i++) {
        launcher_node_t* node = &g_nodes[i];
        int nx = cx + node->rel_x - node->w / 2;
        int ny = cy + node->rel_y - node->h / 2;
        int selected = (i == g_selected_index);

        /* Connecting spoke */
        graphics_draw_line(cx, cy, cx + node->rel_x, cy + node->rel_y, selected ? node->accent : 0xFF162A40);

        /* Node Box */
        uint32_t bg = selected ? 0xFF132236 : 0xFF0B1422;
        uint32_t border = selected ? node->accent : 0xFF1E334D;
        graphics_fill_rounded_rect(nx, ny, node->w, node->h, 6, bg);
        graphics_draw_rounded_rect(nx, ny, node->w, node->h, 6, border);
        if (selected) {
            graphics_draw_rounded_rect(nx - 1, ny - 1, node->w + 2, node->h + 2, 7, 0xFF0E3A2F);
        }

        /* Number Key Chip */
        char key_str[4];
        key_str[0] = '[';
        key_str[1] = node->key;
        key_str[2] = ']';
        key_str[3] = '\0';
        draw_text(nx + 8, ny + 8, key_str, selected ? node->accent : 0xFF64748B, FONT_TRANSPARENT, 0);

        /* Node Title */
        draw_text(nx + 36, ny + 8, node->title, selected ? 0xFFFFFFFF : 0xFFCBD5E1, FONT_TRANSPARENT, 1);

        /* Node Tag Pill */
        draw_text(nx + 36, ny + 24, node->tag, selected ? node->accent : 0xFF94A3B8, FONT_TRANSPARENT, 0);
    }

    /* 5. Bottom Description Bar */
    launcher_node_t* sel = &g_nodes[g_selected_index];
    int bar_w = 420;
    int bar_h = 42;
    int bar_x = (sw - bar_w) / 2;
    int bar_y = sh - 56;
    graphics_fill_rounded_rect(bar_x, bar_y, bar_w, bar_h, 6, 0xFF0B1320);
    graphics_draw_rounded_rect(bar_x, bar_y, bar_w, bar_h, 6, sel->accent);
    draw_text_centered(bar_x, bar_y + 6, bar_w, sel->desc, 0xFFE2E8F0, FONT_TRANSPARENT, 1);
    draw_text_centered(bar_x, bar_y + 22, bar_w, "Press 1-6 / Enter to Launch  *  ESC to Close", 0xFF64748B, FONT_TRANSPARENT, 0);
}

int launcher_handle_event(const input_event_t* ev, int sw, int sh, int* out_launch_app) {
    if (!g_launcher_visible || !ev) return 0;
    if (out_launch_app) *out_launch_app = LAUNCHER_APP_NONE;

    int cx = sw / 2;
    int cy = sh / 2 + 10;

    /* Keyboard navigation */
    if (ev->type == INPUT_EVENT_KEY_DOWN) {
        if (ev->ascii == 27) { /* ESC */
            launcher_hide();
            return 1;
        }

        if (ev->ascii >= '1' && ev->ascii <= '6') {
            int idx = ev->ascii - '1';
            if (idx >= 0 && idx < LAUNCHER_NODE_COUNT) {
                if (out_launch_app) *out_launch_app = g_nodes[idx].app_id;
                launcher_hide();
                return 1;
            }
        }

        if (ev->ascii == '\r' || ev->ascii == '\n' || ev->ascii == ' ') {
            if (out_launch_app) *out_launch_app = g_nodes[g_selected_index].app_id;
            launcher_hide();
            return 1;
        }

        if (ev->extended) {
            if (ev->ascii == INPUT_KEY_UP || ev->ascii == INPUT_KEY_LEFT) {
                g_selected_index = (g_selected_index + LAUNCHER_NODE_COUNT - 1) % LAUNCHER_NODE_COUNT;
                return 1;
            } else if (ev->ascii == INPUT_KEY_DOWN || ev->ascii == INPUT_KEY_RIGHT) {
                g_selected_index = (g_selected_index + 1) % LAUNCHER_NODE_COUNT;
                return 1;
            }
        }
    }

    /* Mouse movement & hover */
    if (ev->type == INPUT_EVENT_MOUSE_MOVE) {
        for (int i = 0; i < LAUNCHER_NODE_COUNT; i++) {
            launcher_node_t* node = &g_nodes[i];
            int nx = cx + node->rel_x - node->w / 2;
            int ny = cy + node->rel_y - node->h / 2;
            if (ev->x >= nx && ev->x < nx + node->w && ev->y >= ny && ev->y < ny + node->h) {
                g_selected_index = i;
                return 1;
            }
        }
    }

    /* Mouse click */
    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        for (int i = 0; i < LAUNCHER_NODE_COUNT; i++) {
            launcher_node_t* node = &g_nodes[i];
            int nx = cx + node->rel_x - node->w / 2;
            int ny = cy + node->rel_y - node->h / 2;
            if (ev->x >= nx && ev->x < nx + node->w && ev->y >= ny && ev->y < ny + node->h) {
                if (out_launch_app) *out_launch_app = node->app_id;
                launcher_hide();
                return 1;
            }
        }

        /* Clicking center hub or outside closes launcher */
        launcher_hide();
        return 1;
    }

    return 1;
}
