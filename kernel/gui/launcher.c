/* ==============================================================================
 * Greenhouse OS - Light Card Launcher (implementation)
 * ==============================================================================
 */

#include "launcher.h"
#include "app_registry.h"
#include "renderer.h"
#include "gh_theme.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"

static int g_launcher_visible = 0;
static int g_selected_index = 0;
static char g_search[64] = "";

static launcher_node_t g_nodes[LAUNCHER_NODE_COUNT] = {
    { LAUNCHER_APP_TERMINAL, '1', "Terminal",       "TERM", "Greenhouse interactive shell and CLI",       GH_COLOR_GREEN_LEAF },
    { LAUNCHER_APP_FILES,    '2', "File Browser",   "FILES","Browse FAT32 (C:) and RAMFS (R:) drives",    GH_COLOR_BLUE_INFO },
    { LAUNCHER_APP_SYSMON,   '3', "System Monitor", "SYS",  "Real-time CPU, RAM, heap, and processes",    GH_COLOR_AMBER_WARN },
    { LAUNCHER_APP_BERRY,    '4', "Berry AI",       "BERRY","Interactive system assistant and voice AI",   GH_BERRY_ACCENT },
    { LAUNCHER_APP_CANVAS,   '5', "GFX Canvas",     "GFX",  "2D graphics demonstration and primitives",   GH_COLOR_GREEN_LEAF },
    { LAUNCHER_APP_SETTINGS, '6', "System Info",    "INFO", "Display properties and compositor report",   GH_COLOR_GREEN_LEAF_DEEP }
};

static void launcher_sync_nodes(void) {
    int count = app_registry_count();
    if (count > LAUNCHER_NODE_COUNT) count = LAUNCHER_NODE_COUNT;
    for (int i = 0; i < count; i++) {
        const app_entry_t* app = app_registry_get(i);
        if (app) {
            g_nodes[i].app_id = app->id;
            g_nodes[i].key = app->key;
            g_nodes[i].title = app->name;
            g_nodes[i].tag = app->tag;
            g_nodes[i].desc = app->desc;
            g_nodes[i].accent = app->accent;
        }
    }
}

void launcher_init(void) {
    g_launcher_visible = 0;
    g_selected_index = 0;
    launcher_sync_nodes();
}

void launcher_show(void) {
    launcher_sync_nodes();
    g_launcher_visible = 1;
}

void launcher_hide(void) {
    g_launcher_visible = 0;
}

void launcher_toggle(void) {
    if (!g_launcher_visible) launcher_sync_nodes();
    g_launcher_visible = !g_launcher_visible;
}

int launcher_is_visible(void) {
    return g_launcher_visible;
}

void launcher_draw(int sw, int sh) {
    if (!g_launcher_visible) return;

    /* Card dimensions */
    int card_w = 520;
    int card_h = 470;
    int card_x = (sw - card_w) / 2;
    int card_y = (sh - card_h) / 2;

    /* 1. Backdrop overlay scrim - soft botanical ambient glass */
    renderer_fill_alpha_rect(0, 0, sw, sh, 0x142018, 45);

    /* 2. Main Launcher Card */
    renderer_draw_card_panel(card_x, card_y, card_w, card_h, GH_RADIUS_PANEL,
                             GH_COLOR_SURFACE, 252, GH_COLOR_BORDER_LIGHT, 4);

    /* 3. Header */
    int header_h = 54;
    int header_y = card_y + 14;
    /* Greenhouse brand with Seed Menu title */
    draw_text(card_x + 20, header_y, "Greenhouse Seed Menu", GH_COLOR_GREEN_LEAF_DEEP, FONT_TRANSPARENT, 1);
    draw_text(card_x + 20, header_y + 20, "Press Seed / [Win] or number (1-6) to launch...", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);

    /* 4. Search Input Field */
    int input_x = card_x + 20;
    int input_y = card_y + header_h + 8;
    int input_w = card_w - 40;
    int input_h = 38;
    renderer_draw_input_field(input_x, input_y, input_w, input_h, g_search,
                              GH_COLOR_SURFACE_HOVER, GH_COLOR_BORDER_LIGHT,
                              GH_COLOR_TEXT_PRIMARY, GH_COLOR_TEXT_FAINT, 1, g_search[0] != '\0');

    /* 5. App Grid / List */
    int item_h = 44;
    int item_y = input_y + input_h + 12;
    int visible_items = 6;

    for (int i = 0; i < LAUNCHER_NODE_COUNT && i < visible_items; i++) {
        launcher_node_t* node = &g_nodes[i];
        int selected = (i == g_selected_index);

        int item_x = card_x + 16;
        int item_w = card_w - 32;

        /* Item background on hover/selection */
        if (selected) {
            renderer_fill_alpha_rounded_rect(item_x, item_y, item_w, item_h, GH_RADIUS_MD,
                                             GH_COLOR_SURFACE_HOVER, 255);
            graphics_draw_rounded_rect(item_x, item_y, item_w, item_h, GH_RADIUS_MD, GH_COLOR_GREEN_LEAF);
        }

        /* Accent color indicator bar on left */
        int bar_x = item_x + 12;
        int bar_y = item_y + 6;
        int bar_w = 3;
        int bar_h = item_h - 12;
        renderer_fill_alpha_rounded_rect(bar_x, bar_y, bar_w, bar_h, 1, node->accent, 255);

        /* Key chip */
        char key_str[4];
        key_str[0] = '[';
        key_str[1] = node->key;
        key_str[2] = ']';
        key_str[3] = '\0';
        draw_text(item_x + 24, item_y + 6, key_str, selected ? node->accent : GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);

        /* Title */
        draw_text(item_x + 52, item_y + 6, node->title, selected ? GH_COLOR_TEXT_PRIMARY : GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 1);

        /* Tag/Desc */
        draw_text(item_x + 52, item_y + 22, node->tag, selected ? GH_COLOR_GREEN_LEAF : GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);

        item_y += item_h + 6;
    }

    /* 6. Footer hint */
    int hint_y = card_y + card_h - 24;
    draw_text_centered(card_x, hint_y, card_w, "Type to search  |  1-6 or Enter to launch  |  ESC to close", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
}

int launcher_handle_event(const input_event_t* ev, int sw, int sh, int* out_launch_app) {
    (void)sw; (void)sh;
    if (!g_launcher_visible || !ev) return 0;
    if (out_launch_app) *out_launch_app = LAUNCHER_APP_NONE;

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

    /* Simple mouse handling - just check if click is outside card area to close */
    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        /* For now, any mouse click closes launcher (could be improved with hit testing) */
        launcher_hide();
        return 1;
    }

    return 1;
}
