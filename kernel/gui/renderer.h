/* ==============================================================================
 * Greenhouse OS — Modern Light-Mode 2D Renderer (Centralized Foundation)
 * ==============================================================================
 * Provides high-performance, bounds-checked rendering primitives matching the
 * Greenhouse website (https://berrygreenhouse.freebuff.app) in LIGHT MODE:
 *   - 32-bit RGBA framebuffer operations
 *   - Fast alpha compositing
 *   - Linear / interpolated gradients (horizontal, vertical, arbitrary angle)
 *   - Radial gradients (ambient botanical bloom)
 *   - Antialiased rounded rectangles and circles
 *   - Multi-tier soft ambient drop shadows
 *   - Stack-based clipping
 *   - Cached surfaces and offscreen rendering targets
 *   - Double-buffered presentation
 *   - Centralized Greenhouse theme integration
 * ==============================================================================
 */

#ifndef GUI_RENDERER_H
#define GUI_RENDERER_H

#include <stdint.h>
#include <stddef.h>
#include "gh_theme.h"
#include "../graphics/graphics.h"
#include "../graphics/font.h"

/* Fast color pack helpers */
static inline uint32_t gui_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)((0xFF << 24) | (r << 16) | (g << 8) | b);
}

static inline uint32_t gui_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (uint32_t)((a << 24) | (r << 16) | (g << 8) | b);
}

/* Fast alpha blending between two 0xAARRGGBB colors */
uint32_t renderer_alpha_blend(uint32_t src, uint32_t dst, uint8_t alpha);
uint32_t renderer_blend_over(uint32_t src_with_alpha, uint32_t dst);

/* Primitive drawing functions */
void renderer_blend_pixel(int x, int y, uint32_t color, uint8_t alpha);
void renderer_draw_line(int x0, int y0, int x1, int y1, uint32_t color, uint8_t alpha);
void renderer_fill_alpha_rect(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void renderer_fill_alpha_rounded_rect(int x, int y, int w, int h, int radius, uint32_t color, uint8_t alpha);

/* Antialiased Primitives (subpixel coverage) */
void renderer_fill_aa_rounded_rect(int x, int y, int w, int h, int radius, uint32_t color, uint8_t alpha);
void renderer_draw_aa_rounded_rect(int x, int y, int w, int h, int radius, int thickness, uint32_t color, uint8_t alpha);
void renderer_fill_aa_circle(int cx, int cy, int radius, uint32_t color, uint8_t alpha);
void renderer_draw_aa_circle(int cx, int cy, int radius, int thickness, uint32_t color, uint8_t alpha);

/* Linear and Radial Gradients */
void renderer_fill_gradient_v(int x, int y, int w, int h, uint32_t top_color, uint32_t bottom_color, uint8_t alpha);
void renderer_fill_gradient_h(int x, int y, int w, int h, uint32_t left_color, uint32_t right_color, uint8_t alpha);
void renderer_fill_gradient_linear(int x, int y, int w, int h, uint32_t start_col, uint32_t end_col, int angle_deg, uint8_t alpha);
void renderer_fill_radial_gradient(int cx, int cy, int radius, uint32_t inner_color, uint32_t outer_color, uint8_t alpha);

/* Soft Ambient Drop Shadows */
void renderer_draw_drop_shadow(int x, int y, int w, int h, int radius, int shadow_size, uint8_t max_alpha);
void renderer_draw_soft_shadow(int x, int y, int w, int h, int radius, int blur, int offset_x, int offset_y, uint32_t shadow_color, uint8_t max_alpha);

/* Physical Frosted Glass / Card Panels */
void renderer_draw_glass_panel(int x, int y, int w, int h, int radius,
                               uint32_t body_color, uint8_t body_alpha,
                               uint32_t border_color, int shadow_depth);
void renderer_draw_card_panel(int x, int y, int w, int h, int radius,
                              uint32_t body_color, uint8_t body_alpha,
                              uint32_t border_color, int shadow_depth);
void renderer_draw_card(int x, int y, int w, int h, int radius,
                        const char* header, uint32_t header_color,
                        uint32_t bg_color, uint32_t border_color);

/* Desktop Background (smooth gradients and subtle ambient shapes) */
void renderer_draw_light_background(int w, int h);

/* Stack-based Clipping Rectangle Management */
void renderer_set_clip(int x, int y, int w, int h);
void renderer_push_clip(int x, int y, int w, int h);
void renderer_pop_clip(void);
void renderer_clear_clip(void);
int  renderer_get_clip(int* x, int* y, int* w, int* h);

/* Cached Surfaces (Offscreen 32-bit ARGB Buffers) */
typedef struct renderer_surface {
    int width;
    int height;
    int pitch;          /* row stride in bytes */
    uint32_t* pixels;   /* 32-bit ARGB pixel memory */
} renderer_surface_t;

renderer_surface_t* renderer_surface_create(int w, int h);
void renderer_surface_free(renderer_surface_t* s);
void renderer_surface_set_target(renderer_surface_t* s);
renderer_surface_t* renderer_surface_get_target(void);
void renderer_surface_clear(renderer_surface_t* s, uint32_t color);
void renderer_surface_blit(int dst_x, int dst_y, const renderer_surface_t* src, int src_x, int src_y, int w, int h, uint8_t alpha);

/* Modern UI Components */
void renderer_draw_button(int x, int y, int w, int h, const char* label,
                          uint32_t bg_color, uint32_t border_color, uint32_t text_color,
                          int is_hovered, int is_pressed, int is_primary);

void renderer_draw_pill_button(int x, int y, int w, int h, const char* label,
                               uint32_t bg_color, uint32_t border_color, uint32_t text_color, int is_active);

void renderer_draw_badge(int x, int y, const char* text,
                         uint32_t bg_color, uint32_t text_color, int radius);

void renderer_draw_input_field(int x, int y, int w, int h, const char* text,
                               uint32_t bg_color, uint32_t border_color, uint32_t text_color,
                               uint32_t placeholder_color, int is_focused, int has_text);

void renderer_draw_meter(int x, int y, int w, int h, int percent,
                         uint32_t fill_color, uint32_t bg_color, uint32_t border_color,
                         int radius);

void renderer_draw_separator(int x, int y, int w, uint32_t color, uint8_t alpha);

void renderer_draw_avatar(int x, int y, int size, const char* initials,
                          uint32_t bg_color, uint32_t text_color);

/* Button style presets */
#define RENDERER_BTN_PRIMARY     1
#define RENDERER_BTN_SECONDARY   2
#define RENDERER_BTN_GHOST       3
#define RENDERER_BTN_OUTLINE     4
#define RENDERER_BTN_BRAND       5
#define RENDERER_BTN_DESTRUCTIVE 6

void renderer_draw_button_styled(int x, int y, int w, int h, const char* label,
                                 int style, int is_hovered, int is_pressed);

#endif /* GUI_RENDERER_H */