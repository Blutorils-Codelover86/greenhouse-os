/* ==============================================================================
 * Greenhouse OS - Phase 6: Bitmap Font & Text Rendering
 *
 * The font is compiled into the kernel image: text rendering never depends on
 * a mounted filesystem, so the graphical terminal and the graphics self test
 * work before any drive is available.
 * ==============================================================================
 */

#ifndef FONT_H
#define FONT_H

#include <stdint.h>
#include <stddef.h>

#define FONT_GLYPH_WIDTH   8
#define FONT_GLYPH_HEIGHT  16
#define FONT_FIRST_CHAR    0x20
#define FONT_LAST_CHAR     0x7E

/* Colour helpers: the framebuffer works with 0x00RRGGBB values. */
#define FONT_RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))
#define FONT_TRANSPARENT  ((uint32_t)0xFFFFFFFF)

void font_init(void);
int  font_glyph_width(void);
int  font_glyph_height(void);
const uint8_t* font_get_glyph(char c);

/* Width in pixels of a string rendered with the built-in font. */
int font_text_width(const char* text);
int font_text_height(const char* text);

/* Text primitives.  `spacing` is the extra pixel gap added between the glyph
 * cell and the next cell (0 gives the classic console look). */
void draw_char(int x, int y, char c, uint32_t fg, uint32_t bg, int spacing);
void draw_text(int x, int y, const char* text, uint32_t fg, uint32_t bg, int spacing);
int  draw_text_clipped(int x, int y, int max_width, const char* text, uint32_t fg, uint32_t bg, int spacing);

/* Centred text, used by the graphics self test and window title bars. */
void draw_text_centered(int x, int y, int area_width, const char* text, uint32_t fg, uint32_t bg, int spacing);

#endif /* FONT_H */
