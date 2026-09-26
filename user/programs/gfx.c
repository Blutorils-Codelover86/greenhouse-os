#include "gui.h"
#include "stdio.h"
#include "unistd.h"

/* Draws a frame, follows the pointer, and hands the text console back exactly
 * as it found it when ESC is pressed. */

#define PANEL_BG   GFX_RGB(0x18, 0x1C, 0x28)
#define PANEL_EDGE GFX_RGB(0x39, 0x44, 0x5C)
#define TEXT_FG    GFX_RGB(0xE6, 0xEA, 0xF2)
#define TEXT_DIM   GFX_RGB(0x8A, 0x93, 0xA8)
#define ACCENT     GFX_RGB(0x5A, 0xC8, 0xFA)

static char* put_str(char* p, char* end, const char* s) {
    while (*s && p < end - 1) *p++ = *s++;
    return p;
}

static char* put_int(char* p, char* end, int value) {
    char digits[12];
    int n = 0;
    if (value < 0) {
        *p++ = '-';
        value = -value;
    }
    do {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value && n < (int)sizeof(digits));
    while (n > 0 && p < end - 1) *p++ = digits[--n];
    return p;
}

int main(int argc, char** argv) {
    gfx_surface_t surface;
    user_input_event_t ev;
    char line[128];
    int frames = 0, events = 0, px = 0, py = 0, running = 1;

    (void)argc;
    (void)argv;

    if (gfx_enter() != 0) {
        printf("GFX: the display refused the graphics mode\n");
        return 1;
    }
    if (gfx_get_info(&surface) != 0 || surface.width == 0) {
        printf("GFX: no surface information\n");
        gfx_leave();
        return 1;
    }
    printf("GFX: surface %dx%d @ %d bpp\n", surface.width, surface.height, surface.bpp);

    while (gfx_poll_event(&ev) > 0) { /* drain the pre-existing queue */ }

    while (running) {
        char* end = line + sizeof(line) - 1;
        char* p;

        for (int i = 0; i < 4 && running; i++) {
            user_input_event_t local;
            while (gfx_poll_event(&local) > 0) {
                events++;
                if (local.type == GFX_EVENT_MOUSE_MOVE) {
                    px = local.x;
                    py = local.y;
                } else if (local.type == GFX_EVENT_KEY_DOWN && local.ascii == 27) {
                    running = 0; /* ESC */
                }
            }
            sleep(16);
        }

        gfx_clear(PANEL_BG);
        gfx_rect(16, 48, (int)surface.width - 32, 96, PANEL_EDGE);
        gfx_rect(16, 48, (int)surface.width - 32, 24, ACCENT);
        gfx_text(24, 52, "Greenhouse OS - userland graphics", TEXT_FG, ACCENT);

        p = put_str(line, end, "surface ");
        p = put_int(p, end, (int)surface.width);
        p = put_str(p, end, "x");
        p = put_int(p, end, (int)surface.height);
        p = put_str(p, end, " @ ");
        p = put_int(p, end, (int)surface.bpp);
        p = put_str(p, end, " bpp");
        *p = '\0';
        gfx_text(24, 92, line, TEXT_FG, PANEL_BG);

        p = put_str(line, end, "frames ");
        p = put_int(p, end, frames);
        p = put_str(p, end, "   pointer ");
        p = put_int(p, end, px);
        p = put_str(p, end, ",");
        p = put_int(p, end, py);
        p = put_str(p, end, "   events ");
        p = put_int(p, end, events);
        *p = '\0';
        gfx_text(24, 112, line, TEXT_DIM, PANEL_BG);

        if (px >= 0 && py >= 0 && (uint32_t)px < surface.width && (uint32_t)py < surface.height) {
            gfx_line(px - 8, py, px + 8, py, ACCENT);
            gfx_line(px, py - 8, px, py + 8, ACCENT);
        }
        gfx_text(16, (int)surface.height - 24, "press ESC to return to the text console",
                 TEXT_DIM, PANEL_BG);

        gfx_present();
        if (++frames > 600) running = 0; /* never spin forever */
    }

    gfx_leave();
    printf("GFX: final pointer %d,%d after %d event(s)\n", px, py, events);
    printf("GFX: userland demo finished after %d frame(s), %d event(s)\n", frames, events);
    return 0;
}
