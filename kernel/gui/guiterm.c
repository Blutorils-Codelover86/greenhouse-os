/* ==============================================================================
 * Greenhouse OS - Phase 7: Graphical Terminal (implementation)
 * ==============================================================================
 */

#include "guiterm.h"
#include "wm.h"
#include "cursor.h"
#include "../version.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "../graphics/framebuffer.h"
#include "../input/input.h"
#include "../input/mouse.h"
#include "../input/kbd.h"
#include "../pmm.h"
#include "../heap.h"
#include "../process.h"
#include "../vfs.h"
#include "../irq.h"
#include "../vmm.h"
#include "../elf.h"
#include "../syscall.h"
#include "../cpu.h"
#include "renderer.h"
#include "gh_theme.h"
#include "../os_shell.h"
#include "verdant.h"


static int gt_copy(char* dst, int cap, const char* src) {
    int n = 0;
    if (src) {
        for (; src[n] && n < cap - 1; n++) dst[n] = src[n];
    }
    dst[n] = '\0';
    return n;
}

static int gt_streq(const char* a, const char* b) {
    if (!a || !b) return 0;
    int i = 0;
    while (a[i] && a[i] == b[i]) i++;
    return a[i] == b[i];
}

/* Small formatting helpers (no libc in the kernel). */
static int gt_put_str(char* buf, int n, int cap, const char* s) {
    while (*s && n < cap - 1) buf[n++] = *s++;
    buf[n] = '\0';
    return n;
}

static int gt_put_uint(char* buf, int n, int cap, uint64_t v) {
    char tmp[24];
    int tl = 0;
    do { tmp[tl++] = (char)('0' + (v % 10)); v /= 10; } while (v);
    while (tl && n < cap - 1) buf[n++] = tmp[--tl];
    buf[n] = '\0';
    return n;
}

static int gt_put_int(char* buf, int n, int cap, int v) {
    if (v < 0) {
        buf[n++] = '-';
        v = -v;
    }
    return gt_put_uint(buf, n, cap, (uint64_t)v);
}

static int gt_put_hex(char* buf, int n, int cap, uint64_t v) {
    static const char digits[] = "0123456789ABCDEF";
    char tmp[20];
    int tl = 0;
    if (v == 0) tmp[tl++] = '0';
    while (v) { tmp[tl++] = digits[v & 0xF]; v >>= 4; }
    if (tl < 2) tmp[tl++] = '0';
    buf[n++] = '0';
    buf[n++] = 'x';
    while (tl && n < cap - 1) buf[n++] = tmp[--tl];
    buf[n] = '\0';
    return n;
}

/* Split off the first word of a command line. */
static int gt_token(const char* line, char* out, int cap, const char** rest) {
    while (*line == ' ') line++;
    int i = 0;
    while (line[i] && line[i] != ' ' && i < cap - 1) {
        out[i] = line[i];
        i++;
    }
    out[i] = '\0';
    while (line[i] == ' ') i++;
    if (rest) *rest = line + i;
    return i;
}

void guiterm_init(guiterm_t* t) {
    if (!t) return;
    for (int i = 0; i < GTERM_LINES; i++) t->lines[i][0] = '\0';
    t->count = 0;
    t->scroll = 0;
    t->history_count = 0;
    t->history_pos = -1;
    t->input[0] = '\0';
    t->input_len = 0;
    t->fg = GH_COLOR_TEXT_PRIMARY;         /* Off-white ink text */
    t->bg = GH_COLOR_CODE_BG;              /* Deep obsidian-emerald code canvas */
    t->prompt_color = GH_COLOR_GREEN_SPROUT; /* Vibrant mint-sprout prompt */
    t->accent = GH_COLOR_GREEN_LEAF;        /* Brand leaf accent */
    t->commands_run = 0;

    guiterm_write_line(t, "Greenhouse OS 1.1.0 -- Berry Shell");
    guiterm_write_line(t, "Your hardware, your software. Type 'help' for commands.");
    guiterm_write_line(t, "");
}

void guiterm_write_line(guiterm_t* t, const char* text) {
    if (!t) return;

    if (t->count >= GTERM_LINES) {
        for (int i = 1; i < GTERM_LINES; i++) {
            for (int c = 0; c <= GTERM_COLS; c++) t->lines[i - 1][c] = t->lines[i][c];
        }
        t->count = GTERM_LINES - 1;
    }

    gt_copy(t->lines[t->count], GTERM_COLS + 1, text ? text : "");
    t->count++;
    t->scroll = t->count;
}

void guiterm_printf(guiterm_t* t, const char* text) { guiterm_write_line(t, text); }

void guiterm_clear(guiterm_t* t) {
    if (!t) return;
    for (int i = 0; i < GTERM_LINES; i++) t->lines[i][0] = '\0';
    t->count = 0;
    t->scroll = 0;
    t->input[0] = '\0';
    t->input_len = 0;
}

static void gt_cmd_gwinfo(guiterm_t* t) {
    const framebuffer_info_t* fb = framebuffer_get_info();
    char buf[128];
    int n = 0;
    buf[0] = '\0';

    if (fb) {
        n = gt_put_str(buf, n, sizeof(buf), "mode: ");
        n = gt_put_uint(buf, n, sizeof(buf), fb->width);
        n = gt_put_str(buf, n, sizeof(buf), "x");
        n = gt_put_uint(buf, n, sizeof(buf), fb->height);
        n = gt_put_str(buf, n, sizeof(buf), " ");
        n = gt_put_uint(buf, n, sizeof(buf), fb->bpp);
        n = gt_put_str(buf, n, sizeof(buf), "bpp pitch ");
        n = gt_put_uint(buf, n, sizeof(buf), fb->pitch);
        guiterm_write_line(t, buf);

        n = 0;
        n = gt_put_str(buf, n, sizeof(buf), "fb at ");
        n = gt_put_hex(buf, n, sizeof(buf), fb->address);
        guiterm_write_line(t, buf);

        n = 0;
        n = gt_put_str(buf, n, sizeof(buf),
                       (fb->backend == FB_BACKEND_MULTIBOOT2) ? "backend: multiboot2 framebuffer tag"
                                                             : "backend: legacy vbe (bochs registers)");
        guiterm_write_line(t, buf);
    } else {
        guiterm_write_line(t, "no framebuffer active");
    }

    n = 0;
    n = gt_put_str(buf, n, sizeof(buf), "surface: ");
    n = gt_put_uint(buf, n, sizeof(buf), (uint64_t)graphics_get_width());
    n = gt_put_str(buf, n, sizeof(buf), "x");
    n = gt_put_uint(buf, n, sizeof(buf), (uint64_t)graphics_get_height());
    n = gt_put_str(buf, n, sizeof(buf), graphics_has_back_buffer() ? "  back buffer: allocated" : "  back buffer: none (direct)");
    guiterm_write_line(t, buf);

    n = 0;
    n = gt_put_str(buf, n, sizeof(buf), "presents: ");
    n = gt_put_uint(buf, n, sizeof(buf), graphics_get_present_count());
    guiterm_write_line(t, buf);
}

static void gt_cmd_windows(guiterm_t* t) {
    char buf[128];
    int n = gt_put_uint(buf, 0, sizeof(buf), (uint64_t)wm_window_count());
    gt_put_str(buf, n, sizeof(buf), " window(s), top to bottom:");
    guiterm_write_line(t, buf);

    for (int i = wm_window_count() - 1; i >= 0; i--) {
        window_t* win = wm_window_at_index(i);
        if (!win) continue;

        int k = gt_put_str(buf, 0, sizeof(buf), "  #");
        k = gt_put_uint(buf, k, sizeof(buf), win->id);
        k = gt_put_str(buf, k, sizeof(buf), " [");
        k = gt_put_str(buf, k, sizeof(buf), win->title);
        k = gt_put_str(buf, k, sizeof(buf), "]  ");
        k = gt_put_uint(buf, k, sizeof(buf), (uint64_t)win->w);
        k = gt_put_str(buf, k, sizeof(buf), "x");
        k = gt_put_uint(buf, k, sizeof(buf), (uint64_t)win->h);
        k = gt_put_str(buf, k, sizeof(buf), (win == wm_focused_window()) ? "  focused" : "");
        guiterm_write_line(t, buf);
    }
}

static void gt_cmd_pointer(guiterm_t* t) {
    int x = 0, y = 0;
    input_get_pointer(&x, &y);

    char buf[96];
    int n = gt_put_str(buf, 0, sizeof(buf), "position: ");
    n = gt_put_int(buf, n, sizeof(buf), x);
    n = gt_put_str(buf, n, sizeof(buf), ", ");
    n = gt_put_int(buf, n, sizeof(buf), y);
    guiterm_write_line(t, buf);

    n = gt_put_str(buf, 0, sizeof(buf), "bounds: ");
    n = gt_put_int(buf, n, sizeof(buf), graphics_get_width() - 1);
    n = gt_put_str(buf, n, sizeof(buf), " x ");
    n = gt_put_int(buf, n, sizeof(buf), graphics_get_height() - 1);
    guiterm_write_line(t, buf);

    int buttons = input_get_buttons();
    n = gt_put_str(buf, 0, sizeof(buf), "buttons: ");
    if (buttons & INPUT_MOUSE_LEFT)   n = gt_put_str(buf, n, sizeof(buf), "left ");
    if (buttons & INPUT_MOUSE_RIGHT)  n = gt_put_str(buf, n, sizeof(buf), "right ");
    if (buttons & INPUT_MOUSE_MIDDLE) n = gt_put_str(buf, n, sizeof(buf), "middle ");
    if (!buttons) n = gt_put_str(buf, n, sizeof(buf), "none");
    n = gt_put_str(buf, n, sizeof(buf), "   cursor: ");
    n = gt_put_str(buf, n, sizeof(buf), cursor_shape_name(cursor_get_shape()));
    guiterm_write_line(t, buf);
}

static void gt_cmd_input(guiterm_t* t) {
    input_stats_t st;
    input_get_stats(&st);

    char buf[128];
    int n = gt_put_str(buf, 0, sizeof(buf), "events posted ");
    n = gt_put_uint(buf, n, sizeof(buf), st.posted);
    n = gt_put_str(buf, n, sizeof(buf), ", delivered ");
    n = gt_put_uint(buf, n, sizeof(buf), st.delivered);
    n = gt_put_str(buf, n, sizeof(buf), ", dropped ");
    n = gt_put_uint(buf, n, sizeof(buf), st.dropped);
    guiterm_write_line(t, buf);

    n = gt_put_str(buf, 0, sizeof(buf), "key down ");
    n = gt_put_uint(buf, n, sizeof(buf), st.key_down);
    n = gt_put_str(buf, n, sizeof(buf), ", key up ");
    n = gt_put_uint(buf, n, sizeof(buf), st.key_up);
    guiterm_write_line(t, buf);

    n = gt_put_str(buf, 0, sizeof(buf), "mouse moves ");
    n = gt_put_uint(buf, n, sizeof(buf), st.mouse_move);
    n = gt_put_str(buf, n, sizeof(buf), ", button events ");
    n = gt_put_uint(buf, n, sizeof(buf), st.mouse_buttons);
    guiterm_write_line(t, buf);

    n = gt_put_str(buf, 0, sizeof(buf), "keyboard scancodes ");
    n = gt_put_uint(buf, n, sizeof(buf), kbd_get_scancode_count());
    n = gt_put_str(buf, n, sizeof(buf), ", queue size ");
    n = gt_put_uint(buf, n, sizeof(buf), (uint64_t)INPUT_QUEUE_SIZE);
    guiterm_write_line(t, buf);

    n = gt_put_str(buf, 0, sizeof(buf), "mouse: ");
    n = gt_put_str(buf, n, sizeof(buf), mouse_get_name());
    n = gt_put_str(buf, n, sizeof(buf), ", packets ");
    n = gt_put_uint(buf, n, sizeof(buf), mouse_get_packet_count());
    guiterm_write_line(t, buf);
}

static void gt_cmd_berry(guiterm_t* t, const char* arg) {
    if (!arg || !arg[0]) {
        guiterm_write_line(t, "Berry: Greenhouse OS AI assistant ready. Ask me anything!");
    } else {
        guiterm_write_line(t, "Berry: Processed request via Greenhouse cognitive core.");
    }
}

static void guiterm_shell_output_cb(const char* line, void* ctx) {
    guiterm_t* t = (guiterm_t*)ctx;
    if (t) {
        guiterm_write_line(t, line);
    }
}

int guiterm_run_elf(guiterm_t* t, const char* filepath) {
    if (!t || !filepath) return -1;
    while (*filepath == ' ') filepath++;
    if (!*filepath) {
        guiterm_write_line(t, "Usage: run <program.elf>");
        return -1;
    }

    os_shell_set_output_hook(guiterm_shell_output_cb, t);
    int rc = os_shell_run_elf(filepath, 0, NULL);
    os_shell_set_output_hook(NULL, NULL);
    return rc;
}

void guiterm_execute(guiterm_t* t, const char* line) {
    if (!t) return;

    /* Trim leading spaces */
    const char* p = line ? line : "";
    while (*p == ' ') p++;

    /* Echo input line with greenhouse> prompt */
    char echoed[GTERM_COLS + 16];
    int n = 0;
    const char* lead = "greenhouse> ";
    while (*lead && n < GTERM_COLS) echoed[n++] = *lead++;
    const char* orig = p;
    while (*p && n < GTERM_COLS) echoed[n++] = *p++;
    echoed[n] = '\0';
    guiterm_write_line(t, echoed);

    if (*orig == '\0') {
        return;
    }

    t->commands_run++;

    char first_word[32];
    const char* rest = NULL;
    gt_token(orig, first_word, sizeof(first_word), &rest);

    /* Check for logout / exit command */
    if (gt_streq(first_word, "logout") || gt_streq(first_word, "exit") || gt_streq(first_word, "quit")) {
        guiterm_write_line(t, "Logging out of Verdant graphical session...");
        verdant_request_exit(VERDANT_EXIT_CLOSED);
        return;
    }

    /* Check for terminal clear */
    if (gt_streq(first_word, "clear") || gt_streq(first_word, "cls")) {
        guiterm_clear(t);
        return;
    }



    /* Verdant GUI diagnostics helpers */
    if (gt_streq(first_word, "gwinfo")) {
        gt_cmd_gwinfo(t);
        return;
    }
    if (gt_streq(first_word, "windows")) {
        gt_cmd_windows(t);
        return;
    }
    if (gt_streq(first_word, "pointer")) {
        gt_cmd_pointer(t);
        return;
    }
    if (gt_streq(first_word, "input")) {
        gt_cmd_input(t);
        return;
    }
    if (gt_streq(first_word, "berry")) {
        gt_cmd_berry(t, rest);
        return;
    }

    /* Route all commands through Greenhouse OS unified command engine */
    os_shell_set_output_hook(guiterm_shell_output_cb, t);
    os_shell_execute(orig);
    os_shell_set_output_hook(NULL, NULL);
}

void guiterm_handle_event(guiterm_t* t, const input_event_t* ev) {
    if (!t || !ev) return;

    if (ev->type != INPUT_EVENT_KEY_DOWN) {
        if (ev->type == INPUT_EVENT_MOUSE_WHEEL) {
            if (ev->dy < 0) {
                if (t->scroll > 0) t->scroll--;
            } else {
                if (t->scroll < t->count - 1) t->scroll++;
            }
        }
        return;
    }

    if (ev->extended) {
        if (ev->ascii == INPUT_KEY_UP || ev->ascii == INPUT_KEY_DOWN) {
            if (t->history_count > 0) {
                if (ev->ascii == INPUT_KEY_UP) {
                    if (t->history_pos < t->history_count - 1) t->history_pos++;
                } else {
                    if (t->history_pos > 0) t->history_pos--;
                }
                const char* src = (t->history_pos < 0) ? "" : t->history[t->history_pos];
                t->input_len = gt_copy(t->input, GTERM_COLS + 1, src);
            }
        }
        return;
    }

    uint8_t c = ev->ascii;
    if (!c) return;

    if (c == '\r' || c == '\n') {
        t->input[t->input_len] = '\0';
        if (t->history_count < GTERM_HISTORY) {
            gt_copy(t->history[t->history_count], GTERM_COLS + 1, t->input);
            t->history_count++;
        } else {
            for (int i = 1; i < GTERM_HISTORY; i++) {
                for (int c2 = 0; c2 <= GTERM_COLS; c2++) t->history[i - 1][c2] = t->history[i][c2];
            }
            gt_copy(t->history[GTERM_HISTORY - 1], GTERM_COLS + 1, t->input);
        }
        t->history_pos = -1;
        char line[GTERM_COLS + 1];
        gt_copy(line, GTERM_COLS + 1, t->input);
        t->input[0] = '\0';
        t->input_len = 0;
        guiterm_execute(t, line);
        return;
    }

    if (c == 27) {
        /* ESC cancels the current input line without leaving GUI */
        t->input[0] = '\0';
        t->input_len = 0;
        t->history_pos = -1;
        return;
    }

    if (c == INPUT_KEY_BACKSPACE) {

        if (t->input_len > 0) {
            t->input_len--;
            t->input[t->input_len] = '\0';
            t->history_pos = -1;
        }
        return;
    }

    if (c >= 0x20 && c < 0x7F && t->input_len < GTERM_COLS) {
        t->input[t->input_len++] = (char)c;
        t->input[t->input_len] = '\0';
        t->history_pos = -1;
    }
}

void guiterm_draw(guiterm_t* t, int cx, int cy, int cw, int ch) {
    if (!t) return;

    /* Deep obsidian-emerald code canvas background */
    renderer_fill_alpha_rect(cx, cy, cw, ch, GH_COLOR_CODE_BG, 248);

    int pad = GTERM_PAD + 4;
    int gw = font_glyph_width() + 1;
    int gh = font_glyph_height() + 1;
    int cols = (cw - 2 * pad) / gw;
    int rows = (ch - 2 * pad) / gh;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (cols > GTERM_COLS) cols = GTERM_COLS;

    /* Keep the view pinned to the bottom unless the user scrolled up. */
    if (t->scroll >= t->count) t->scroll = t->count > rows ? t->count - rows : 0;
    if (t->scroll < 0) t->scroll = 0;

    int first = t->scroll;
    if (t->count - t->scroll < rows) first = t->count - rows;
    if (first < 0) first = 0;
    if (first > t->count - 1) first = (t->count > 0) ? t->count - 1 : 0;

    for (int r = 0; r < rows - 1; r++) {
        int index = first + r;
        if (index >= t->count) break;
        draw_text_clipped(cx + pad, cy + pad + r * gh, cols * gw,
                          t->lines[index], t->fg, FONT_TRANSPARENT, 0);
    }

    /* Prompt line: always the last row, styled like a real terminal */
    int prompt_y = cy + pad + (rows - 1) * gh;
    const char* prompt_str = "greenhouse> ";
    int p_chars = 12;
    int p_width = p_chars * gw;
    draw_text(cx + pad, prompt_y, prompt_str, t->prompt_color, FONT_TRANSPARENT, 0);
    draw_text_clipped(cx + pad + p_width, prompt_y, (cols - p_chars) * gw, t->input, t->fg, FONT_TRANSPARENT, 0);

    /* Blinking block cursor with soft rounded pill */
    if (((timer_get_ticks() / 25) & 1) == 0) {
        int cur_x = cx + pad + p_width + t->input_len * gw;
        renderer_fill_alpha_rounded_rect(cur_x, prompt_y + 1, font_glyph_width(), gh - 2, 2, t->prompt_color, 220);
    }

    /* Subtle scroll indicator */
    if (t->count > rows) {
        int bar_x = cx + cw - 6;
        int bar_h = ch - 2 * pad;
        renderer_fill_alpha_rounded_rect(bar_x, cy + pad, 3, bar_h, 1, GH_COLOR_BORDER_LIGHT, 150);
        int visible = rows * bar_h / t->count;
        if (visible < 8) visible = 8;
        int offset = (t->count - rows) ? (t->scroll * (bar_h - visible)) / (t->count - rows) : 0;
        renderer_fill_alpha_rounded_rect(bar_x, cy + pad + offset, 3, visible, 1, t->accent, 200);
    }
}
