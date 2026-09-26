/* ==============================================================================
 * Greenhouse OS - Phase 7: Graphical User Interface (implementation)
 * ==============================================================================
 */

#include "gui.h"
#include "wm.h"
#include "widget.h"
#include "cursor.h"
#include "guiterm.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "../graphics/framebuffer.h"
#include "../input/input.h"
#include "../input/mouse.h"
#include "../version.h"
#include "../irq.h"
#include "../pmm.h"

#define GUI_REDRAW_INTERVAL 5      /* ticks between animated redraws (50 ms) */

/* Sample windows */
static window_t  gui_control_window;
static window_t  gui_term_window;
static window_t  gui_about_window;

static widget_t  gui_widgets[5];
static guiterm_t gui_term;

static int  gui_running = 0;
static int  gui_entered = 0;
static int  gui_exit_reason = GUI_EXIT_NONE;
static int  gui_events_processed = 0;
static uint32_t gui_last_presents = 0;
static int  gui_animate = 1;
static int  gui_about_open = 0;
static gui_report_t gui_last_report;

static void gui_put(char* buf, int* n, int cap, const char* s) {
    while (*s && *n < cap - 1) buf[(*n)++] = *s++;
    buf[*n] = '\0';
}

static void gui_put_uint(char* buf, int* n, int cap, uint64_t v) {
    char tmp[24];
    int tl = 0;
    do { tmp[tl++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (tl && *n < cap - 1) buf[(*n)++] = tmp[--tl];
    buf[*n] = '\0';
}

/* ------------------------------------------------------------------------------
 * Control panel window
 * -------------------------------------------------------------------------- */

static void gui_control_draw(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;

    /* Header */
    graphics_fill_gradient_h(cx + 8, cy + 8, cw - 16, 28, 0xFF1D4ED8, 0xFF0EA5E9);
    draw_text(cx + 18, cy + 14, "Greenhouse Control Panel", 0xFFFFFFFF, 0xFF1D4ED8, 1);

    if (gui_animate) {
        /* Frame paced progress: the bar advances once per PIT tick. */
        widget_set_percent(&gui_widgets[3], (int)(timer_get_ticks() % 100));
    }
    for (int i = 0; i < 5; i++) widget_draw(&gui_widgets[i]);

    /* Live pointer readout at the bottom of the client area. */
    int px = 0, py = 0;
    input_get_pointer(&px, &py);

    char line[96];
    int n = 0;
    line[0] = '\0';
    gui_put(line, &n, (int)sizeof(line), "pointer ");
    gui_put_uint(line, &n, (int)sizeof(line), (uint64_t)px);
    gui_put(line, &n, (int)sizeof(line), ",");
    gui_put(line, &n, (int)sizeof(line), " ");
    gui_put_uint(line, &n, (int)sizeof(line), (uint64_t)py);
    gui_put(line, &n, (int)sizeof(line), "   buttons ");
    gui_put_uint(line, &n, (int)sizeof(line), (uint64_t)input_get_buttons());
    gui_put(line, &n, (int)sizeof(line), "   events ");
    gui_put_uint(line, &n, (int)sizeof(line), (uint64_t)gui_events_processed);
    draw_text(cx + 12, cy + ch - 46, line, 0xFF94A3B8, win->client_color, 1);

    n = 0;
    line[0] = '\0';
    gui_put(line, &n, (int)sizeof(line), "mode ");
    gui_put_uint(line, &n, (int)sizeof(line), (uint64_t)graphics_get_width());
    gui_put(line, &n, (int)sizeof(line), "x");
    gui_put_uint(line, &n, (int)sizeof(line), (uint64_t)graphics_get_height());
    gui_put(line, &n, (int)sizeof(line), "  bpp ");
    gui_put_uint(line, &n, (int)sizeof(line), (uint64_t)graphics_get_bpp());
    gui_put(line, &n, (int)sizeof(line), "  surface ");
    gui_put(line, &n, (int)sizeof(line), graphics_has_back_buffer() ? "back buffer" : "direct");
    draw_text(cx + 12, cy + ch - 26, line, 0xFF94A3B8, win->client_color, 1);
}

static int gui_control_event(window_t* win, const input_event_t* ev) {
    int cx, cy, cw, ch;
    wm_client_rect(win, &cx, &cy, &cw, &ch);

    if (ev->type != INPUT_EVENT_MOUSE_BUTTON_DOWN && ev->type != INPUT_EVENT_MOUSE_BUTTON_UP) {
        return 0;
    }

    int pressed = (ev->type == INPUT_EVENT_MOUSE_BUTTON_DOWN);
    int lx = ev->x - cx;
    int ly = ev->y - cy;

    for (int i = 0; i < 5; i++) {
        widget_t* w = &gui_widgets[i];
        if (w->type != WIDGET_BUTTON && w->type != WIDGET_CHECKBOX) continue;

        int inside = (lx >= w->x && ly >= w->y && lx < w->x + w->w && ly < w->y + w->h);
        if (!inside) {
            widget_set_pressed(w, 0);
            continue;
        }

        if (w->type == WIDGET_BUTTON) {
            widget_set_pressed(w, pressed);
            if (pressed && i == 1) {
                /* "focus terminal" */
                if (gui_term_window.id != WINDOW_ID_NONE) {
                    wm_focus_window(gui_term_window.id);
                    wm_raise_window(gui_term_window.id);
                }
            }
        } else if (w->type == WIDGET_CHECKBOX) {
            if (pressed) {
                widget_set_checked(w, !widget_is_checked(w));
                gui_animate = widget_is_checked(w);
            }
        }
        wm_request_redraw();
        return 1;
    }

    /* Clicking the panel background releases any pressed button. */
    for (int i = 0; i < 5; i++) widget_set_pressed(&gui_widgets[i], 0);
    return 1;
}

/* ------------------------------------------------------------------------------
 * Terminal window
 * -------------------------------------------------------------------------- */

static void gui_term_draw(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;
    guiterm_draw(&gui_term, cx, cy, cw, ch);
}

static int gui_term_event(window_t* win, const input_event_t* ev) {
    (void)win;
    guiterm_handle_event(&gui_term, ev);
    wm_request_redraw();
    return 1;
}

/* ------------------------------------------------------------------------------
 * About window
 * -------------------------------------------------------------------------- */

static void gui_about_draw(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;
    graphics_fill_rect(cx, cy, cw, ch, 0xFF101828);

    int y = cy + 10;
    draw_text(cx + 14, y, "Greenhouse OS", 0xFF60A5FA, 0xFF101828, 1);
    y += 24;
    draw_text(cx + 14, y, GREENHOUSE_VERSION_STRING "  " GREENHOUSE_BUILD_TYPE, 0xFFE2E8F0, 0xFF101828, 1);
    y += 24;
    draw_text(cx + 14, y, "compositor: software (back buffer)", 0xFF94A3B8, 0xFF101828, 1);
    y += 20;
    draw_text(cx + 14, y, "pointer: PS/2 mouse, software cursor", 0xFF94A3B8, 0xFF101828, 1);
    y += 20;
    draw_text(cx + 14, y, "font: 8x16 bitmap (Adwaita Mono)", 0xFF94A3B8, 0xFF101828, 1);
    y += 20;
    draw_text(cx + 14, y, "press ESC to leave the GUI", 0xFF6EE7B7, 0xFF101828, 1);
}

static int gui_about_event(window_t* win, const input_event_t* ev) {
    (void)win; (void)ev;
    return 0;
}

/* ------------------------------------------------------------------------------
 * Window creation
 * -------------------------------------------------------------------------- */

static void gui_build_widgets(int cw, int ch) {
    (void)ch;
    widget_init_array(gui_widgets, 5);

    int pad = 12;
    int y = 48;

    widget_set_text(&gui_widgets[0], "animate");
    gui_widgets[0].type = WIDGET_CHECKBOX;
    widget_set_rect(&gui_widgets[0], pad, y, 120, 24);
    widget_set_checked(&gui_widgets[0], 1);

    widget_set_text(&gui_widgets[1], "focus terminal");
    gui_widgets[1].type = WIDGET_BUTTON;
    widget_set_rect(&gui_widgets[1], pad + 140, y - 2, 130, 28);

    widget_set_text(&gui_widgets[2], "rendering");
    gui_widgets[2].type = WIDGET_LABEL;
    widget_set_rect(&gui_widgets[2], pad, y + 40, cw - 2 * pad, 18);
    gui_widgets[2].fg = 0xFF94A3B8;

    widget_set_text(&gui_widgets[3], "load");
    gui_widgets[3].type = WIDGET_PROGRESS;
    widget_set_rect(&gui_widgets[3], pad, y + 66, cw - 2 * pad, 20);

    widget_set_text(&gui_widgets[4], "drag the title bar, resize from a corner, click the X");
    gui_widgets[4].type = WIDGET_LABEL;
    widget_set_rect(&gui_widgets[4], pad, y + 100, cw - 2 * pad, 18);
    gui_widgets[4].fg = 0xFF64748B;
}

int gui_open_terminal(void) {
    if (!gui_entered) return -1;
    if (gui_term_window.id != WINDOW_ID_NONE) {
        wm_raise_window(gui_term_window.id);
        wm_request_redraw();
        return 0;
    }

    int w = graphics_get_width() / 2;
    int h = graphics_get_height() / 2;
    window_t* win = wm_create_window("Greenhouse Terminal", graphics_get_width() - w - 24, 24, w, h, 0xFF34D399);
    if (!win) return -1;
    gui_term_window = *win;
    gui_term_window.on_draw = gui_term_draw;
    gui_term_window.on_event = gui_term_event;
    *wm_get_window(gui_term_window.id) = gui_term_window;

    guiterm_write_line(&gui_term, GREENHOUSE_VERSION_LINE);
    guiterm_write_line(&gui_term, "type 'help' for the command list");
    wm_request_redraw();
    return 0;
}

int gui_open_about(void) {
    if (!gui_entered) return -1;
    if (gui_about_open) return 0;

    int w = 380, h = 200;
    int x = (graphics_get_width() - w) / 2;
    int y = (graphics_get_height() - h) / 2;
    window_t* win = wm_create_window("About", x, y, w, h, 0xFF60A5FA);
    if (!win) return -1;
    gui_about_window = *win;
    gui_about_window.on_draw = gui_about_draw;
    gui_about_window.on_event = gui_about_event;
    *wm_get_window(gui_about_window.id) = gui_about_window;
    gui_about_open = 1;
    wm_request_redraw();
    return 0;
}

/* ------------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

void gui_init(void) {
    gui_running = 0;
    gui_entered = 0;
    gui_exit_reason = GUI_EXIT_NONE;
    gui_events_processed = 0;
    gui_animate = 1;
    gui_about_open = 0;
    gui_term_window.id = WINDOW_ID_NONE;
    gui_about_window.id = WINDOW_ID_NONE;
    guiterm_init(&gui_term);
    graphics_init();
    cursor_init();
}

int gui_enter(void) {
    if (gui_entered) return 0;

    int rc = graphics_enter();
    if (rc != 0) return rc;

    wm_init();
    cursor_init();

    int sw = graphics_get_width();
    int sh = graphics_get_height();

    gui_entered = 1;

    /* Control panel: bottom of the stack, on the left. */
    int pw = (sw > 640) ? 480 : sw - 40;
    int ph = (sh > 480) ? 260 : sh - 80;
    window_t* panel = wm_create_window("Greenhouse Control Panel", 24, sh - ph - 24, pw, ph, 0xFF38BDF8);
    if (!panel) { gui_entered = 0; return -1; }
    gui_control_window = *panel;
    gui_control_window.on_draw = gui_control_draw;
    gui_control_window.on_event = gui_control_event;
    *wm_get_window(gui_control_window.id) = gui_control_window;
    gui_build_widgets(panel->w, panel->h);

    gui_open_terminal();
    gui_open_about();

    input_set_pointer_bounds(sw, sh);
    input_set_pointer(sw / 2, sh / 2);
    gui_last_presents = (uint32_t)graphics_get_present_count();
    return 0;
}

void gui_leave(void) {
    if (!gui_entered) return;
    gui_entered = 0;
    gui_running = 0;
    wm_init();
    graphics_leave();
    cursor_set_visible(1);
    gui_term_window.id = WINDOW_ID_NONE;
    gui_about_window.id = WINDOW_ID_NONE;
    gui_about_open = 0;
}

/* ------------------------------------------------------------------------------
 * Event loop
 * -------------------------------------------------------------------------- */

static void gui_update_cursor_shape(int px, int py) {
    window_t* win = 0;
    int zone = 0;
    if (!wm_hit_test(px, py, &win, &zone)) {
        cursor_set_shape(CURSOR_ARROW);
        return;
    }

    if (zone == 3) {    /* resize zone: horizontal or vertical, by corner */
        int top = py - win->y;
        int horizontal = (top > WM_RESIZE_ZONE && top < win->h - WM_RESIZE_ZONE);
        cursor_set_shape(horizontal ? CURSOR_RESIZE_H : CURSOR_RESIZE_V);
        return;
    }

    if (zone == 2) {    /* title bar */
        cursor_set_shape(CURSOR_HAND);
        return;
    }

    /* Client area: hand when over an interactive widget. */
    if (win == wm_get_window(gui_control_window.id)) {
        int cx, cy, cw, ch;
        wm_client_rect(win, &cx, &cy, &cw, &ch);
        int lx = px - cx, ly = py - cy;
        for (int i = 0; i < 5; i++) {
            if (gui_widgets[i].type != WIDGET_BUTTON && gui_widgets[i].type != WIDGET_CHECKBOX) continue;
            if (lx >= gui_widgets[i].x && ly >= gui_widgets[i].y &&
                lx < gui_widgets[i].x + gui_widgets[i].w && ly < gui_widgets[i].y + gui_widgets[i].h) {
                cursor_set_shape(CURSOR_HAND);
                return;
            }
        }
    }

    cursor_set_shape(CURSOR_ARROW);
}

int gui_run(int max_seconds) {
    if (!gui_entered) return GUI_EXIT_ERROR;

    gui_running = 1;
    gui_exit_reason = GUI_EXIT_NONE;

    uint32_t hz = timer_get_frequency();
    if (hz == 0) hz = 100;

    uint64_t start = timer_get_ticks();
    uint64_t limit = (max_seconds > 0) ? (uint64_t)max_seconds * hz : 0;
    int frames = 0;

    while (gui_running) {
        input_event_t ev;
        int got_event = input_wait_event(&ev, GUI_REDRAW_INTERVAL);

        if (got_event) {
            gui_events_processed++;

            if (ev.type == INPUT_EVENT_KEY_DOWN && ev.ascii == 27 && !ev.extended) {
                gui_exit_reason = GUI_EXIT_ESC;
                break;
            }
            if (ev.type == INPUT_EVENT_MOUSE_MOVE) {
                gui_update_cursor_shape(ev.x, ev.y);
            }
            wm_handle_event(&ev);
        } else if (limit && (timer_get_ticks() - start) >= limit) {
            gui_exit_reason = GUI_EXIT_TIMEOUT;
            break;
        }

        if (!gui_animate && !wm_needs_redraw()) continue;

        graphics_begin_frame();
        wm_compose();

        int px = 0, py = 0;
        input_get_pointer(&px, &py);
        cursor_draw(px, py);

        graphics_end_frame();
        graphics_present();
        frames++;

        if (limit && (timer_get_ticks() - start) >= limit) {
            gui_exit_reason = GUI_EXIT_TIMEOUT;
            break;
        }
    }

    gui_running = 0;
    if (gui_exit_reason == GUI_EXIT_NONE) gui_exit_reason = GUI_EXIT_CLOSED;

    uint64_t presents = graphics_get_present_count();
    gui_last_report.width = graphics_get_width();
    gui_last_report.height = graphics_get_height();
    gui_last_report.bpp = graphics_get_bpp();
    gui_last_report.backend_multiboot =
        (framebuffer_get_info()->backend == FB_BACKEND_MULTIBOOT2);
    gui_last_report.back_buffer = graphics_has_back_buffer();
    gui_last_report.back_buffer_size = (uint64_t)graphics_get_back_buffer_size();
    gui_last_report.presents = (uint32_t)(presents - gui_last_presents);
    gui_last_report.frames = frames;
    gui_last_report.events_processed = gui_events_processed;
    gui_last_report.exit_reason = gui_exit_reason;
    return gui_exit_reason;
}

int  gui_is_running(void)    { return gui_running; }
void gui_request_exit(int reason) { gui_exit_reason = reason; }
int  gui_get_exit_reason(void)   { return gui_exit_reason; }
void gui_get_report(gui_report_t* out) { if (out) *out = gui_last_report; }
