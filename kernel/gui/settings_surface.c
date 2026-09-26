/* ==============================================================================
 * Greenhouse OS - VERDANT Settings / Display Inspector Surface (implementation)
 * ==============================================================================
 */

#include "settings_surface.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"
#include "../graphics/framebuffer.h"
#include "../version.h"

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
    graphics_fill_gradient_h(cx + 8, cy + 8, cw - 16, 26, 0xFF0D2820, 0xFF081410);
    graphics_draw_rounded_rect(cx + 8, cy + 8, cw - 16, 26, 4, 0xFF10B981);
    draw_text(cx + 14, cy + 13, "[*] DISPLAY PROPERTIES & VERDANT ENGINE", 0xFF6EE7B7, FONT_TRANSPARENT, 1);

    int y = cy + 42;
    int gh = font_glyph_height() + 4;

    /* Mode readout */
    char line[128];
    int n = 0;
    line[0] = '\0';
    draw_text(cx + 14, y, "Active Mode: ", 0xFF94A3B8, FONT_TRANSPARENT, 0);
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
    line[n++] = '\0';
    draw_text(cx + 120, y, line, 0xFF38BDF8, FONT_TRANSPARENT, 1);
    y += gh;

    /* Stride & Backend */
    n = 0;
    line[0] = '\0';
    draw_text(cx + 14, y, "Stride / BAR0: ", 0xFF94A3B8, FONT_TRANSPARENT, 0);
    if (fb) {
        set_put_uint(line, &n, sizeof(line), fb->pitch);
        line[n++] = ' ';
        line[n++] = 'b';
        line[n++] = 'y';
        line[n++] = 't';
        line[n++] = 'e';
        line[n++] = 's';
        line[n++] = ' ';
        line[n++] = '|';
        line[n++] = ' ';
        set_put_hex(line, &n, sizeof(line), fb->address);
    } else {
        line[0] = 'N'; line[1] = '/'; line[2] = 'A'; line[3] = '\0';
    }
    draw_text(cx + 120, y, line, 0xFFE2E8F0, FONT_TRANSPARENT, 0);
    y += gh;

    /* Back Buffer Memory */
    n = 0;
    line[0] = '\0';
    draw_text(cx + 14, y, "Compositor: ", 0xFF94A3B8, FONT_TRANSPARENT, 0);
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
        line[n++] = '\0';
    } else {
        line[0] = 'D'; line[1] = 'i'; line[2] = 'r'; line[3] = 'e'; line[4] = 'c'; line[5] = 't'; line[6] = '\0';
    }
    draw_text(cx + 120, y, line, 0xFF34D399, FONT_TRANSPARENT, 0);
    y += gh;

    /* Presents & FPS count */
    n = 0;
    line[0] = '\0';
    draw_text(cx + 14, y, "Presents: ", 0xFF94A3B8, FONT_TRANSPARENT, 0);
    set_put_uint(line, &n, sizeof(line), graphics_get_present_count());
    draw_text(cx + 120, y, line, 0xFFFBBF24, FONT_TRANSPARENT, 0);
    y += gh;

    /* Shortcuts Card */
    int card_y = cy + ch - 50;
    graphics_fill_rounded_rect(cx + 8, card_y, cw - 16, 42, 4, 0xFF0E1724);
    graphics_draw_rounded_rect(cx + 8, card_y, cw - 16, 42, 4, 0xFF1E3A5F);
    draw_text(cx + 14, card_y + 6, "HOTKEYS: Space/F1 = Launcher | Tab = Focus | F5 = Arrange", 0xFFCBD5E1, FONT_TRANSPARENT, 0);
    draw_text(cx + 14, card_y + 22, "Press ESC or click 'Text [X]' to exit to VGA shell.", 0xFFF43F5E, FONT_TRANSPARENT, 0);
}

int settings_surface_event(surface_t* s, const input_event_t* ev) {
    (void)s; (void)ev;
    return 0;
}
