/* ==============================================================================
 * Greenhouse OS - Light-Mode Settings / Display Inspector Surface (implementation)
 * ==============================================================================
 */

#include "settings_surface.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "gh_theme.h"
#include "../graphics/framebuffer.h"
#include "../version.h"
#include "../cpu.h"

static void set_put_uint(char* buf, int* n, int cap, uint64_t v) {
    char tmp[24];
    int tl = 0;
    do { tmp[tl++] = (char)('0' + (v % 10)); v /= 10; } while (v);
    while (tl && *n < cap - 1) buf[(*n)++] = tmp[--tl];
    buf[*n] = '\0';
}

static void set_put_hex(char* buf, int* n, int cap, uint64_t v) {
    static const char digits[] = "0123456789ABCDEF";
    char tmp[20];
    int tl = 0;
    if (v == 0) tmp[tl++] = '0';
    while (v) { tmp[tl++] = digits[v & 0xF]; v >>= 4; }
    if (tl < 2) tmp[tl++] = '0';
    while (*n < cap - 3) {
        buf[(*n)++] = '0';
        buf[(*n)++] = 'x';
        break;
    }
    while (tl && *n < cap - 1) buf[(*n)++] = tmp[--tl];
    buf[*n] = '\0';
}

void settings_surface_init(void) {
}

void settings_surface_draw(surface_t* s, int cx, int cy, int cw, int ch) {
    (void)s; (void)ch;
    const framebuffer_info_t* fb = framebuffer_get_info();

    /* 1. Header */
    graphics_fill_gradient_h(cx + 8, cy + 6, cw - 16, 26, GH_COLOR_GREEN_LEAF_SOFT, GH_COLOR_GREEN_LEAF_DEEP);
    graphics_draw_rounded_rect(cx + 8, cy + 6, cw - 16, 26, 4, GH_COLOR_GREEN_LEAF);
    draw_text(cx + 14, cy + 11, "[*] SYSTEM PROPERTIES & VERDANT ENGINE", GH_COLOR_WHITE, FONT_TRANSPARENT, 1);

    /* 2. Top Card: Hardware & Compositor Specifications */
    int card1_y = cy + 38;
    int card1_h = 100;
    graphics_fill_rounded_rect(cx + 8, card1_y, cw - 16, card1_h, 6, GH_COLOR_SURFACE_HOVER);
    graphics_draw_rounded_rect(cx + 8, card1_y, cw - 16, card1_h, 6, GH_COLOR_BORDER_LIGHT);

    int y = card1_y + 8;
    int gh = font_glyph_height() + 4;

    /* Processor Info */
    const CPUInfo* cpu = get_cpu_info();
    draw_text(cx + 16, y, "Processor:", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    if (cpu && cpu->brand[0]) {
        draw_text_clipped(cx + 110, y, cw - 130, cpu->brand, GH_COLOR_GREEN_LEAF_DEEP, FONT_TRANSPARENT, 1);
    } else {
        draw_text(cx + 110, y, "x86_64 Compatible", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    }
    y += gh;

    /* Mode & Pitch */
    char line[128];
    int n = 0;
    line[0] = '\0';
    draw_text(cx + 16, y, "Display:", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    set_put_uint(line, &n, sizeof(line), (uint64_t)graphics_get_width());
    line[n++] = 'x';
    set_put_uint(line, &n, sizeof(line), (uint64_t)graphics_get_height());
    line[n++] = ' ';
    line[n++] = '@';
    line[n++] = ' ';
    set_put_uint(line, &n, sizeof(line), (uint64_t)graphics_get_bpp());
    line[n++] = ' ';
    line[n++] = 'b';
    line[n++] = 'p';
    line[n++] = 'p';
    line[n++] = ' ';
    line[n++] = '(';
    if (fb) {
        set_put_hex(line, &n, sizeof(line), fb->address);
    } else {
        line[n++] = '-';
    }
    line[n++] = ')';
    line[n++] = '\0';
    draw_text(cx + 110, y, line, GH_COLOR_GREEN_LEAF_DEEP, FONT_TRANSPARENT, 0);
    y += gh;

    /* Compositor Buffer & Presents */
    n = 0;
    line[0] = '\0';
    draw_text(cx + 16, y, "Compositor:", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    if (graphics_has_back_buffer()) {
        set_put_uint(line, &n, sizeof(line), (uint64_t)(graphics_get_back_buffer_size() / 1024));
        line[n++] = ' ';
        line[n++] = 'K';
        line[n++] = 'i';
        line[n++] = 'B';
        line[n++] = ' ';
        line[n++] = 'd';
        line[n++] = 'o';
        line[n++] = 'u';
        line[n++] = 'b';
        line[n++] = 'l';
        line[n++] = 'e';
        line[n++] = '-';
        line[n++] = 'b';
        line[n++] = 'u';
        line[n++] = 'f';
        line[n++] = 'f';
        line[n++] = 'e';
        line[n++] = 'r';
    } else {
        line[n++] = 'D'; line[n++] = 'i'; line[n++] = 'r'; line[n++] = 'e'; line[n++] = 'c'; line[n++] = 't';
    }
    line[n++] = ' ';
    line[n++] = '|';
    line[n++] = ' ';
    line[n++] = 'P';
    line[n++] = 'r';
    line[n++] = 'e';
    line[n++] = 's';
    line[n++] = 'e';
    line[n++] = 'n';
    line[n++] = 't';
    line[n++] = 's';
    line[n++] = ':';
    line[n++] = ' ';
    set_put_uint(line, &n, sizeof(line), graphics_get_present_count());
    line[n++] = '\0';
    draw_text(cx + 110, y, line, GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 0);
    y += gh;

    /* OS & Architecture */
    draw_text(cx + 16, y, "Platform:", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    draw_text(cx + 110, y, "Greenhouse OS (x86_64, Ring 3 ELF Multitasking)", GH_COLOR_TEXT_SECONDARY, FONT_TRANSPARENT, 0);

    /* 3. Bottom Card: Desktop Shortcuts & Window Management Reference */
    int card2_y = card1_y + card1_h + 8;
    int card2_h = ch - (card2_y - cy) - 8;
    if (card2_h < 130) card2_h = 130;
    graphics_fill_rounded_rect(cx + 8, card2_y, cw - 16, card2_h, 6, GH_COLOR_SURFACE_HOVER);
    graphics_draw_rounded_rect(cx + 8, card2_y, cw - 16, card2_h, 6, GH_COLOR_BORDER_LIGHT);

    int sy = card2_y + 8;
    draw_text(cx + 16, sy, "[=] WINDOW MANAGEMENT & DESKTOP SHORTCUTS", GH_COLOR_GREEN_LEAF_DEEP, FONT_TRANSPARENT, 1);
    sy += gh + 2;

    draw_text(cx + 16, sy, "Alt + Left / Right", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 1);
    draw_text(cx + 180, sy, "Snap window to Left / Right Half", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    sy += gh;

    draw_text(cx + 16, sy, "Alt + Up / Down", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 1);
    draw_text(cx + 180, sy, "Maximize window  /  Restore or Minimize", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    sy += gh;

    draw_text(cx + 16, sy, "Alt + Tab", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 1);
    draw_text(cx + 180, sy, "Cycle active surface focus & raise", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    sy += gh;

    draw_text(cx + 16, sy, "F11  or  Win + D", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 1);
    draw_text(cx + 180, sy, "Toggle Show Desktop (minimize / restore all)", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    sy += gh;

    draw_text(cx + 16, sy, "Shift+F10 / R-Click", GH_COLOR_TEXT_PRIMARY, FONT_TRANSPARENT, 1);
    draw_text(cx + 180, sy, "Open Desktop Context Menu", GH_COLOR_TEXT_MUTED, FONT_TRANSPARENT, 0);
    sy += gh;

    draw_text(cx + 16, sy, "Space / F1 / [GH]", GH_COLOR_GREEN_LEAF, FONT_TRANSPARENT, 1);
    draw_text(cx + 180, sy, "Radial App Launcher   |   ESC: Return to Console", GH_COLOR_AMBER_WARN, FONT_TRANSPARENT, 0);
}

int settings_surface_event(surface_t* s, const input_event_t* ev) {
    (void)s; (void)ev;
    return 0;
}