/* ==============================================================================
 * Greenhouse OS — Modern Dedicated Cursor System (Implementation)
 * ==============================================================================
 */

#include "cursor.h"
#include "renderer.h"
#include "../graphics/graphics.h"

/* ------------------------------------------------------------------------------
 * Palette definitions for cursor rendering
 * -------------------------------------------------------------------------- */

#define C_0 0x00000000   /* Transparent */
#define C_S 0x22122019   /* Soft outer ambient shadow (alpha ~34) */
#define C_M 0x48122019   /* Medium drop shadow (alpha ~72) */
#define C_D 0x88122019   /* Antialiased outer outline / contact shadow */
#define C_K 0xFF122019   /* Solid crisp dark charcoal outline */
#define C_W 0xFFFFFFFF   /* Solid crisp white body */
#define C_G 0xFF2D8A68   /* Botanical leaf green accent */
#define C_B 0xFF5EE9B5   /* Bright sprout green accent */
#define C_A 0x802D8A68   /* Translucent leaf green */

/* ------------------------------------------------------------------------------
 * Shape 0: CURSOR_DEFAULT (Crisp modern arrow, 16x20, hotspot 1, 1)
 * -------------------------------------------------------------------------- */
static const uint32_t cursor_default_pix[20 * 16] = {
    /* 0 */ C_0, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 1 */ C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 2 */ C_K, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 3 */ C_K, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 4 */ C_K, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 5 */ C_K, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 6 */ C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 7 */ C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 8 */ C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 9 */ C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0,
    /* 10*/ C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0,
    /* 11*/ C_K, C_W, C_W, C_W, C_W, C_W, C_K, C_K, C_K, C_K, C_K, C_K, C_0, C_0, C_0, C_0,
    /* 12*/ C_K, C_W, C_W, C_W, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 13*/ C_K, C_W, C_W, C_K, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 14*/ C_K, C_W, C_K, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 15*/ C_K, C_K, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 16*/ C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 17*/ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 18*/ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 19*/ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_K, C_0, C_0, C_0, C_0, C_0, C_0
};

/* ------------------------------------------------------------------------------
 * Shape 1: CURSOR_POINTER (Modern pointing hand, 18x20, hotspot 5, 1)
 * -------------------------------------------------------------------------- */
static const uint32_t cursor_pointer_pix[20 * 18] = {
    /* 0 */ C_0, C_0, C_0, C_0, C_K, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 1 */ C_0, C_0, C_0, C_K, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 2 */ C_0, C_0, C_0, C_K, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 3 */ C_0, C_0, C_0, C_K, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 4 */ C_0, C_0, C_0, C_K, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 5 */ C_0, C_0, C_0, C_K, C_W, C_W, C_K, C_K, C_K, C_0, C_K, C_K, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 6 */ C_0, C_K, C_K, C_K, C_W, C_W, C_K, C_W, C_W, C_K, C_W, C_W, C_K, C_K, C_0, C_0, C_0, C_0,
    /* 7 */ C_K, C_W, C_W, C_K, C_W, C_W, C_K, C_W, C_W, C_K, C_W, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 8 */ C_K, C_W, C_W, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_W, C_W, C_K, C_0, C_0,
    /* 9 */ C_0, C_K, C_W, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0,
    /* 10*/ C_0, C_0, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0,
    /* 11*/ C_0, C_0, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0,
    /* 12*/ C_0, C_0, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0,
    /* 13*/ C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0,
    /* 14*/ C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0,
    /* 15*/ C_0, C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0,
    /* 16*/ C_0, C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 17*/ C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 18*/ C_0, C_0, C_0, C_0, C_0, C_K, C_K, C_K, C_K, C_K, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 19*/ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0
};

/* ------------------------------------------------------------------------------
 * Shape 2: CURSOR_TEXT (Sleek I-beam, 11x17, hotspot 5, 8)
 * -------------------------------------------------------------------------- */
static const uint32_t cursor_text_pix[17 * 11] = {
    /* 0 */ C_0, C_K, C_K, C_K, C_0, C_0, C_0, C_K, C_K, C_K, C_0,
    /* 1 */ C_K, C_W, C_W, C_W, C_K, C_K, C_K, C_W, C_W, C_W, C_K,
    /* 2 */ C_0, C_K, C_K, C_K, C_W, C_K, C_W, C_K, C_K, C_K, C_0,
    /* 3 */ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 4 */ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 5 */ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 6 */ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 7 */ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 8 */ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 9 */ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 10*/ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 11*/ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 12*/ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 13*/ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 14*/ C_0, C_K, C_K, C_K, C_W, C_K, C_W, C_K, C_K, C_K, C_0,
    /* 15*/ C_K, C_W, C_W, C_W, C_K, C_K, C_K, C_W, C_W, C_W, C_K,
    /* 16*/ C_0, C_K, C_K, C_K, C_0, C_0, C_0, C_K, C_K, C_K, C_0
};

/* ------------------------------------------------------------------------------
 * Shape 3: CURSOR_RESIZE_H (Horizontal bidirectional arrow, 19x11, hotspot 9, 5)
 * -------------------------------------------------------------------------- */
static const uint32_t cursor_resize_h_pix[11 * 19] = {
    /* 0 */ C_0, C_0, C_0, C_0, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_0, C_0, C_0, C_0,
    /* 1 */ C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 2 */ C_0, C_0, C_K, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_W, C_K, C_0, C_0,
    /* 3 */ C_0, C_K, C_W, C_W, C_W, C_K, C_K, C_K, C_K, C_K, C_K, C_K, C_K, C_K, C_W, C_W, C_W, C_K, C_0,
    /* 4 */ C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K,
    /* 5 */ C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K,
    /* 6 */ C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K,
    /* 7 */ C_0, C_K, C_W, C_W, C_W, C_K, C_K, C_K, C_K, C_K, C_K, C_K, C_K, C_K, C_W, C_W, C_W, C_K, C_0,
    /* 8 */ C_0, C_0, C_K, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_W, C_K, C_0, C_0,
    /* 9 */ C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 10*/ C_0, C_0, C_0, C_0, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_0, C_0, C_0, C_0
};

/* ------------------------------------------------------------------------------
 * Shape 4: CURSOR_RESIZE_V (Vertical bidirectional arrow, 11x19, hotspot 5, 9)
 * -------------------------------------------------------------------------- */
static const uint32_t cursor_resize_v_pix[19 * 11] = {
    /* 0 */ C_0, C_0, C_0, C_0, C_0, C_K, C_0, C_0, C_0, C_0, C_0,
    /* 1 */ C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0,
    /* 2 */ C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_K, C_0, C_0, C_0,
    /* 3 */ C_0, C_0, C_K, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0,
    /* 4 */ C_0, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0,
    /* 5 */ C_K, C_K, C_K, C_K, C_W, C_W, C_W, C_K, C_K, C_K, C_K,
    /* 6 */ C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_K, C_0, C_0, C_0,
    /* 7 */ C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_K, C_0, C_0, C_0,
    /* 8 */ C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_K, C_0, C_0, C_0,
    /* 9 */ C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_K, C_0, C_0, C_0,
    /* 10*/ C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_K, C_0, C_0, C_0,
    /* 11*/ C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_K, C_0, C_0, C_0,
    /* 12*/ C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_K, C_0, C_0, C_0,
    /* 13*/ C_K, C_K, C_K, C_K, C_W, C_W, C_W, C_K, C_K, C_K, C_K,
    /* 14*/ C_0, C_K, C_W, C_W, C_W, C_W, C_W, C_W, C_W, C_K, C_0,
    /* 15*/ C_0, C_0, C_K, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0,
    /* 16*/ C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_K, C_0, C_0, C_0,
    /* 17*/ C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0,
    /* 18*/ C_0, C_0, C_0, C_0, C_0, C_K, C_0, C_0, C_0, C_0, C_0
};

/* ------------------------------------------------------------------------------
 * Shape 5: CURSOR_RESIZE_DIAG (Diagonal resize arrow, 17x17, hotspot 8, 8)
 * -------------------------------------------------------------------------- */
static const uint32_t cursor_resize_diag_pix[17 * 17] = {
    /* 0 */ C_K, C_K, C_K, C_K, C_K, C_K, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 1 */ C_K, C_W, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 2 */ C_K, C_W, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 3 */ C_K, C_W, C_W, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 4 */ C_K, C_W, C_W, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 5 */ C_K, C_W, C_K, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 6 */ C_K, C_K, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 7 */ C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 8 */ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 9 */ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_0, C_0, C_0,
    /* 10*/ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_0, C_0, C_K, C_K,
    /* 11*/ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_0, C_K, C_W, C_K,
    /* 12*/ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_K, C_W, C_W, C_K,
    /* 13*/ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_K,
    /* 14*/ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_K,
    /* 15*/ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_W, C_W, C_W, C_W, C_K,
    /* 16*/ C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_0, C_K, C_K, C_K, C_K, C_K, C_K, C_K
};

/* ------------------------------------------------------------------------------
 * Shape 6: CURSOR_BUSY (Botanical animated spinner ring, 17x17, hotspot 8, 8)
 * -------------------------------------------------------------------------- */
static uint32_t cursor_busy_buf[17 * 17];
static int g_busy_tick = 0;

static const struct {
    int dx, dy;
} g_spinner_segments[8] = {
    {  0, -6 }, /* 0: Top */
    {  4, -4 }, /* 1: Top-Right */
    {  6,  0 }, /* 2: Right */
    {  4,  4 }, /* 3: Bottom-Right */
    {  0,  6 }, /* 4: Bottom */
    { -4,  4 }, /* 5: Bottom-Left */
    { -6,  0 }, /* 6: Left */
    { -4, -4 }  /* 7: Top-Left */
};

static void cursor_render_busy_frame(void) {
    for (int i = 0; i < 17 * 17; i++) cursor_busy_buf[i] = C_0;

    int cx = 8, cy = 8;
    int phase = g_busy_tick % 8;

    for (int s = 0; s < 8; s++) {
        int order = (s - phase + 8) % 8;
        uint32_t seg_color;
        if (order == 0) seg_color = C_B;         /* Bright energetic sprout green */
        else if (order == 1) seg_color = C_G;    /* Vibrant leaf green */
        else if (order == 2) seg_color = C_A;    /* Translucent leaf green */
        else if (order <= 4) seg_color = C_D;    /* Muted charcoal */
        else continue;                           /* Faded tail */

        int px = cx + g_spinner_segments[s].dx;
        int py = cy + g_spinner_segments[s].dy;

        /* Draw 2x2 dot */
        for (int oy = 0; oy <= 1; oy++) {
            for (int ox = 0; ox <= 1; ox++) {
                int ix = px + ox;
                int iy = py + oy;
                if (ix >= 0 && ix < 17 && iy >= 0 && iy < 17) {
                    cursor_busy_buf[iy * 17 + ix] = seg_color;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------------------
 * Cursor Descriptor Table
 * -------------------------------------------------------------------------- */

typedef struct {
    int width;
    int height;
    int hot_x;
    int hot_y;
    const uint32_t* pixels;
    const char* name;
} cursor_desc_t;

static cursor_desc_t g_cursors[CURSOR_TYPE_COUNT] = {
    { 16, 20, 1, 1, cursor_default_pix,      "default" },
    { 18, 20, 5, 1, cursor_pointer_pix,      "pointer" },
    { 11, 17, 5, 8, cursor_text_pix,         "text" },
    { 19, 11, 9, 5, cursor_resize_h_pix,     "resize-horizontal" },
    { 11, 19, 5, 9, cursor_resize_v_pix,     "resize-vertical" },
    { 17, 17, 8, 8, cursor_resize_diag_pix,  "resize-diagonal" },
    { 17, 17, 8, 8, cursor_busy_buf,          "busy" }
};

static int g_current_shape = CURSOR_DEFAULT;
static int g_cursor_visible = 1;
static int g_cursor_scale = 1;
static int g_custom_hot_x = -1;
static int g_custom_hot_y = -1;

void cursor_init(void) {
    g_current_shape = CURSOR_DEFAULT;
    g_cursor_visible = 1;
    g_cursor_scale = 1;
    g_custom_hot_x = -1;
    g_custom_hot_y = -1;
    g_busy_tick = 0;
    cursor_render_busy_frame();
}

void cursor_set_shape(int shape) {
    if (shape < 0 || shape >= CURSOR_TYPE_COUNT) return;
    g_current_shape = shape;
    g_custom_hot_x = -1;
    g_custom_hot_y = -1;
}

int cursor_get_shape(void) {
    return g_current_shape;
}

void cursor_set_visible(int visible) {
    g_cursor_visible = visible ? 1 : 0;
}

int cursor_is_visible(void) {
    return g_cursor_visible;
}

void cursor_set_scale(int scale) {
    if (scale < 1) scale = 1;
    if (scale > 2) scale = 2;
    g_cursor_scale = scale;
}

int cursor_get_scale(void) {
    return g_cursor_scale;
}

void cursor_set_hotspot(int x, int y) {
    g_custom_hot_x = x;
    g_custom_hot_y = y;
}

void cursor_get_hotspot(int shape, int* hx, int* hy) {
    if (shape < 0 || shape >= CURSOR_TYPE_COUNT) shape = CURSOR_DEFAULT;
    if (hx) *hx = (g_custom_hot_x >= 0) ? g_custom_hot_x : g_cursors[shape].hot_x;
    if (hy) *hy = (g_custom_hot_y >= 0) ? g_custom_hot_y : g_cursors[shape].hot_y;
}

void cursor_tick(void) {
    g_busy_tick++;
    if (g_current_shape == CURSOR_BUSY) {
        cursor_render_busy_frame();
    }
}

int cursor_get_width(void) {
    return g_cursors[g_current_shape].width * g_cursor_scale;
}

int cursor_get_height(void) {
    return g_cursors[g_current_shape].height * g_cursor_scale;
}

const char* cursor_shape_name(int shape) {
    if (shape < 0 || shape >= CURSOR_TYPE_COUNT) return "unknown";
    return g_cursors[shape].name;
}

void cursor_draw(int x, int y) {
    if (!g_cursor_visible || !graphics_is_active()) return;

    cursor_desc_t* desc = &g_cursors[g_current_shape];
    if (g_current_shape == CURSOR_BUSY) {
        cursor_render_busy_frame();
    }

    int hx = (g_custom_hot_x >= 0) ? g_custom_hot_x : desc->hot_x;
    int hy = (g_custom_hot_y >= 0) ? g_custom_hot_y : desc->hot_y;

    int base_x = x - hx * g_cursor_scale;
    int base_y = y - hy * g_cursor_scale;

    int cw = desc->width;
    int ch = desc->height;
    const uint32_t* pix = desc->pixels;

    if (g_cursor_scale == 1) {
        for (int cy = 0; cy < ch; cy++) {
            int py = base_y + cy;
            for (int cx = 0; cx < cw; cx++) {
                uint32_t c = pix[cy * cw + cx];
                uint8_t a = (uint8_t)((c >> 24) & 0xFF);
                if (a == 0) continue;
                renderer_blend_pixel(base_x + cx, py, c, a);
            }
        }
    } else {
        /* High-DPI 2x scale */
        for (int cy = 0; cy < ch; cy++) {
            int py = base_y + cy * 2;
            for (int cx = 0; cx < cw; cx++) {
                uint32_t c = pix[cy * cw + cx];
                uint8_t a = (uint8_t)((c >> 24) & 0xFF);
                if (a == 0) continue;
                int px = base_x + cx * 2;
                renderer_blend_pixel(px,     py,     c, a);
                renderer_blend_pixel(px + 1, py,     c, a);
                renderer_blend_pixel(px,     py + 1, c, a);
                renderer_blend_pixel(px + 1, py + 1, c, a);
            }
        }
    }
}
