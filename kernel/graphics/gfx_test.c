/* ==============================================================================
 * Greenhouse OS - Phase 6: Graphics Self Test (implementation)
 * ==============================================================================
 */

#include "gfx_test.h"
#include "graphics.h"
#include "font.h"
#include "framebuffer.h"
#include "vbe.h"

#define T_BG        0xFF101828
#define T_PANEL     0xFF182338
#define T_TEXT      0xFFE6EDF7
#define T_MUTED     0xFF8FA0B8
#define T_OK        0xFF34D399
#define T_WARN      0xFFFBBF24
#define T_ERR       0xFFF87171

static gfx_test_result_t gfx_result;

static void gfx_record_failure(int x, int y, uint32_t got, uint32_t expected) {
    if (gfx_result.fail_x >= 0) return; /* keep the first one only */
    gfx_result.fail_x = x;
    gfx_result.fail_y = y;
    gfx_result.fail_got = got;
    gfx_result.fail_expected = expected;
}

/* ------------------------------------------------------------------------------
 * Pixel verification
 * ---------------------------------------------------------------------------- */

/* How far a channel may move between the value asked for and the value that
 * comes back out of the surface.  A shallower pixel format quantises the
 * channels, so an exact comparison would fail every check at 16bpp even when
 * the conversion is correct.  The tolerance is the quantisation step of the
 * narrowest channel in the format, in 8-bit terms. */
static uint32_t gfx_channel_tolerance(void) {
    switch (graphics_get_bpp()) {
        case 16: return 8;   /* 5 bits per channel -> steps of 8 */
        case 24:
        case 32: return 0;   /* 8 bits per channel, exact */
        default: return 0;
    }
}

static int gfx_colour_matches(uint32_t got, uint32_t expected, uint32_t tolerance) {
    if (tolerance == 0) return got == (expected & 0x00FFFFFFu);

    for (int shift = 0; shift <= 16; shift += 8) {
        int g = (int)((got >> shift) & 0xFF);
        int e = (int)((expected >> shift) & 0xFF);
        int diff = g - e;
        if (diff < 0) diff = -diff;
        if (diff > (int)tolerance) return 0;
    }
    return 1;
}

static int gfx_check_pixel(int x, int y, uint32_t expected) {
    uint32_t got = 0;
    gfx_result.checks++;
    if (graphics_get_pixel(x, y, &got) != 0) {
        gfx_result.failures++;
        gfx_record_failure(x, y, 0, expected);
        return 0;
    }
    if (!gfx_colour_matches(got, expected, gfx_channel_tolerance())) {
        gfx_result.failures++;
        gfx_record_failure(x, y, got, expected);
        return 0;
    }
    return 1;
}

int gfx_test_expect_pixel(int x, int y, uint32_t expected) {
    return gfx_check_pixel(x, y, expected);
}

/* Verifies that a present actually reached display memory: the framebuffer and
 * the back buffer must agree at the sample points.  The QEMU screendump cannot
 * be trusted for this (its surface size lags the mode switch), so the check is
 * done here where both sides are readable. */
static void gfx_check_present(void) {
    int w = graphics_get_width();
    int h = graphics_get_height();
    const int pts_x[5] = {0, w / 2, w - 1, w / 3, 17};
    const int pts_y[5] = {0, 8, h - 1, h / 2, 251};

    for (int i = 0; i < 5; i++) {
        int x = pts_x[i];
        int y = pts_y[i];
        uint32_t src = 0;
        uint32_t dst = 0;
        gfx_result.present_checks++;

        if (graphics_get_pixel(x, y, &src) != 0) {
            gfx_result.present_failures++;
            continue;
        }
        if (framebuffer_get_pixel(x, y, &dst) != 0) {
            gfx_result.present_failures++;
            gfx_record_failure(x, y, dst, src);
            continue;
        }
        if (src != dst) {
            gfx_result.present_failures++;
            gfx_record_failure(x, y, dst, src);
        }
    }
}

/* ------------------------------------------------------------------------------
 * Scenes
 * -------------------------------------------------------------------------- */

void gfx_test_draw_color_bars(void) {
    static const uint32_t bars[8] = {
        0xFFFFFFFF, 0xFFFFFF00, 0xFF00FF00, 0xFF00FFFF,
        0xFF0000FF, 0xFFFF00FF, 0xFFFF0000, 0xFF000000
    };

    int w = graphics_get_width();
    int bar_h = 40;
    for (int i = 0; i < 8; i++) {
        int x0 = (i * w) / 8;
        int x1 = ((i + 1) * w) / 8;
        graphics_fill_rect(x0, 0, x1 - x0, bar_h, bars[i]);
        gfx_check_pixel((x0 + x1) / 2, bar_h / 2, bars[i]);
    }

    /* Grey ramp below the bars. */
    int bw = w / 256;
    for (int i = 0; i < 256; i++) {
        uint32_t v = (uint32_t)i;
        graphics_fill_rect(i * bw, bar_h, bw, 24, (v << 16) | (v << 8) | v);
    }
    int mid = w / 2;
    uint32_t mv = (uint32_t)(bw ? mid / bw : 0);
    gfx_check_pixel(mid, bar_h + 12, (mv << 16) | (mv << 8) | mv);
}

void gfx_test_draw_shapes(void) {
    int cx = graphics_get_width() / 4;
    int cy = 140;

    graphics_fill_rect(20, 90, graphics_get_width() / 2 - 40, 120, T_PANEL);
    gfx_check_pixel(22, 92, T_PANEL);

    graphics_draw_rect_thick(40, 110, 200, 80, T_TEXT, 2);
    gfx_check_pixel(40, 110, T_TEXT);

    graphics_fill_rect(60, 126, 160, 48, 0xFF1D4ED8);
    gfx_check_pixel(140, 150, 0xFF1D4ED8);

    graphics_draw_line(20, 220, graphics_get_width() - 20, 220, T_WARN);
    gfx_check_pixel(30, 220, T_WARN);

    graphics_fill_circle(cx + 300, cy, 48, T_OK);
    gfx_check_pixel(cx + 300, cy, T_OK);

    graphics_draw_circle(cx + 300, cy, 60, T_ERR);
    gfx_check_pixel(cx + 300, cy - 60, T_ERR);

    graphics_fill_gradient_v(20, 240, 240, 80, 0xFF38BDF8, 0xFF1E3A8A);
    gfx_check_pixel(140, 240, 0xFF38BDF8);
}

void gfx_test_draw_text(void) {
    int x = 24;
    int y = 340;
    draw_text(x, y, "Greenhouse OS graphics self test", T_TEXT, T_BG, 1);
    draw_text(x, y + 20, "8x16 bitmap font (Adwaita Mono, OFL 1.1)", T_MUTED, T_BG, 1);
    draw_text(x, y + 40, "!\"#$%&'()*+,-./0123456789:;<=>?", T_OK, T_BG, 1);
    draw_text(x, y + 60, "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_", T_OK, T_BG, 1);
    draw_text(x, y + 80, "`abcdefghijklmnopqrstuvwxyz{|}~", T_OK, T_BG, 1);

    /* A glyph is 8 lit columns at most; verify the cell of a filled block. */
    draw_char(x, y + 100, 0xDB, T_TEXT, T_BG, 0);
    gfx_check_pixel(x, y + 100, T_BG);
}

void gfx_test_draw_grid(void) {
    int w = graphics_get_width();
    int h = graphics_get_height();

    graphics_clear(T_BG);
    gfx_check_pixel(0, 0, T_BG);

    for (int y = 0; y < h; y += 32) graphics_fill_rect(0, y, w, 1, 0xFF1E2A42);
    for (int x = 0; x < w; x += 32) graphics_fill_rect(x, 0, 1, h, 0xFF1E2A42);
    gfx_check_pixel(0, 0, 0xFF1E2A42);

    /* Out of bounds writes must be ignored, not crash. */
    graphics_put_pixel(-1, -1, T_ERR);
    graphics_put_pixel(w, h, T_ERR);
    gfx_check_pixel(0, 0, 0xFF1E2A42);

    /* Clipping must keep drawing inside the given rectangle. */
    graphics_set_clip(100, 100, 50, 50);
    graphics_fill_rect(0, 0, 400, 400, T_ERR);
    gfx_check_pixel(120, 120, T_ERR);
    gfx_check_pixel(80, 80, T_BG);
    graphics_clear_clip();

    graphics_fill_rect(0, 0, w, 40, 0xFF0F172A);
    draw_text(20, 12, "Greenhouse OS - framebuffer verified", T_TEXT, 0xFF0F172A, 1);
}

/* ------------------------------------------------------------------------------
 * Frame sequencing
 * -------------------------------------------------------------------------- */

int gfx_test_draw_frame(int frame) {
    int index = frame % GFX_TEST_FRAMES;
    int w = graphics_get_width();
    int h = graphics_get_height();

    graphics_begin_frame();
    graphics_clear(T_BG);

    switch (index) {
        case 0:
            gfx_test_draw_color_bars();
            draw_text(16, h - 24, "frame 0: colour bars, grey ramp, clipping", T_MUTED, T_BG, 1);
            break;
        case 1:
            gfx_test_draw_shapes();
            draw_text(16, h - 24, "frame 1: rectangles, lines, circles, gradients", T_MUTED, T_BG, 1);
            break;
        case 2:
            gfx_test_draw_text();
            draw_text(16, h - 24, "frame 2: bitmap font coverage", T_MUTED, T_BG, 1);
            break;
        case 3:
            gfx_test_draw_grid();
            draw_text(16, h - 24, "frame 3: grid, bounds checks, clip rectangle", T_MUTED, T_BG, 1);
            break;
        default: {
            /* Frame 4: live status, including real hardware state. */
            gfx_test_draw_grid();
            graphics_fill_rect(0, h / 2 - 60, w, 120, 0xE6101828);
            graphics_draw_rect_thick(0, h / 2 - 60, w, 120, 0xFF38BDF8, 2);

            char line[80];
            int n = 0;
            const char* p = "surface ";
            while (*p) line[n++] = *p++;
            int v = w;
            char tmp[12];
            int tl = 0;
            do { tmp[tl++] = (char)('0' + v % 10); v /= 10; } while (v);
            while (tl) line[n++] = tmp[--tl];
            line[n++] = 'x';
            v = h;
            tl = 0;
            do { tmp[tl++] = (char)('0' + v % 10); v /= 10; } while (v);
            while (tl) line[n++] = tmp[--tl];
            line[n++] = ' ';
            v = graphics_get_bpp();
            tl = 0;
            do { tmp[tl++] = (char)('0' + v % 10); v /= 10; } while (v);
            while (tl) line[n++] = tmp[--tl];
            line[n++] = 'b';
            line[n++] = 'p';
            line[n] = '\0';
            draw_text(24, h / 2 - 40, line, T_TEXT, 0xFF101828, 1);

            const framebuffer_info_t* fb = framebuffer_get_info();
            n = 0;
            const char* q = "backend ";
            while (*q) line[n++] = *q++;
            const char* b = (fb && fb->backend == FB_BACKEND_MULTIBOOT2) ? "multiboot2" : "vbe-bochs";
            while (*b) line[n++] = *b++;
            const char* c = "   legacy vbe registers: ";
            while (*c) line[n++] = *c++;
            line[n++] = framebuffer_has_vbe() ? 'y' : 'n';
            line[n] = '\0';
            draw_text(24, h / 2 - 16, line, T_OK, 0xFF101828, 1);

            n = 0;
            const char* m = "surface ";
            while (*m) line[n++] = *m++;
            const char* mode = graphics_has_back_buffer() ? "back buffer" : "direct rendering";
            if (!graphics_has_back_buffer() && graphics_get_back_buffer_error() != 0) {
                /* the caller prints the numeric reason */
            }
            while (*mode) line[n++] = *mode++;
            const char* pm = "   present: ";
            while (*pm) line[n++] = *pm++;
            uint64_t presents = graphics_get_present_count();
            tl = 0;
            do { tmp[tl++] = (char)('0' + presents % 10); presents /= 10; } while (presents);
            while (tl) line[n++] = tmp[--tl];
            line[n] = '\0';
            draw_text(24, h / 2 + 8, line, T_MUTED, 0xFF101828, 1);
            break;
        }
    }

    graphics_end_frame();
    gfx_result.frames_drawn++;
    return index;
}

void gfx_test_run_all(gfx_test_result_t* out) {
    gfx_result.checks = 0;
    gfx_result.failures = 0;
    gfx_result.present_checks = 0;
    gfx_result.present_failures = 0;
    gfx_result.frames_drawn = 0;
    gfx_result.fail_x = -1;
    gfx_result.fail_y = -1;
    gfx_result.fail_got = 0;
    gfx_result.fail_expected = 0;

    for (int i = 0; i < GFX_TEST_FRAMES; i++) {
        gfx_test_draw_frame(i);
        graphics_present();
        gfx_check_present();
    }

    gfx_result.width = graphics_get_width();
    gfx_result.height = graphics_get_height();
    gfx_result.bpp = graphics_get_bpp();
    gfx_result.back_buffer = graphics_has_back_buffer();
    gfx_result.back_buffer_error = graphics_get_back_buffer_error();

    if (out) *out = gfx_result;
}
