/* ==============================================================================
 * Greenhouse OS - Phase 6: Graphics Self Test
 *
 * Draws a sequence of test frames and verifies the drawing path by reading the
 * pixels back (graphics_get_pixel).  A frame passes when every checked pixel
 * matches the colour that was requested, which exercises clipping, colour
 * conversion, the back buffer and the present path in one go.
 * ==============================================================================
 */

#ifndef GFX_TEST_H
#define GFX_TEST_H

#include <stdint.h>

#define GFX_TEST_FRAMES 5

typedef struct {
    uint32_t checks;            /* pixels verified in the back buffer        */
    uint32_t failures;
    uint32_t present_checks;    /* back buffer vs framebuffer after present  */
    uint32_t present_failures;
    int      frames_drawn;
    int      width;
    int      height;
    int      bpp;
    int      back_buffer;
    int      back_buffer_error;
    /* Details of the first failed pixel check (fail_x < 0 when none failed). */
    int      fail_x;
    int      fail_y;
    uint32_t fail_got;
    uint32_t fail_expected;
} gfx_test_result_t;

/* Draws one test frame (wraps around every GFX_TEST_FRAMES) and accumulates the
 * pixel verification results.  Returns the frame number that was drawn. */
int  gfx_test_draw_frame(int frame);

/* Runs all frames with a present in between. */
void gfx_test_run_all(gfx_test_result_t* out);

/* Individual scenes, exposed so the shell and userland can reuse them. */
void gfx_test_draw_color_bars(void);
void gfx_test_draw_shapes(void);
void gfx_test_draw_text(void);
void gfx_test_draw_grid(void);

int  gfx_test_expect_pixel(int x, int y, uint32_t expected);

#endif /* GFX_TEST_H */
