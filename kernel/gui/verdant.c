/* ==============================================================================
 * Greenhouse OS — Modern Liquid-Glass Desktop Environment (Implementation)
 * ==============================================================================
 */

#include "verdant.h"
#include "renderer.h"
#include "compositor.h"
#include "shell.h"
#include "launcher.h"
#include "rail.h"
#include "cursor.h"
#include "morph.h"
#include "guiterm.h"
#include "filebrowser.h"
#include "berry_surface.h"
#include "sysmon.h"
#include "canvas_surface.h"
#include "settings_surface.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "../graphics/framebuffer.h"
#include "../input/input.h"
#include "../input/mouse.h"
#include "../input/kbd.h"
#include "../irq.h"
#include "../pmm.h"
#include "../version.h"

#define VERDANT_REDRAW_INTERVAL 5 /* 50ms tick pacing */

static surface_t    g_surfaces[SURFACE_MAX];
static int          g_surface_order[SURFACE_MAX];
static int          g_surface_count = 0;
static int          g_focused_index = -1;
static surface_id_t g_next_id = 1;

static int          g_verdant_running = 0;
static int          g_verdant_entered = 0;
static int          g_exit_reason = VERDANT_EXIT_NONE;
static int          g_events_processed = 0;
static uint32_t     g_last_presents = 0;
static int          g_redraw_needed = 1;
static verdant_report_t g_last_report;

/* Drag & Resize state */
static int          g_drag_active = 0;
static int          g_drag_index = -1;
static int          g_drag_offset_x = 0;
static int          g_drag_offset_y = 0;
static int          g_resize_active = 0;
static int          g_resize_grab_x = 0;
static int          g_resize_grab_y = 0;
static int          g_resize_w = 0;
static int          g_resize_h = 0;

/* Built-in App States */
static guiterm_t             g_term_state;
static filebrowser_state_t   g_fb_state;
static berry_surface_state_t g_berry_state;

static surface_id_t g_term_id = SURFACE_ID_NONE;
static surface_id_t g_fb_id = SURFACE_ID_NONE;
static surface_id_t g_sysmon_id = SURFACE_ID_NONE;
static surface_id_t g_berry_id = SURFACE_ID_NONE;
static surface_id_t g_canvas_id = SURFACE_ID_NONE;
static surface_id_t g_settings_id = SURFACE_ID_NONE;

/* ------------------------------------------------------------------------------
 * Surface Table & Z-Order Management
 * -------------------------------------------------------------------------- */

static int verdant_find_index(surface_id_t id) {
    for (int i = 0; i < g_surface_count; i++) {
        if (g_surfaces[i].id == id) return i;
    }
    return -1;
}

static void verdant_reorder_from(int index) {
    int pos = -1;
    for (int i = 0; i < g_surface_count; i++) {
        if (g_surface_order[i] == index) { pos = i; break; }
    }
    if (pos < 0 || pos == g_surface_count - 1) return;

    for (int i = pos; i < g_surface_count - 1; i++) g_surface_order[i] = g_surface_order[i + 1];
    g_surface_order[g_surface_count - 1] = index;
}

surface_t* verdant_create_surface(const char* title, const char* tag, int x, int y, int w, int h, uint32_t accent) {
    if (g_surface_count >= SURFACE_MAX) return 0;

    int index = g_surface_count;
    surface_init(&g_surfaces[index], g_next_id++, title, tag, x, y, w, h, accent);

    for (int i = 0; i < g_surface_count; i++) g_surface_order[i] = i;
    g_surface_order[g_surface_count] = index;
    g_surface_count++;

    g_focused_index = index;
    g_redraw_needed = 1;
    return &g_surfaces[index];
}

int verdant_destroy_surface(surface_id_t id) {
    int index = verdant_find_index(id);
    if (index < 0) return -1;

    surface_t* s = &g_surfaces[index];
    if (s->on_close) s->on_close(s);

    for (int i = index; i < g_surface_count - 1; i++) {
        g_surfaces[i] = g_surfaces[i + 1];
    }
    g_surface_count--;

    int new_order[SURFACE_MAX];
    int n = 0;
    for (int i = 0; i < g_surface_count; i++) {
        int entry = g_surface_order[i];
        if (entry == index) continue;
        if (entry > index) entry--;
        new_order[n++] = entry;
    }
    for (int i = 0; i < n; i++) g_surface_order[i] = new_order[i];

    if (g_drag_index == index) { g_drag_active = 0; g_drag_index = -1; }
    if (g_focused_index == index) g_focused_index = (g_surface_count > 0) ? g_surface_order[g_surface_count - 1] : -1;
    if (g_resize_active) g_resize_active = 0;

    if (id == g_term_id) g_term_id = SURFACE_ID_NONE;
    if (id == g_fb_id) g_fb_id = SURFACE_ID_NONE;
    if (id == g_sysmon_id) g_sysmon_id = SURFACE_ID_NONE;
    if (id == g_berry_id) g_berry_id = SURFACE_ID_NONE;
    if (id == g_canvas_id) g_canvas_id = SURFACE_ID_NONE;
    if (id == g_settings_id) g_settings_id = SURFACE_ID_NONE;

    g_redraw_needed = 1;
    return 0;
}

surface_t* verdant_get_surface(surface_id_t id) {
    int idx = verdant_find_index(id);
    return (idx >= 0) ? &g_surfaces[idx] : 0;
}

surface_t* verdant_focused_surface(void) {
    return (g_focused_index >= 0 && g_focused_index < g_surface_count) ? &g_surfaces[g_focused_index] : 0;
}

int verdant_focus_surface(surface_id_t id) {
    int idx = verdant_find_index(id);
    if (idx < 0) return -1;
    g_focused_index = idx;
    verdant_reorder_from(idx);
    g_surfaces[idx].visible = 1;
    g_surfaces[idx].is_minimized = 0;
    g_redraw_needed = 1;
    return 0;
}

int verdant_raise_surface(surface_id_t id) {
    return verdant_focus_surface(id);
}

/* ------------------------------------------------------------------------------
 * Spatial Workspace Constellation Layout
 * -------------------------------------------------------------------------- */

void verdant_arrange_spatial(void) {
    int sw = graphics_get_width();
    int sh = graphics_get_height();

    int usable_y = SHELL_TOPBAR_Y + SHELL_TOPBAR_H + 12;
    int usable_h = SHELL_DOCK_Y - usable_y - 12;

    int vis_count = 0;
    for (int i = 0; i < g_surface_count; i++) {
        if (!g_surfaces[i].is_minimized) vis_count++;
    }
    if (vis_count == 0) return;

    if (vis_count == 1) {
        for (int i = 0; i < g_surface_count; i++) {
            if (!g_surfaces[i].is_minimized) {
                g_surfaces[i].target_w = (sw > 700) ? 660 : sw - 40;
                g_surfaces[i].target_h = usable_h - 20;
                g_surfaces[i].target_x = (sw - g_surfaces[i].target_w) / 2;
                g_surfaces[i].target_y = usable_y + 10;
            }
        }
    } else if (vis_count == 2) {
        int w = (sw - 60) / 2;
        int idx = 0;
        for (int i = 0; i < g_surface_count; i++) {
            if (!g_surfaces[i].is_minimized) {
                g_surfaces[i].target_w = w;
                g_surfaces[i].target_h = usable_h - 20;
                g_surfaces[i].target_x = 20 + idx * (w + 20);
                g_surfaces[i].target_y = usable_y + 10;
                idx++;
            }
        }
    } else {
        /* 3 or more: primary surface + side column */
        int col_w = (sw - 60) / 2;
        int idx = 0;
        for (int i = 0; i < g_surface_count; i++) {
            if (!g_surfaces[i].is_minimized) {
                if (idx == 0) {
                    g_surfaces[i].target_x = 20;
                    g_surfaces[i].target_y = usable_y + 10;
                    g_surfaces[i].target_w = col_w;
                    g_surfaces[i].target_h = usable_h - 20;
                } else {
                    int sub_idx = idx - 1;
                    int sub_h = (usable_h - 30) / (vis_count - 1);
                    if (sub_h < 180) sub_h = 180;
                    g_surfaces[i].target_x = col_w + 40;
                    g_surfaces[i].target_y = usable_y + 10 + sub_idx * (sub_h + 10);
                    g_surfaces[i].target_w = col_w;
                    g_surfaces[i].target_h = sub_h;
                }
                idx++;
            }
        }
    }
    g_redraw_needed = 1;
}

/* ------------------------------------------------------------------------------
 * Application Launchers
 * -------------------------------------------------------------------------- */

static void term_surface_draw(surface_t* s, int cx, int cy, int cw, int ch) {
    guiterm_t* t = (guiterm_t*)s->user_data;
    if (t) guiterm_draw(t, cx, cy, cw, ch);
}

static int term_surface_event(surface_t* s, const input_event_t* ev) {
    guiterm_t* t = (guiterm_t*)s->user_data;
    if (t) {
        guiterm_handle_event(t, ev);
        return 1;
    }
    return 0;
}

int verdant_open_terminal(void) {
    if (g_term_id != SURFACE_ID_NONE) {
        verdant_raise_surface(g_term_id);
        return 0;
    }

    int sw = graphics_get_width();
    int sh = graphics_get_height();
    int w = (sw > 800) ? 580 : sw - 60;
    int h = (sh > 600) ? 380 : sh - 100;
    int x = 30;
    int y = 50;

    surface_t* s = verdant_create_surface("Terminal", "TERM", x, y, w, h, GFX_COLOR_EMERALD_PRIMARY);
    if (!s) return -1;
    g_term_id = s->id;
    s->user_data = &g_term_state;
    s->on_draw = term_surface_draw;
    s->on_event = term_surface_event;
    return 0;
}

int verdant_open_files(void) {
    if (g_fb_id != SURFACE_ID_NONE) {
        verdant_raise_surface(g_fb_id);
        return 0;
    }

    int sw = graphics_get_width();
    int sh = graphics_get_height();
    int w = (sw > 800) ? 540 : sw - 60;
    int h = (sh > 600) ? 360 : sh - 100;
    int x = (sw - w) / 2 + 60;
    int y = 70;

    surface_t* s = verdant_create_surface("File Manager", "FILES", x, y, w, h, GFX_COLOR_CYAN_ACCENT);
    if (!s) return -1;
    g_fb_id = s->id;
    s->user_data = &g_fb_state;
    s->on_draw = filebrowser_draw;
    s->on_event = filebrowser_event;
    return 0;
}

int verdant_open_sysmon(void) {
    if (g_sysmon_id != SURFACE_ID_NONE) {
        verdant_raise_surface(g_sysmon_id);
        return 0;
    }

    int sw = graphics_get_width();
    int sh = graphics_get_height();
    int w = (sw > 800) ? 520 : sw - 60;
    int h = (sh > 600) ? 360 : sh - 100;
    int x = 24;
    int y = 50;

    surface_t* s = verdant_create_surface("Activity Monitor", "SYS", x, y, w, h, GFX_COLOR_AMBER_WARN);
    if (!s) return -1;
    g_sysmon_id = s->id;
    s->on_draw = sysmon_draw;
    s->on_event = sysmon_event;
    return 0;
}

int verdant_open_berry(void) {
    if (g_berry_id != SURFACE_ID_NONE) {
        verdant_raise_surface(g_berry_id);
        return 0;
    }

    int sw = graphics_get_width();
    int sh = graphics_get_height();
    int w = (sw > 800) ? 460 : sw - 60;
    int h = (sh > 600) ? 340 : sh - 100;
    int x = sw - w - 24;
    int y = 50;

    surface_t* s = verdant_create_surface("Berry Assistant", "BERRY", x, y, w, h, GFX_COLOR_BERRY_ACCENT);
    if (!s) return -1;
    g_berry_id = s->id;
    s->user_data = &g_berry_state;
    s->on_draw = berry_surface_draw;
    s->on_event = berry_surface_event;
    return 0;
}

int verdant_open_canvas(void) {
    if (g_canvas_id != SURFACE_ID_NONE) {
        verdant_raise_surface(g_canvas_id);
        return 0;
    }

    int sw = graphics_get_width();
    int sh = graphics_get_height();
    int w = (sw > 800) ? 480 : sw - 60;
    int h = (sh > 600) ? 300 : sh - 100;
    int x = (sw - w) / 2;
    int y = (sh - h) / 2;

    surface_t* s = verdant_create_surface("Canvas Studio", "GFX", x, y, w, h, GFX_COLOR_MINT_ACCENT);
    if (!s) return -1;
    g_canvas_id = s->id;
    s->on_draw = canvas_surface_draw;
    s->on_event = canvas_surface_event;
    return 0;
}

int verdant_open_settings(void) {
    if (g_settings_id != SURFACE_ID_NONE) {
        verdant_raise_surface(g_settings_id);
        return 0;
    }

    int sw = graphics_get_width();
    int sh = graphics_get_height();
    int w = (sw > 800) ? 480 : sw - 60;
    int h = (sh > 600) ? 280 : sh - 100;
    int x = (sw - w) / 2;
    int y = (sh - h) / 2;

    surface_t* s = verdant_create_surface("System Settings", "INFO", x, y, w, h, GFX_COLOR_TEXT_SECONDARY);
    if (!s) return -1;
    g_settings_id = s->id;
    s->on_draw = settings_surface_draw;
    s->on_event = settings_surface_event;
    return 0;
}

/* ------------------------------------------------------------------------------
 * Hit Testing
 * -------------------------------------------------------------------------- */

static int verdant_hit_test(int px, int py, surface_t** out_s, int* out_zone) {
    if (out_s) *out_s = 0;
    if (out_zone) *out_zone = COMPOSITOR_ZONE_NONE;

    for (int i = g_surface_count - 1; i >= 0; i--) {
        surface_t* s = &g_surfaces[g_surface_order[i]];
        if (s->is_minimized || !s->visible) continue;
        if (!surface_includes(s, px, py)) continue;

        int zone = compositor_hit_test(s->x, s->y, s->w, s->h, px, py);
        if (out_s) *out_s = s;
        if (out_zone) *out_zone = zone;
        return 1;
    }
    return 0;
}

static void verdant_update_cursor_shape(int px, int py) {
    if (launcher_is_visible()) {
        cursor_set_shape(CURSOR_ARROW);
        return;
    }

    surface_t* s = 0;
    int zone = COMPOSITOR_ZONE_NONE;
    if (!verdant_hit_test(px, py, &s, &zone)) {
        cursor_set_shape(CURSOR_ARROW);
        return;
    }

    if (zone == COMPOSITOR_ZONE_RESIZE) {
        cursor_set_shape(CURSOR_RESIZE_H);
        return;
    }

    if (zone == COMPOSITOR_ZONE_TITLE) {
        cursor_set_shape(CURSOR_HAND);
        return;
    }

    cursor_set_shape(CURSOR_ARROW);
}

static int verdant_handle_event(const input_event_t* ev) {
    if (!ev) return 0;
    int sw = graphics_get_width();
    int sh = graphics_get_height();

    /* 1. Global Keyboard Shortcuts */
    if (ev->type == INPUT_EVENT_KEY_DOWN) {
        /* ESC: dismiss launcher or request exit */
        if (ev->ascii == 27) {
            if (launcher_is_visible()) {
                launcher_hide();
                g_redraw_needed = 1;
                return 1;
            }
            g_exit_reason = VERDANT_EXIT_ESC;
            g_verdant_running = 0;
            return 1;
        }

        /* Number hotkeys 1..6 */
        if (launcher_is_visible() && ev->ascii >= '1' && ev->ascii <= '6') {
            int app = ev->ascii - '1';
            launcher_hide();
            if (app == 0) verdant_open_terminal();
            else if (app == 1) verdant_open_files();
            else if (app == 2) verdant_open_sysmon();
            else if (app == 3) verdant_open_berry();
            else if (app == 4) verdant_open_canvas();
            else if (app == 5) verdant_open_settings();
            g_redraw_needed = 1;
            return 1;
        }

        /* F1..F5 function hotkeys */
        if (ev->scancode == 0x3B) { launcher_toggle(); g_redraw_needed = 1; return 1; }
        if (ev->scancode == 0x3C) { verdant_open_berry(); return 1; }
        if (ev->scancode == 0x3D) { verdant_open_terminal(); return 1; }
        if (ev->scancode == 0x3E) { verdant_open_files(); return 1; }
        if (ev->scancode == 0x3F) { verdant_arrange_spatial(); return 1; }

        /* Space with Alt/Ctrl: Toggle Launcher */
        if (((ev->ascii == ' ' || ev->ascii == 'l' || ev->ascii == 'L') &&
             (ev->modifiers & (INPUT_MOD_ALT | INPUT_MOD_CTRL)))) {
            launcher_toggle();
            g_redraw_needed = 1;
            return 1;
        }

        /* Tab: Cycle Focus */
        if (ev->ascii == '\t' && g_surface_count > 1) {
            int next_idx = (g_focused_index + 1) % g_surface_count;
            verdant_focus_surface(g_surfaces[next_idx].id);
            return 1;
        }
    }

    /* 2. Radial Launcher Overlay */
    if (launcher_is_visible()) {
        int launch_app = LAUNCHER_APP_NONE;
        if (launcher_handle_event(ev, sw, sh, &launch_app)) {
            if (launch_app == LAUNCHER_APP_TERMINAL) verdant_open_terminal();
            else if (launch_app == LAUNCHER_APP_FILES) verdant_open_files();
            else if (launch_app == LAUNCHER_APP_SYSMON) verdant_open_sysmon();
            else if (launch_app == LAUNCHER_APP_BERRY) verdant_open_berry();
            else if (launch_app == LAUNCHER_APP_CANVAS) verdant_open_canvas();
            else if (launch_app == LAUNCHER_APP_SETTINGS) verdant_open_settings();
            g_redraw_needed = 1;
            return 1;
        }
    }

    /* 3. Top Status Bar Hit Testing */
    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        int tab_idx = -1;
        int top_hit = shell_topbar_hit_test(ev->x, ev->y, &tab_idx);
        if (top_hit == 999) {
            /* Console Exit clicked */
            g_exit_reason = VERDANT_EXIT_ESC;
            g_verdant_running = 0;
            return 1;
        }
    }

    /* 4. Bottom Dock Hit Testing */
    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        int dock_idx = shell_dock_hit_test(ev->x, ev->y);
        if (dock_idx >= 0) {
            if (dock_idx == 0) verdant_open_terminal();
            else if (dock_idx == 1) verdant_open_files();
            else if (dock_idx == 2) verdant_open_sysmon();
            else if (dock_idx == 3) verdant_open_berry();
            else if (dock_idx == 4) verdant_open_canvas();
            else if (dock_idx == 5) verdant_open_settings();
            g_redraw_needed = 1;
            return 1;
        }
    }

    /* 5. Pointer Movement & Drag/Resize */
    if (ev->type == INPUT_EVENT_MOUSE_MOVE) {
        verdant_update_cursor_shape(ev->x, ev->y);

        if (g_drag_active && g_drag_index >= 0) {
            surface_t* s = &g_surfaces[g_drag_index];
            int x = ev->x - g_drag_offset_x;
            int y = ev->y - g_drag_offset_y;

            if (x > sw - s->w) x = sw - s->w;
            if (y > sh - s->h) y = sh - s->h;
            if (x < 0) x = 0;
            if (y < 0) y = 0;

            s->x = x; s->y = y;
            s->target_x = x; s->target_y = y;
            g_redraw_needed = 1;
        } else if (g_resize_active && g_focused_index >= 0) {
            surface_t* s = &g_surfaces[g_focused_index];
            int w = g_resize_w + (ev->x - g_resize_grab_x);
            int h = g_resize_h + (ev->y - g_resize_grab_y);
            if (w < s->min_w) w = s->min_w;
            if (h < s->min_h) h = s->min_h;
            if (w > sw) w = sw;
            if (h > sh) h = sh;

            s->w = w; s->h = h;
            s->target_w = w; s->target_h = h;
            g_redraw_needed = 1;
        }
        return 0;
    }

    /* 6. Mouse Down on Surfaces */
    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        surface_t* s = 0;
        int zone = COMPOSITOR_ZONE_NONE;

        if (verdant_hit_test(ev->x, ev->y, &s, &zone)) {
            verdant_focus_surface(s->id);

            if (zone == COMPOSITOR_ZONE_TITLE) {
                g_drag_active = 1;
                g_drag_index = verdant_find_index(s->id);
                g_drag_offset_x = ev->x - s->x;
                g_drag_offset_y = ev->y - s->y;
            } else if (zone == COMPOSITOR_ZONE_RESIZE && (s->flags & SURFACE_FLAG_RESIZABLE)) {
                g_resize_active = 1;
                g_focused_index = verdant_find_index(s->id);
                g_resize_grab_x = ev->x;
                g_resize_grab_y = ev->y;
                g_resize_w = s->w;
                g_resize_h = s->h;
            } else if (zone == COMPOSITOR_ZONE_CLOSE) {
                verdant_destroy_surface(s->id);
                return 1;
            } else if (zone == COMPOSITOR_ZONE_MAXIMIZE) {
                surface_maximize(s, sw, sh);
                g_redraw_needed = 1;
                return 1;
            } else if (zone == COMPOSITOR_ZONE_MINIMIZE) {
                s->is_minimized = 1;
                g_redraw_needed = 1;
                return 1;
            } else if (zone == COMPOSITOR_ZONE_CLIENT && s->on_event) {
                int res = s->on_event(s, ev);
                if (g_berry_state.requested_action == 1) { g_berry_state.requested_action = 0; verdant_open_terminal(); }
                else if (g_berry_state.requested_action == 2) { g_berry_state.requested_action = 0; verdant_open_files(); }
                else if (g_berry_state.requested_action == 3) { g_berry_state.requested_action = 0; verdant_open_sysmon(); }
                return res;
            }
        }
        g_redraw_needed = 1;
        return 0;
    }

    /* 7. Mouse Up */
    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_UP) {
        if (g_drag_active || g_resize_active) {
            g_drag_active = 0;
            g_drag_index = -1;
            g_resize_active = 0;
            g_redraw_needed = 1;
        }
        surface_t* s = verdant_focused_surface();
        if (s && s->on_event) return s->on_event(s, ev);
        return 0;
    }

    /* 8. Key / Wheel routing to focused surface */
    if (ev->type == INPUT_EVENT_KEY_DOWN || ev->type == INPUT_EVENT_MOUSE_WHEEL) {
        surface_t* s = verdant_focused_surface();
        if (s && s->on_event) {
            int res = s->on_event(s, ev);
            if (g_berry_state.requested_action == 1) { g_berry_state.requested_action = 0; verdant_open_terminal(); }
            else if (g_berry_state.requested_action == 2) { g_berry_state.requested_action = 0; verdant_open_files(); }
            else if (g_berry_state.requested_action == 3) { g_berry_state.requested_action = 0; verdant_open_sysmon(); }
            return res;
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------------
 * Composition
 * -------------------------------------------------------------------------- */

static void verdant_compose(void) {
    int sw = graphics_get_width();
    int sh = graphics_get_height();

    /* 1. Calm Organic Wallpaper */
    compositor_draw_wallpaper(sw, sh);

    /* 2. Surfaces in Z-Order */
    for (int i = 0; i < g_surface_count; i++) {
        surface_t* s = &g_surfaces[g_surface_order[i]];
        if (s->is_minimized || !s->visible) continue;
        surface_step_animation(s);
        int is_focus = (g_surface_order[i] == g_focused_index);

        /* Window frame & drop shadow */
        compositor_draw_surface_frame(s->x, s->y, s->w, s->h, s->title, s->tag, is_focus, s->is_maximized);

        /* Client Area (Clipped) */
        int cx = s->x + 2;
        int cy = s->y + COMPOSITOR_TITLE_HEIGHT + 1;
        int cw = s->w - 4;
        int ch = s->h - COMPOSITOR_TITLE_HEIGHT - 3;
        if (cw > 0 && ch > 0) {
            graphics_set_clip(cx, cy, cw, ch);
            if (s->on_draw) {
                s->on_draw(s, cx, cy, cw, ch);
            }
            graphics_clear_clip();
        }
    }

    /* 3. Pointer coordinates for dock */
    int mx = 0, my = 0;
    input_get_pointer(&mx, &my);
    int hover_dock = shell_dock_hit_test(mx, my);

    /* 4. Top Status Bar & Bottom Floating Dock */
    surface_t* surface_ptrs[SURFACE_MAX];
    for (int i = 0; i < g_surface_count; i++) surface_ptrs[i] = &g_surfaces[i];
    surface_t* focused = (g_focused_index >= 0) ? &g_surfaces[g_focused_index] : NULL;

    shell_draw_topbar(focused, surface_ptrs, g_surface_count);
    shell_draw_dock(hover_dock, surface_ptrs, g_surface_count);

    /* 5. Radial Launcher Overlay */
    if (launcher_is_visible()) {
        launcher_draw(sw, sh);
    }

    /* 6. Software Cursor */
    cursor_draw(mx, my);

    g_redraw_needed = 0;
}

/* ------------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

void verdant_init(void) {
    g_surface_count = 0;
    g_focused_index = -1;
    g_next_id = 1;
    g_verdant_running = 0;
    g_verdant_entered = 0;
    g_exit_reason = VERDANT_EXIT_NONE;
    g_events_processed = 0;
    g_redraw_needed = 1;

    g_term_id = SURFACE_ID_NONE;
    g_fb_id = SURFACE_ID_NONE;
    g_sysmon_id = SURFACE_ID_NONE;
    g_berry_id = SURFACE_ID_NONE;
    g_canvas_id = SURFACE_ID_NONE;
    g_settings_id = SURFACE_ID_NONE;

    compositor_init();
    shell_init();
    launcher_init();
    cursor_init();
    guiterm_init(&g_term_state);
    filebrowser_init(&g_fb_state);
    berry_surface_init(&g_berry_state);
    sysmon_init();
    canvas_surface_init();
    settings_surface_init();
}

int verdant_enter(void) {
    if (g_verdant_entered) return 0;

    int rc = graphics_enter();
    if (rc != 0) return rc;

    verdant_init();
    g_verdant_entered = 1;

    int sw = graphics_get_width();
    int sh = graphics_get_height();

    /* Open default spatial layout: Terminal, File Manager, Berry Assistant */
    verdant_open_terminal();
    verdant_open_files();
    verdant_open_berry();
    verdant_arrange_spatial();

    input_set_pointer_bounds(sw, sh);
    input_set_pointer(sw / 2, sh / 2);
    g_last_presents = (uint32_t)graphics_get_present_count();
    return 0;
}

void verdant_leave(void) {
    if (!g_verdant_entered) return;
    g_verdant_entered = 0;
    g_verdant_running = 0;
    g_surface_count = 0;
    g_focused_index = -1;

    graphics_leave();
    cursor_set_visible(1);
}

int verdant_run(int max_seconds) {
    if (!g_verdant_entered) return VERDANT_EXIT_ERROR;

    g_verdant_running = 1;
    g_exit_reason = VERDANT_EXIT_NONE;
    uint64_t start_ticks = timer_get_ticks();
    uint64_t last_redraw = 0;

    input_flush();

    while (g_verdant_running) {
        uint64_t now = timer_get_ticks();

        if (max_seconds > 0 && ((now - start_ticks) >= (uint64_t)(max_seconds * 100))) {
            g_exit_reason = VERDANT_EXIT_TIMEOUT;
            break;
        }

        input_event_t ev;
        int had_events = 0;

        while (input_poll_event(&ev)) {
            had_events = 1;
            g_events_processed++;
            verdant_handle_event(&ev);
            if (!g_verdant_running) break;
        }

        if (had_events || (now - last_redraw >= VERDANT_REDRAW_INTERVAL)) {
            graphics_begin_frame();
            verdant_compose();
            graphics_end_frame();
            graphics_present();
            last_redraw = now;
        }

        /* Halt until next IRQ tick */
        asm volatile("hlt");
    }

    g_last_report.width = graphics_get_width();
    g_last_report.height = graphics_get_height();
    g_last_report.bpp = graphics_get_bpp();
    g_last_report.backend_multiboot = (framebuffer_get_info()->backend == FB_BACKEND_MULTIBOOT2);
    g_last_report.back_buffer = graphics_has_back_buffer();
    g_last_report.back_buffer_size = graphics_get_back_buffer_size();
    g_last_report.presents = (uint32_t)(graphics_get_present_count() - g_last_presents);
    g_last_report.frames = (int)g_last_report.presents;
    g_last_report.events_processed = g_events_processed;
    g_last_report.exit_reason = g_exit_reason;

    return g_exit_reason;
}

int  verdant_is_running(void) { return g_verdant_running; }
void verdant_request_exit(int reason) { g_exit_reason = reason; g_verdant_running = 0; }
int  verdant_get_exit_reason(void) { return g_exit_reason; }
void verdant_get_report(verdant_report_t* out) {
    if (out) *out = g_last_report;
}
