/* ==============================================================================
 * Greenhouse OS - Phase 6: Graphics Subsystem
 *
 * Owns the drawing surface used by everything above it (window manager, GUI
 * applications, self tests):
 *
 *   - a back buffer allocated from the physical memory manager and mapped with
 *     the virtual memory manager (no second allocator is introduced),
 *   - a frame / present model so a whole composed screen reaches the visible
 *     framebuffer in one copy,
 *   - the 2D primitives (pixels, rectangles, lines, circles, clipping).
 *
 * If a full size back buffer cannot be allocated the subsystem falls back to
 * direct rendering: every primitive goes straight to the visible framebuffer
 * and graphics_present() only becomes a no-op.  This is reported by
 * graphics_has_back_buffer() and printed by the shell / self tests.
 * ==============================================================================
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>
#include "framebuffer.h"

/* Virtual base used to map the back buffer pool.  It must sit *outside* the 1 GiB
 * boot identity map: inside that range every address already resolves to a
 * 2 MiB huge page, so the "is this VA free?" probe would always report a
 * collision.  1 GiB is above the kernel image (1 MiB), user programs (4 MiB)
 * and the kernel heap (0x2000_0000), and nothing else is mapped there. */
#define GFX_BACKBUF_VIRT_BASE  0x0000000040000000ULL

void  graphics_init(void);

/* Switch the display into graphics mode and prepare the drawing surface.
 * Returns 0 on success, negative on failure (leaves the display untouched). */
int   graphics_enter(void);

/* Enter graphics mode at a requested resolution. Any argument may be 0 to keep
 * the driver default; the adapter's answer is what the surface ends up using. */
int   graphics_enter_ex(uint32_t width, uint32_t height, uint32_t bpp);

/* Return to the VGA text console. */
int   graphics_leave(void);

int   graphics_is_active(void);
int   graphics_has_back_buffer(void);

/* Why the last back buffer allocation failed, or 0 when it succeeded.
 * -1 size rejected, -2 virtual range busy, -3 out of physical pages,
 * -4 page table mapping failed. */
int   graphics_get_back_buffer_error(void);
void* graphics_get_back_buffer(void);
size_t graphics_get_back_buffer_size(void);

/* Frame lifecycle */
void  graphics_begin_frame(void);
void  graphics_end_frame(void);
void  graphics_present(void);
void  graphics_force_present(void);

/* Geometry of the active drawing surface */
int   graphics_get_width(void);
int   graphics_get_height(void);
int   graphics_get_pitch(void);
int   graphics_get_bpp(void);

/* Clipping rectangle (window manager uses it to confine drawing) */
void  graphics_set_clip(int x, int y, int w, int h);
void  graphics_clear_clip(void);
int   graphics_get_clip(int* x, int* y, int* w, int* h);

/* Primitives - all clipped, none of them can write outside the surface. */
void  graphics_put_pixel(int x, int y, uint32_t color);
int   graphics_get_pixel(int x, int y, uint32_t* out_color);
void  graphics_clear(uint32_t color);
void  graphics_draw_rect(int x, int y, int w, int h, uint32_t color);
void  graphics_fill_rect(int x, int y, int w, int h, uint32_t color);
void  graphics_draw_rect_thick(int x, int y, int w, int h, uint32_t color, int thickness);
void  graphics_draw_hline(int x, int y, int w, uint32_t color);
void  graphics_draw_vline(int x, int y, int h, uint32_t color);
void  graphics_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void  graphics_draw_circle(int cx, int cy, int radius, uint32_t color);
void  graphics_fill_circle(int cx, int cy, int radius, uint32_t color);
void  graphics_fill_gradient_v(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
void  graphics_fill_gradient_h(int x, int y, int w, int h, uint32_t left, uint32_t right);
void  graphics_draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void  graphics_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void  graphics_fill_rounded_gradient_v(int x, int y, int w, int h, int r, uint32_t top, uint32_t bottom);
void  graphics_fill_glass_panel(int x, int y, int w, int h, int r, uint32_t bg_top, uint32_t bg_bot, uint32_t border_col, uint32_t highlight_col);
void  graphics_blit(int x, int y, int w, int h, const uint32_t* pixels, int src_pitch);

/* Colour helpers */
uint32_t graphics_rgb(int r, int g, int b);
uint32_t graphics_blend(uint32_t a, uint32_t b, uint32_t numerator, uint32_t denominator);

uint64_t graphics_get_present_count(void);

#endif /* GRAPHICS_H */
