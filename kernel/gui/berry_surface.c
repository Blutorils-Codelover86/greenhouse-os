/* ==============================================================================
 * Greenhouse OS - VERDANT Berry Assistant Surface (implementation)
 * ==============================================================================
 */

#include "berry_surface.h"
#include "compositor.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "../pmm.h"
#include "../heap.h"
#include "../process.h"
#include "../irq.h"
#include "../version.h"

static void b_strcpy(char* dst, int cap, const char* src) {
    int i = 0;
    if (src) {
        for (; src[i] && i < cap - 1; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int b_streq(const char* a, const char* b) {
    if (!a || !b) return 0;
    int i = 0;
    while (a[i] && (a[i] == b[i] || a[i] + 32 == b[i] || a[i] - 32 == b[i])) i++;
    return (a[i] == '\0' && b[i] == '\0');
}

static void b_put_uint(char* buf, int* n, int cap, uint64_t v) {
    char tmp[24];
    int tl = 0;
    do { tmp[tl++] = (char)('0' + (v % 10)); v /= 10; } while (v);
    while (tl && *n < cap - 1) buf[(*n)++] = tmp[--tl];
    buf[*n] = '\0';
}

static void b_add_line(berry_surface_state_t* st, const char* text, uint32_t color) {
    if (!st) return;
    if (st->line_count >= BERRY_CHAT_LINES) {
        for (int i = 1; i < BERRY_CHAT_LINES; i++) {
            b_strcpy(st->lines[i - 1], BERRY_LINE_LEN + 1, st->lines[i]);
            st->colors[i - 1] = st->colors[i];
        }
        st->line_count = BERRY_CHAT_LINES - 1;
    }
    b_strcpy(st->lines[st->line_count], BERRY_LINE_LEN + 1, text);
    st->colors[st->line_count] = color;
    st->line_count++;
}

void berry_surface_init(berry_surface_state_t* st) {
    if (!st) return;
    st->line_count = 0;
    st->input[0] = '\0';
    st->input_len = 0;
    st->requested_action = 0;

    b_add_line(st, "Berry: Welcome to VERDANT on Greenhouse OS!", 0xFFF43F5E);
    b_add_line(st, "Berry: I am your spatial system assistant.", 0xFFE2E8F0);
    b_add_line(st, "Berry: Click a chip above or type a command below.", 0xFF94A3B8);
}

static void b_execute_command(berry_surface_state_t* st, const char* cmd) {
    char user_line[BERRY_LINE_LEN + 1];
    user_line[0] = '>';
    user_line[1] = ' ';
    b_strcpy(user_line + 2, BERRY_LINE_LEN - 1, cmd);
    b_add_line(st, user_line, 0xFF6EE7B7);

    if (b_streq(cmd, "help") || b_streq(cmd, "?")) {
        b_add_line(st, "Berry: Commands: terminal, memory, files, ps, status, about, clear", 0xFFFB7185);
    } else if (b_streq(cmd, "terminal") || b_streq(cmd, "term") || b_streq(cmd, "1")) {
        b_add_line(st, "Berry: Launching / raising Greenhouse Terminal...", 0xFF34D399);
        st->requested_action = 1; /* Terminal */
    } else if (b_streq(cmd, "memory") || b_streq(cmd, "ram") || b_streq(cmd, "mem") || b_streq(cmd, "2")) {
        pmm_stats_t pmm = pmm_get_stats();
        heap_stats_t hp = heap_get_stats();

        char rep[BERRY_LINE_LEN + 1];
        int n = 0;
        b_strcpy(rep, sizeof(rep), "Berry: PMM RAM: ");
        n = 16;
        b_put_uint(rep, &n, sizeof(rep), pmm.usable_memory_bytes / (1024 * 1024));
        b_strcpy(rep + n, sizeof(rep) - n, " MB usable, Free: ");
        n += 18;
        b_put_uint(rep, &n, sizeof(rep), (pmm.free_frames * 4096) / (1024 * 1024));
        b_strcpy(rep + n, sizeof(rep) - n, " MB");
        b_add_line(st, rep, 0xFF38BDF8);

        char rep2[BERRY_LINE_LEN + 1];
        n = 0;
        b_strcpy(rep2, sizeof(rep2), "Berry: Heap: ");
        n = 13;
        b_put_uint(rep2, &n, sizeof(rep2), hp.used_bytes / 1024);
        b_strcpy(rep2 + n, sizeof(rep2) - n, " KB used, Free: ");
        n += 16;
        b_put_uint(rep2, &n, sizeof(rep2), hp.free_bytes / 1024);
        b_strcpy(rep2 + n, sizeof(rep2) - n, " KB");
        b_add_line(st, rep2, 0xFF38BDF8);
    } else if (b_streq(cmd, "files") || b_streq(cmd, "dir") || b_streq(cmd, "3")) {
        b_add_line(st, "Berry: Launching / raising File Browser (C: FAT32, R: RAMFS)...", 0xFF38BDF8);
        st->requested_action = 2; /* Files */
    } else if (b_streq(cmd, "ps") || b_streq(cmd, "tasks") || b_streq(cmd, "sysmon") || b_streq(cmd, "4")) {
        int cnt = process_count();
        char rep[BERRY_LINE_LEN + 1];
        int n = 0;
        b_strcpy(rep, sizeof(rep), "Berry: Active Processes: ");
        n = 25;
        b_put_uint(rep, &n, sizeof(rep), (uint64_t)cnt);
        b_strcpy(rep + n, sizeof(rep) - n, " tasks running in Ring 3 / Ring 0.");
        b_add_line(st, rep, 0xFFF59E0B);
        st->requested_action = 3; /* SysMon */
    } else if (b_streq(cmd, "status") || b_streq(cmd, "uptime") || b_streq(cmd, "5")) {
        uint64_t ticks = timer_get_ticks();
        char rep[BERRY_LINE_LEN + 1];
        int n = 0;
        b_strcpy(rep, sizeof(rep), "Berry: System Healthy. Uptime: ");
        n = 31;
        b_put_uint(rep, &n, sizeof(rep), ticks / 100);
        b_strcpy(rep + n, sizeof(rep) - n, "s (Ticks: ");
        n += 10;
        b_put_uint(rep, &n, sizeof(rep), ticks);
        b_strcpy(rep + n, sizeof(rep) - n, ")");
        b_add_line(st, rep, 0xFF34D399);
    } else if (b_streq(cmd, "about") || b_streq(cmd, "whoami")) {
        b_add_line(st, "Berry: Greenhouse OS " GREENHOUSE_VERSION_STRING " (" GREENHOUSE_BUILD_TYPE ")", 0xFFFB7185);
        b_add_line(st, "Berry: Powered by 64-bit Long Mode and Verdant GUI.", 0xFFE2E8F0);
    } else if (b_streq(cmd, "clear") || b_streq(cmd, "cls")) {
        st->line_count = 0;
    } else {
        b_add_line(st, "Berry: I didn't recognize that command. Type 'help' for options.", 0xFFF87171);
    }
}

void berry_surface_draw(surface_t* s, int cx, int cy, int cw, int ch) {
    berry_surface_state_t* st = (berry_surface_state_t*)s->user_data;
    if (!st) return;

    /* 1. Header Card */
    graphics_fill_gradient_h(cx + 8, cy + 8, cw - 16, 26, 0xFF2A0F1E, 0xFF140810);
    graphics_draw_rounded_rect(cx + 8, cy + 8, cw - 16, 26, 4, 0xFFF43F5E);
    draw_text(cx + 14, cy + 13, "[*] BERRY ASSISTANT", 0xFFFB7185, FONT_TRANSPARENT, 1);
    draw_text(cx + cw - 110, cy + 13, "[● READY]", 0xFF34D399, FONT_TRANSPARENT, 0);

    /* 2. Quick Action Chips */
    const char* chips[] = { "1:Term", "2:Mem", "3:Files", "4:Tasks", "5:Status" };
    int chip_x = cx + 8;
    int chip_y = cy + 40;
    for (int i = 0; i < 5; i++) {
        graphics_fill_rounded_rect(chip_x, chip_y, 64, 20, 4, 0xFF1A1526);
        graphics_draw_rounded_rect(chip_x, chip_y, 64, 20, 4, 0xFF818CF8);
        draw_text(chip_x + 6, chip_y + 4, chips[i], 0xFFE0E7FF, FONT_TRANSPARENT, 0);
        chip_x += 70;
    }

    /* 3. Chat History */
    int chat_y = cy + 68;
    int gh = font_glyph_height();
    for (int i = 0; i < st->line_count; i++) {
        draw_text_clipped(cx + 12, chat_y + i * gh, cw - 24, st->lines[i], st->colors[i], FONT_TRANSPARENT, 0);
    }

    /* 4. Input Prompt Box at Bottom */
    int in_box_y = cy + ch - 32;
    graphics_fill_rounded_rect(cx + 8, in_box_y, cw - 16, 26, 4, 0xFF0E1726);
    graphics_draw_rounded_rect(cx + 8, in_box_y, cw - 16, 26, 4, 0xFFF43F5E);

    draw_text(cx + 14, in_box_y + 5, "Berry> ", 0xFFF43F5E, FONT_TRANSPARENT, 0);
    draw_text_clipped(cx + 62, in_box_y + 5, cw - 80, st->input, 0xFFF1F5F9, FONT_TRANSPARENT, 0);

    /* Blinking cursor */
    if (((timer_get_ticks() / 25) & 1) == 0) {
        int cur_x = cx + 62 + st->input_len * (font_glyph_width() + 0);
        graphics_fill_rect(cur_x, in_box_y + 5, font_glyph_width(), gh, 0xFFF43F5E);
    }
}

int berry_surface_event(surface_t* s, const input_event_t* ev) {
    berry_surface_state_t* st = (berry_surface_state_t*)s->user_data;
    if (!st || !ev) return 0;

    int cx, cy, cw, ch;
    compositor_client_rect(s, &cx, &cy, &cw, &ch);

    /* Chip clicks */
    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        int chip_x = cx + 8;
        int chip_y = cy + 40;
        for (int i = 0; i < 5; i++) {
            if (ev->x >= chip_x && ev->x < chip_x + 64 && ev->y >= chip_y && ev->y < chip_y + 20) {
                if (i == 0) b_execute_command(st, "terminal");
                else if (i == 1) b_execute_command(st, "memory");
                else if (i == 2) b_execute_command(st, "files");
                else if (i == 3) b_execute_command(st, "ps");
                else if (i == 4) b_execute_command(st, "status");
                return 1;
            }
            chip_x += 70;
        }
        return 0;
    }

    if (ev->type != INPUT_EVENT_KEY_DOWN) return 0;

    uint8_t c = ev->ascii;
    if (c == '\r' || c == '\n') {
        if (st->input_len > 0) {
            char cmd[BERRY_LINE_LEN + 1];
            b_strcpy(cmd, sizeof(cmd), st->input);
            st->input[0] = '\0';
            st->input_len = 0;
            b_execute_command(st, cmd);
        }
        return 1;
    }

    if (c == INPUT_KEY_BACKSPACE) {
        if (st->input_len > 0) {
            st->input_len--;
            st->input[st->input_len] = '\0';
        }
        return 1;
    }

    if (c >= 0x20 && c < 0x7F && st->input_len < BERRY_LINE_LEN - 10) {
        st->input[st->input_len++] = (char)c;
        st->input[st->input_len] = '\0';
        return 1;
    }

    return 0;
}
