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
    t->fg = 0xFFCBD5E1;
    t->bg = 0xFF0A0F16;
    t->prompt_color = 0xFF6EE7B7;
    t->accent = 0xFF34D399;
    t->commands_run = 0;
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

static void gt_cmd_help(guiterm_t* t) {
    guiterm_write_line(t, "Verdant Terminal Commands:");
    guiterm_write_line(t, "  help      this command list");
    guiterm_write_line(t, "  clear     clear the terminal screen");
    guiterm_write_line(t, "  ver       Greenhouse OS version & kernel info");
    guiterm_write_line(t, "  gwinfo    framebuffer & compositor surface");
    guiterm_write_line(t, "  windows   window / surface manager state");
    guiterm_write_line(t, "  pointer   pointer position, buttons, cursor");
    guiterm_write_line(t, "  input     keyboard / mouse event statistics");
    guiterm_write_line(t, "  memory    physical memory manager usage");
    guiterm_write_line(t, "  heap      kernel heap usage & allocations");
    guiterm_write_line(t, "  ps        active process list");
    guiterm_write_line(t, "  dir       list files on current drive (C:\\)");
    guiterm_write_line(t, "  disks     mounted drives & filesystem status");
    guiterm_write_line(t, "  uptime    kernel PIT ticks and uptime");
    guiterm_write_line(t, "  berry     ask Berry AI assistant");
}

static void gt_cmd_ver(guiterm_t* t) {
    guiterm_write_line(t, "Greenhouse OS " GREENHOUSE_VERSION_STRING " " GREENHOUSE_BUILD_TYPE);
    guiterm_write_line(t, "kernel built " __DATE__ " " __TIME__);
    guiterm_write_line(t, "graphics: back buffer compositor, software cursor, input queue");
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

static void gt_cmd_memory(guiterm_t* t) {
    pmm_stats_t st = pmm_get_stats();

    char buf[128];
    int n = gt_put_str(buf, 0, sizeof(buf), "frames used ");
    n = gt_put_uint(buf, n, sizeof(buf), st.used_frames);
    n = gt_put_str(buf, n, sizeof(buf), " of ");
    n = gt_put_uint(buf, n, sizeof(buf), st.total_frames);
    n = gt_put_str(buf, n, sizeof(buf), " (free ");
    n = gt_put_uint(buf, n, sizeof(buf), st.free_frames);
    n = gt_put_str(buf, n, sizeof(buf), ")");
    guiterm_write_line(t, buf);

    n = gt_put_str(buf, 0, sizeof(buf), "usable ");
    n = gt_put_uint(buf, n, sizeof(buf), st.usable_memory_bytes / (1024 * 1024));
    n = gt_put_str(buf, n, sizeof(buf), " MiB of ");
    n = gt_put_uint(buf, n, sizeof(buf), st.total_memory_bytes / (1024 * 1024));
    n = gt_put_str(buf, n, sizeof(buf), " MiB total");
    guiterm_write_line(t, buf);

    n = gt_put_str(buf, 0, sizeof(buf), "regions reported: ");
    n = gt_put_uint(buf, n, sizeof(buf), (uint64_t)pmm_get_region_count());
    guiterm_write_line(t, buf);
}

static void gt_cmd_heap(guiterm_t* t) {
    heap_stats_t hp = heap_get_stats();
    char buf[128];
    int n = gt_put_str(buf, 0, sizeof(buf), "heap used: ");
    n = gt_put_uint(buf, n, sizeof(buf), hp.used_bytes / 1024);
    n = gt_put_str(buf, n, sizeof(buf), " KiB of ");
    n = gt_put_uint(buf, n, sizeof(buf), hp.total_bytes / 1024);
    n = gt_put_str(buf, n, sizeof(buf), " KiB total (free ");
    n = gt_put_uint(buf, n, sizeof(buf), hp.free_bytes / 1024);
    n = gt_put_str(buf, n, sizeof(buf), " KiB, active ");
    n = gt_put_uint(buf, n, sizeof(buf), (uint64_t)hp.active_allocs);
    n = gt_put_str(buf, n, sizeof(buf), ")");
    guiterm_write_line(t, buf);
}

static void gt_cmd_ps(guiterm_t* t) {
    int cnt = process_count();
    char buf[128];
    int n = gt_put_str(buf, 0, sizeof(buf), "PID   NAME                 STATE       MODE");
    guiterm_write_line(t, buf);

    for (int i = 0; i < cnt && i < 12; i++) {
        process_t* p = process_get_by_index((size_t)i);
        if (!p) continue;

        n = gt_put_uint(buf, 0, sizeof(buf), (uint64_t)p->pid);
        while (n < 6) buf[n++] = ' ';
        buf[n] = '\0';
        n = gt_put_str(buf, n, sizeof(buf), p->name);
        while (n < 27) buf[n++] = ' ';
        buf[n] = '\0';

        const char* st = "READY";
        if (p->state == PROCESS_RUNNING) st = "RUNNING";
        else if (p->state == PROCESS_SLEEPING) st = "SLEEPING";
        else if (p->state == PROCESS_BLOCKED) st = "BLOCKED";
        else if (p->state == PROCESS_TERMINATED) st = "DEAD";
        n = gt_put_str(buf, n, sizeof(buf), st);
        while (n < 39) buf[n++] = ' ';
        buf[n] = '\0';

        n = gt_put_str(buf, n, sizeof(buf), p->is_user ? "User" : "Kernel");
        guiterm_write_line(t, buf);
    }
}

static void gt_cmd_dir(guiterm_t* t) {
    vfs_node_t* node = vfs_resolve_path(NULL, "C:\\");
    if (!node) {
        guiterm_write_line(t, "cannot open C:\\ directory");
        return;
    }
    guiterm_write_line(t, "Directory of C:\\");
    vfs_dirent_t ent;
    for (uint32_t i = 0; i < 24; i++) {
        if (vfs_readdir(node, i, &ent) != 0) break;
        char buf[96];
        int n = gt_put_str(buf, 0, sizeof(buf), ent.is_dir ? "<DIR> " : "      ");
        n = gt_put_str(buf, n, sizeof(buf), ent.name);
        while (n < 28) buf[n++] = ' ';
        buf[n] = '\0';
        if (!ent.is_dir) {
            n = gt_put_uint(buf, n, sizeof(buf), (uint64_t)ent.size);
            n = gt_put_str(buf, n, sizeof(buf), " B");
        }
        guiterm_write_line(t, buf);
    }
}

static void gt_cmd_disks(guiterm_t* t) {
    vfs_drive_t* c = vfs_get_drive('C');
    vfs_drive_t* r = vfs_get_drive('R');

    char buf[128];
    if (c && c->is_mounted) {
        int n = gt_put_str(buf, 0, sizeof(buf), "Drive C: [");
        n = gt_put_str(buf, n, sizeof(buf), c->label);
        n = gt_put_str(buf, n, sizeof(buf), "] fs: ");
        n = gt_put_str(buf, n, sizeof(buf), c->fs_type);
        guiterm_write_line(t, buf);
    }
    if (r && r->is_mounted) {
        int n = gt_put_str(buf, 0, sizeof(buf), "Drive R: [");
        n = gt_put_str(buf, n, sizeof(buf), r->label);
        n = gt_put_str(buf, n, sizeof(buf), "] fs: ");
        n = gt_put_str(buf, n, sizeof(buf), r->fs_type);
        guiterm_write_line(t, buf);
    }
}

static void gt_cmd_berry(guiterm_t* t, const char* arg) {
    if (!arg || !arg[0]) {
        guiterm_write_line(t, "Berry: Greenhouse OS AI assistant ready. Ask me anything!");
    } else {
        guiterm_write_line(t, "Berry: Processed request via Greenhouse cognitive core.");
    }
}

static void gt_cmd_uptime(guiterm_t* t) {
    uint64_t ticks = timer_get_ticks();
    char buf[96];
    int n = gt_put_str(buf, 0, sizeof(buf), "ticks ");
    n = gt_put_uint(buf, n, sizeof(buf), ticks);
    n = gt_put_str(buf, n, sizeof(buf), "  (timer 100 Hz, kernel uptime)");
    guiterm_write_line(t, buf);
}

void guiterm_execute(guiterm_t* t, const char* line) {
    if (!t) return;

    char echoed[GTERM_COLS + 8];
    int n = 0;
    const char* lead = "verdant> ";
    while (*lead) echoed[n++] = *lead++;
    while (*line && n < GTERM_COLS) echoed[n++] = *line++;
    echoed[n] = '\0';
    guiterm_write_line(t, echoed);

    char cmd[32];
    const char* rest = 0;
    gt_token(line, cmd, sizeof(cmd), &rest);

    if (cmd[0] == '\0') {
        /* blank line: nothing to do */
    } else if (gt_streq(cmd, "help")) {
        gt_cmd_help(t);
    } else if (gt_streq(cmd, "clear") || gt_streq(cmd, "cls")) {
        guiterm_clear(t);
    } else if (gt_streq(cmd, "ver") || gt_streq(cmd, "version")) {
        gt_cmd_ver(t);
    } else if (gt_streq(cmd, "gwinfo")) {
        gt_cmd_gwinfo(t);
    } else if (gt_streq(cmd, "windows")) {
        gt_cmd_windows(t);
    } else if (gt_streq(cmd, "pointer")) {
        gt_cmd_pointer(t);
    } else if (gt_streq(cmd, "input")) {
        gt_cmd_input(t);
    } else if (gt_streq(cmd, "memory") || gt_streq(cmd, "mem")) {
        gt_cmd_memory(t);
    } else if (gt_streq(cmd, "heap")) {
        gt_cmd_heap(t);
    } else if (gt_streq(cmd, "ps") || gt_streq(cmd, "tasks")) {
        gt_cmd_ps(t);
    } else if (gt_streq(cmd, "dir") || gt_streq(cmd, "ls")) {
        gt_cmd_dir(t);
    } else if (gt_streq(cmd, "disks") || gt_streq(cmd, "vol")) {
        gt_cmd_disks(t);
    } else if (gt_streq(cmd, "berry")) {
        gt_cmd_berry(t, rest);
    } else if (gt_streq(cmd, "uptime")) {
        gt_cmd_uptime(t);
    } else {
        char unknown[GTERM_COLS + 1];
        int n2 = 0;
        const char* p1 = "unknown command: ";
        while (*p1 && n2 < GTERM_COLS - 20) unknown[n2++] = *p1++;
        int i = 0;
        while (cmd[i] && n2 < GTERM_COLS - 1) unknown[n2++] = cmd[i++];
        unknown[n2] = '\0';
        guiterm_write_line(t, unknown);
        guiterm_write_line(t, "try 'help'");
    }

    t->commands_run++;
    (void)rest;
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

    graphics_fill_rect(cx, cy, cw, ch, t->bg);

    int pad = GTERM_PAD;
    int gw = font_glyph_width() + 1;
    int gh = font_glyph_height();
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

    for (int r = 0; r < rows; r++) {
        int index = first + r;
        if (index >= t->count) break;
        draw_text_clipped(cx + pad, cy + pad + r * gh, cols * gw,
                          t->lines[index], t->fg, t->bg, 1);
    }

    /* Prompt line: always the last row, styled like a real terminal. */
    int prompt_y = cy + pad + (rows - 1) * gh;
    draw_text(cx + pad, prompt_y, "> ", t->accent, t->bg, 1);
    draw_text_clipped(cx + pad + 2 * gw, prompt_y, (cols - 2) * gw, t->input, t->fg, t->bg, 1);

    /* Blinking block cursor. */
    if (((timer_get_ticks() / 25) & 1) == 0) {
        int cur_x = cx + pad + 2 * gw + t->input_len * gw;
        graphics_fill_rect(cur_x, prompt_y, font_glyph_width(), gh, t->accent);
    }

    /* Scroll indicator. */
    if (t->count > rows) {
        int bar_x = cx + cw - 5;
        int bar_h = ch - 2 * pad;
        graphics_fill_rect(bar_x, cy + pad, 3, bar_h, 0xFF16202F);
        int visible = rows * bar_h / t->count;
        if (visible < 8) visible = 8;
        int offset = (t->count - rows) ? (t->scroll * (bar_h - visible)) / (t->count - rows) : 0;
        graphics_fill_rect(bar_x, cy + pad + offset, 3, visible, t->accent);
    }
}
