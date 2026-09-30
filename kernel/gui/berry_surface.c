/* ==============================================================================
 * Greenhouse OS - Light-Mode Berry Assistant Surface (implementation)
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
    st->mode = BERRY_MODE_EXPANDED;

    b_add_line(st, "Berry: Welcome to Greenhouse OS!", GH_COLOR_GREEN_LEAF_DEEP);
    b_add_line(st, "Berry: I am your spatial system assistant.", GH_COLOR_TEXT_PRIMARY);
    b_add_line(st, "Berry: Click a chip above or type a command below.", GH_COLOR_TEXT_MUTED);
}

void berry_surface_toggle_mode(surface_t* s) {
    if (!s) return;
    berry_surface_state_t* st = (berry_surface_state_t*)s->user_data;
    if (!st) return;

    if (st->mode == BERRY_MODE_EXPANDED) {
        st->mode = BERRY_MODE_COMPACT;
        s->target_w = 280;
        s->target_h = 96;
        surface_set_tag(s, "MINI");
    } else {
        st->mode = BERRY_MODE_EXPANDED;
        s->target_w = 460;
        s->target_h = 340;
        surface_set_tag(s, "BERRY");
    }
}

static void b_execute_command(berry_surface_state_t* st, const char* cmd) {
    char user_line[BERRY_LINE_LEN + 1];
    user_line[0] = '>';
    user_line[1] = ' ';
    b_strcpy(user_line + 2, BERRY_LINE_LEN - 1, cmd);
    b_add_line(st, user_line, GH_COLOR_GREEN_LEAF_DEEP);

    if (b_streq(cmd, "help") || b_streq(cmd, "?")) {
        b_add_line(st, "Berry: Commands: terminal, memory, files, ps, status, quote, compact, clear", GH_COLOR_AMBER_WARN);
    } else if (b_streq(cmd, "quote") || b_streq(cmd, "wisdom")) {
        b_add_line(st, "Berry: \"The best way to understand a computer", GH_COLOR_AMBER_WARN);
        b_add_line(st, "        is to write its operating system.\"", GH_COLOR_AMBER_WARN);
        b_add_line(st, "        -- Vivaan, Berry-Tech", GH_COLOR_TEXT_PRIMARY);
    } else if (b_streq(cmd, "compact") || b_streq(cmd, "mini") || b_streq(cmd, "morph")) {
        b_add_line(st, "Berry: Morphing into compact companion mode...", GH_COLOR_RED_ERROR);
        st->requested_action = 99; /* Toggle mode */
    } else if (b_streq(cmd, "expand") || b_streq(cmd, "full")) {
        b_add_line(st, "Berry: Morphing into full assistant window...", GH_COLOR_GREEN_LEAF_DEEP);
        st->requested_action = 99; /* Toggle mode */
    } else if (b_streq(cmd, "terminal") || b_streq(cmd, "term") || b_streq(cmd, "1")) {
        b_add_line(st, "Berry: Launching / raising Greenhouse Terminal...", GH_COLOR_GREEN_LEAF_DEEP);
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
        b_add_line(st, rep, GH_COLOR_BLUE_INFO);

        char rep2[BERRY_LINE_LEN + 1];
        n = 0;
        b_strcpy(rep2, sizeof(rep2), "Berry: Heap: ");
        n = 13;
        b_put_uint(rep2, &n, sizeof(rep2), hp.used_bytes / 1024);
        b_strcpy(rep2 + n, sizeof(rep2) - n, " KB used, Free: ");
        n += 16;
        b_put_uint(rep2, &n, sizeof(rep2), hp.free_bytes / 1024);
        b_strcpy(rep2 + n, sizeof(rep2) - n, " KB");
        b_add_line(st, rep2, GH_COLOR_BLUE_INFO);
    } else if (b_streq(cmd, "files") || b_streq(cmd, "dir") || b_streq(cmd, "3")) {
        b_add_line(st, "Berry: Launching / raising File Browser (C: FAT32, R: RAMFS)...", GH_COLOR_BLUE_INFO);
        st->requested_action = 2; /* Files */
    } else if (b_streq(cmd, "ps") || b_streq(cmd, "tasks") || b_streq(cmd, "sysmon") || b_streq(cmd, "4")) {
        int cnt = process_count();
        char rep[BERRY_LINE_LEN + 1];
        int n = 0;
        b_strcpy(rep, sizeof(rep), "Berry: Active Processes: ");
        n = 25;
        b_put_uint(rep, &n, sizeof(rep), (uint64_t)cnt);
        b_strcpy(rep + n, sizeof(rep) - n, " tasks running in Ring 3 / Ring 0.");
        b_add_line(st, rep, GH_COLOR_AMBER_WARN);
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
        b_add_line(st, rep, GH_COLOR_GREEN_LEAF_DEEP);
    } else if (b_streq(cmd, "about") || b_streq(cmd, "whoami")) {
        b_add_line(st, "Berry: Greenhouse OS " GREENHOUSE_VERSION_STRING " (" GREENHOUSE_BUILD_TYPE ")", GH_COLOR_RED_ERROR);
        b_add_line(st, "Berry: Powered by 64-bit Long Mode and Verdant GUI.", GH_COLOR_TEXT_PRIMARY);
    } else if (b_streq(cmd, "clear") || b_streq(cmd, "cls")) {
        st->line_count = 0;
    } else {
        b_add_line(st, "Berry: I didn't recognize that command. Type 'help' for options.", GH_COLOR_RED_ERROR);
    }
}

void berry_surface_draw(surface_t* s, int cx, int cy, int cw, int ch) {
    berry_surface_state_t* st = (berry_surface_state_t*)s->user_data;
    if (!st) return;

    if (st->mode == BERRY_MODE_COMPACT) {
        /* Compact Companion Pill */
        graphics_fill_gradient_h(cx + 4, cy + 4, cw - 8, 26, GH_COLOR_GREEN_LEAF_SOFT, GH_COLOR_GREEN_LEAF_DEEP);
        graphics_draw_rounded_rect(cx + 4, cy + 4, cw - 8, 26, 4, GH_COLOR_GREEN_LEAF);
        draw_text(cx + 10, cy + 9, "[*] BERRY COMPANION", GH_COLOR_WHITE, FONT_TRANSPARENT, 1);
        draw_text(cx + cw - 90, cy + 9, "[+] Expand", GH_COLOR_GREEN_LEAF_DEEP, FONT_TRANSPARENT, 0);

        int msg_y = cy + 36;
        const char* last_msg = (st->line_count > 0) ? st->lines[st->line_count - 1] : "Berry: System online & ready.";
        draw_text_clipped(cx + 10, msg_y, cw - 20, last_msg, GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
        return;
    }

    /* 1. Header Card */
    graphics_fill_gradient_h(cx + 8, cy + 8, cw - 16, 26, GH_COLOR_GREEN_LEAF_SOFT, GH_COLOR_GREEN_LEAF_DEEP);
    graphics_draw_rounded_rect(cx + 8, cy + 8, cw - 16, 26, 4, GH_COLOR_GREEN_LEAF);
    draw_text(cx + 14, cy + 13, "[*] BERRY ASSISTANT", GH_COLOR_WHITE, FONT_TRANSPARENT, 1);
    draw_text(cx + cw - 170, cy + 13, "[-] Mini", GH_COLOR_BLUE_INFO, FONT_TRANSPARENT, 0);
    draw_text(cx + cw - 90, cy + 13, "[*] READY", GH_COLOR_GREEN_LEAF_DEEP, FONT_TRANSPARENT, 0);

    /* 2. Quick Action Chips */
    const char* chips[] = { "1:Term", "2:Mem", "3:Files", "4:Tasks", "5:Status", "6:Quote" };
    int chip_x = cx + 8;
    int chip_y = cy + 40;
    for (int i = 0; i < 6; i++) {
        if (chip_x + 58 > cx + cw - 8) break;
        graphics_fill_rounded_rect(chip_x, chip_y, 56, 20, 4, GH_COLOR_SURFACE_HOVER);
        graphics_draw_rounded_rect(chip_x, chip_y, 56, 20, 4, GH_COLOR_BORDER_LIGHT);
        draw_text(chip_x + 4, chip_y + 4, chips[i], GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
        chip_x += 62;
    }

    /* 3. Chat History */
    int chat_y = cy + 68;
    int gh = font_glyph_height();
    for (int i = 0; i < st->line_count; i++) {
        draw_text_clipped(cx + 12, chat_y + i * gh, cw - 24, st->lines[i], st->colors[i], FONT_TRANSPARENT, 0);
    }

    /* 4. Input Prompt Box at Bottom */
    int in_box_y = cy + ch - 32;
    graphics_fill_rounded_rect(cx + 8, in_box_y, cw - 16, 26, 4, GH_COLOR_SURFACE_HOVER);
    graphics_draw_rounded_rect(cx + 8, in_box_y, cw - 16, 26, 4, GH_COLOR_GREEN_LEAF);

    draw_text(cx + 14, in_box_y + 5, "Berry> ", GH_COLOR_GREEN_LEAF_DEEP, FONT_TRANSPARENT, 0);
    draw_text_clipped(cx + 62, in_box_y + 5, cw - 80, st->input, GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);

    /* Blinking cursor */
    if (((timer_get_ticks() / 25) & 1) == 0) {
        int cur_x = cx + 62 + st->input_len * (font_glyph_width() + 0);
        graphics_fill_rect(cur_x, in_box_y + 5, font_glyph_width(), gh, GH_COLOR_GREEN_LEAF);
    }
}

int berry_surface_event(surface_t* s, const input_event_t* ev) {
    berry_surface_state_t* st = (berry_surface_state_t*)s->user_data;
    if (!st || !ev) return 0;

    int cx, cy, cw, ch;
    compositor_client_rect(s, &cx, &cy, &cw, &ch);

    /* Mode toggle or chip clicks */
    if (ev->type == INPUT_EVENT_MOUSE_BUTTON_DOWN) {
        if (st->mode == BERRY_MODE_COMPACT) {
            /* Click anywhere in compact mode to expand */
            berry_surface_toggle_mode(s);
            return 1;
        }

        /* Check header '[-] Mini' button */
        if (ev->x >= cx + cw - 175 && ev->x <= cx + cw - 100 && ev->y >= cy + 8 && ev->y <= cy + 34) {
            berry_surface_toggle_mode(s);
            return 1;
        }

        int chip_x = cx + 8;
        int chip_y = cy + 40;
        for (int i = 0; i < 6; i++) {
            if (ev->x >= chip_x && ev->x < chip_x + 58 && ev->y >= chip_y && ev->y < chip_y + 20) {
                if (i == 0) b_execute_command(st, "terminal");
                else if (i == 1) b_execute_command(st, "memory");
                else if (i == 2) b_execute_command(st, "files");
                else if (i == 3) b_execute_command(st, "ps");
                else if (i == 4) b_execute_command(st, "status");
                else if (i == 5) b_execute_command(st, "quote");
                if (st->requested_action == 99) {
                    st->requested_action = 0;
                    berry_surface_toggle_mode(s);
                }
                return 1;
            }
            chip_x += 62;
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
            if (st->requested_action == 99) {
                st->requested_action = 0;
                berry_surface_toggle_mode(s);
            }
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