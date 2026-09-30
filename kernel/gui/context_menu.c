/* ==============================================================================
 * Greenhouse OS — Modern Desktop Context Menu (Implementation)
 * ==============================================================================
 */

#include "context_menu.h"
#include "renderer.h"
#include "gh_theme.h"
#include "shell.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"

#define CTX_WIDTH        224
#define CTX_ITEM_HEIGHT  26
#define CTX_PADDING_TOP  8
#define CTX_PADDING_BOT  8
#define CTX_ITEM_COUNT   8

typedef struct {
    const char* label;
    const char* shortcut;
    const char* icon;
    int action;
    uint32_t color;
} ctx_item_desc_t;

static const ctx_item_desc_t s_ctx_items[CTX_ITEM_COUNT] = {
    { "Terminal",        "Seed+T", ">_", CTX_ACTION_TERMINAL, GH_COLOR_GREEN_LEAF },
    { "File Manager",    "Seed+E", "FL", CTX_ACTION_FILES,    GH_COLOR_BLUE_INFO },
    { "System Monitor",  "Seed+M", "SY", CTX_ACTION_SYSMON,   GH_COLOR_AMBER_WARN },
    { "Berry Assistant", "Seed+B", "*",  CTX_ACTION_BERRY,    GH_BERRY_ACCENT },
    { "Tile Windows",    "Seed+S", "[]", CTX_ACTION_TILE,     GH_COLOR_GREEN_LEAF_DEEP },
    { "Show Desktop",    "Seed+D", "DK", CTX_ACTION_DESKTOP,  GH_COLOR_TEXT_PRIMARY },
    { "Settings",        "Seed+,", "CF", CTX_ACTION_SETTINGS, GH_COLOR_TEXT_SECONDARY },
    { "Logout",          "logout", "->", CTX_ACTION_LOGOUT,   GH_COLOR_RED_ERROR }

};


static int s_visible = 0;
static int s_x = 0;
static int s_y = 0;
static int s_w = CTX_WIDTH;
static int s_h = CTX_PADDING_TOP + (CTX_ITEM_COUNT * CTX_ITEM_HEIGHT) + CTX_PADDING_BOT;
static int s_hovered = -1;

void context_menu_init(void) {
    s_visible = 0;
    s_hovered = -1;
}

void context_menu_show(int x, int y) {
    int sw = graphics_get_width();
    int sh = graphics_get_height();
    (void)sh;

    s_w = CTX_WIDTH;
    s_h = CTX_PADDING_TOP + (CTX_ITEM_COUNT * CTX_ITEM_HEIGHT) + CTX_PADDING_BOT;

    /* Clamp to screen keeping clear of topbar and dock */
    int min_y = SHELL_TOPBAR_Y + SHELL_TOPBAR_H + 4;
    int max_y = SHELL_DOCK_Y - s_h - 4;

    if (x + s_w > sw - 12) x = sw - 12 - s_w;
    if (x < 12) x = 12;

    if (y > max_y) y = max_y;
    if (y < min_y) y = min_y;

    s_x = x;
    s_y = y;
    s_hovered = -1;
    s_visible = 1;
}

void context_menu_hide(void) {
    s_visible = 0;
    s_hovered = -1;
}

int context_menu_is_visible(void) {
    return s_visible;
}

void context_menu_draw(void) {
    if (!s_visible || !graphics_is_active()) return;

    /* 1. Frosted Glass Card Panel with soft drop shadow */
    renderer_draw_card_panel(s_x, s_y, s_w, s_h, 8, GH_COLOR_SURFACE, 246, GH_COLOR_BORDER_LIGHT, 3);

    /* 3. Items */
    for (int i = 0; i < CTX_ITEM_COUNT; i++) {
        int ix = s_x + 6;
        int iy = s_y + CTX_PADDING_TOP + (i * CTX_ITEM_HEIGHT);
        int iw = s_w - 12;
        int ih = CTX_ITEM_HEIGHT - 2;

        int is_hov = (i == s_hovered);

        if (i == 4) {
            renderer_fill_alpha_rect(ix + 6, iy - 2, iw - 12, 1, GH_COLOR_BORDER_LIGHT, 120);
        }

        if (is_hov) {
            renderer_fill_aa_rounded_rect(ix, iy, iw, ih, 6, GH_COLOR_SURFACE_HOVER, 240);
            renderer_draw_aa_rounded_rect(ix, iy, iw, ih, 6, 1, GH_COLOR_GREEN_LEAF, 100);
        }

        /* Icon Badge */
        uint32_t badge_bg = is_hov ? s_ctx_items[i].color : GH_COLOR_SURFACE_HOVER;
        uint32_t badge_fg = is_hov ? GH_COLOR_WHITE : s_ctx_items[i].color;
        renderer_draw_badge(ix + 4, iy + 2, s_ctx_items[i].icon, badge_bg, badge_fg, 3);

        /* Action Label */
        uint32_t label_fg = is_hov ? GH_COLOR_TEXT_PRIMARY : GH_COLOR_TEXT_PRIMARY;
        draw_text(ix + 34, iy + 4, s_ctx_items[i].label, label_fg, FONT_TRANSPARENT, 0);

        /* Shortcut Right Aligned */
        if (s_ctx_items[i].shortcut && s_ctx_items[i].shortcut[0]) {
            int sw = font_text_width(s_ctx_items[i].shortcut);
            draw_text(ix + iw - sw - 6, iy + 4, s_ctx_items[i].shortcut, GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
        }
    }
}

int context_menu_handle_event(const input_event_t* ev, int* out_action) {
    if (out_action) *out_action = CTX_ACTION_NONE;
    if (!s_visible || !ev) return 0;

    /* Check ESC key or hotkeys */
    if (ev->type == INPUT_EVENT_KEY_DOWN) {
        if (ev->ascii == 27 || ev->scancode == KBD_SCAN_ESC) {
            context_menu_hide();
            return 1;
        }
        if (ev->ascii >= '1' && ev->ascii <= '6') {
            int idx = ev->ascii - '1';
            if (out_action) *out_action = s_ctx_items[idx].action;
            context_menu_hide();
            return 1;
        }
    }

    /* Mouse Move: Update hover */
    if (ev->type == INPUT_EVENT_MOUSE_MOVE) {
        if (ev->x >= s_x && ev->x < s_x + s_w && ev->y >= s_y && ev->y < s_y + s_h) {
            int rel_y = ev->y - (s_y + CTX_PADDING_TOP);
            if (rel_y >= 0 && rel_y < CTX_ITEM_COUNT * CTX_ITEM_HEIGHT) {
                s_hovered = rel_y / CTX_ITEM_HEIGHT;
            } else {
                s_hovered = -1;
            }
            return 1;
        } else {
            s_hovered = -1;
            return 0;
        }
    }

    /* Mouse Down: Click item or dismiss */
    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev->x >= s_x && ev->x < s_x + s_w && ev->y >= s_y && ev->y < s_y + s_h) {
            int rel_y = ev->y - (s_y + CTX_PADDING_TOP);
            if (rel_y >= 0 && rel_y < CTX_ITEM_COUNT * CTX_ITEM_HEIGHT) {
                int clicked_item = rel_y / CTX_ITEM_HEIGHT;
                if (clicked_item >= 0 && clicked_item < CTX_ITEM_COUNT) {
                    if (out_action) *out_action = s_ctx_items[clicked_item].action;
                }
            }
            context_menu_hide();
            return 1;
        } else {
            /* Click outside: dismiss */
            context_menu_hide();
            return 1;
        }
    }

    return 0;
}
