/* ==============================================================================
 * Greenhouse OS — Modern Liquid-Glass 2D Renderer
 * ==============================================================================
 * Provides high-performance, bounds-checked rendering primitives for the modern
 * Greenhouse desktop:
 *   - Translucent glass surfaces with alpha compositing
 *   - Soft ambient drop shadows
 *   - Rounded panels, cards, pills, and badges
 *   - Progress gauges and telemetry meters
 *   - Specular top highlights and refined borders
 * ==============================================================================
 */

#ifndef GUI_RENDERER_H
#define GUI_RENDERER_H

#include <stdint.h>
#include <stddef.h>
#include "../graphics/graphics.h"
#include "../graphics/font.h"

/* Greenhouse Palette Constants (Calm, Modern, Organic Liquid-Glass) */
#define GFX_COLOR_WALLPAPER_TOP    0xFF0A131C  /* Deep slate-teal top   */
#define GFX_COLOR_WALLPAPER_BOT    0xFF112226  /* Ambient dark forest   */
#define GFX_COLOR_GLASS_SURFACE    0xFF14242E  /* Dark slate glass body */
#define GFX_COLOR_GLASS_SURFACE_ACT 0xFF192F3B /* Active focused glass  */
#define GFX_COLOR_GLASS_BORDER     0xFF2D4856  /* Subtle slate-cyan rim */
#define GFX_COLOR_GLASS_BORDER_ACT 0xFF3D6B5D  /* Active emerald rim    */
#define GFX_COLOR_GLASS_HIGHLIGHT  0x35FFFFFF  /* Top specular glint    */
#define GFX_COLOR_SHADOW           0x30000000  /* Soft ambient shadow   */

#define GFX_COLOR_EMERALD_PRIMARY  0xFF10B981  /* Greenhouse Emerald    */
#define GFX_COLOR_MINT_ACCENT      0xFF34D399  /* Mint highlight        */
#define GFX_COLOR_BERRY_ACCENT     0xFFF43F5E  /* Berry Assistant pink  */
#define GFX_COLOR_AMBER_WARN       0xFFF59E0B  /* Warm Amber status     */
#define GFX_COLOR_CYAN_ACCENT      0xFF06B6D4  /* Technical cyan        */

#define GFX_COLOR_TEXT_PRIMARY     0xFFFFFFFF  /* Crisp white           */
#define GFX_COLOR_TEXT_SECONDARY   0xFF94A3B8  /* Slate subtitle        */
#define GFX_COLOR_TEXT_MUTED       0xFF64748B  /* Muted placeholder     */
#define GFX_COLOR_TEXT_DARK        0xFF0F172A  /* Deep text for pills   */

/* Quick color pack */
static inline uint32_t gui_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)((0xFF << 24) | (r << 16) | (g << 8) | b);
}

static inline uint32_t gui_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (uint32_t)((a << 24) | (r << 16) | (g << 8) | b);
}

/* Fast alpha blending between two 0xAARRGGBB colors */
uint32_t renderer_alpha_blend(uint32_t src, uint32_t dst, uint8_t alpha);

/* Primitive drawing functions */
void renderer_blend_pixel(int x, int y, uint32_t color, uint8_t alpha);
void renderer_fill_alpha_rect(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void renderer_fill_alpha_rounded_rect(int x, int y, int w, int h, int radius, uint32_t color, uint8_t alpha);

/* Soft ambient drop shadows */
void renderer_draw_drop_shadow(int x, int y, int w, int h, int radius, int shadow_size, uint8_t max_alpha);

/* Liquid-Glass Panel */
void renderer_draw_glass_panel(int x, int y, int w, int h, int radius,
                               uint32_t body_color, uint8_t body_alpha,
                               uint32_t border_color, int shadow_depth);

/* Modern UI Components */
void renderer_draw_pill_button(int x, int y, int w, int h, const char* label,
                               uint32_t bg_color, uint32_t border_color, uint32_t text_color, int is_active);

void renderer_draw_badge(int x, int y, const char* text, uint32_t bg_color, uint32_t text_color);

void renderer_draw_meter(int x, int y, int w, int h, int percent,
                         uint32_t fill_color, uint32_t bg_color, uint32_t border_color);

void renderer_draw_card(int x, int y, int w, int h, int radius,
                        const char* header, uint32_t header_color,
                        uint32_t bg_color, uint32_t border_color);

#endif /* GUI_RENDERER_H */
