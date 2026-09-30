/* ==============================================================================
 * Greenhouse OS - Phase 6: Graphics Subsystem (implementation)
 * ==============================================================================
 */

#include "graphics.h"
#include "font.h"
#include "../pmm.h"
#include "../vmm.h"

/* Drawing surface state */
static int      gfx_active = 0;
static uint8_t* gfx_target = NULL;      /* current drawing target (linear)   */
static uint32_t gfx_pitch = 0;
static uint32_t gfx_width = 0;
static uint32_t gfx_height = 0;
static uint32_t gfx_bpp = 0;
static uint32_t gfx_bytes_pp = 4;

/* Back buffer state */
static uint8_t* gfx_back_buffer = NULL;
static size_t   gfx_back_buffer_size = 0;
static uint64_t gfx_back_buffer_frames = 0;
static int      gfx_back_buffer_ok = 0;
static int      gfx_back_buffer_error = 0;

/* Frame bookkeeping */
static int      gfx_in_frame = 0;
static int      gfx_frame_dirty = 0;
static int      gfx_dirty_min_y = 0;
static int      gfx_dirty_max_y = 0;
static uint64_t gfx_present_count = 0;
static uint64_t gfx_frames = 0;

/* Clipping rectangle */
static int clip_x = 0, clip_y = 0, clip_w = 0, clip_h = 0;

/* ------------------------------------------------------------------------------
 * Back buffer management (PMM + VMM, no private allocator)
 * -------------------------------------------------------------------------- */

static void gfx_free_back_buffer(void) {
    if (!gfx_back_buffer) return;

    uint64_t virt = (uint64_t)(uintptr_t)gfx_back_buffer;
    for (uint64_t i = 0; i < gfx_back_buffer_frames; i++) {
        uintptr_t page = (uintptr_t)(virt + i * PAGE_SIZE);
        uintptr_t phys = vmm_virt_to_phys(page);
        vmm_unmap_page(page);
        if (phys) pmm_free_frame(phys);
    }
    gfx_back_buffer = NULL;
    gfx_back_buffer_size = 0;
    gfx_back_buffer_frames = 0;
    gfx_back_buffer_ok = 0;
}

static uint8_t* gfx_alloc_back_buffer(size_t size) {
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    gfx_back_buffer_error = 0;
    if (pages == 0 || pages > 4096) { /* refuse absurd pools */
        gfx_back_buffer_error = -1;
        return NULL;
    }

    uint64_t virt = GFX_BACKBUF_VIRT_BASE;

    for (uint64_t i = 0; i < pages; i++) {
        uintptr_t page = (uintptr_t)(virt + i * PAGE_SIZE);
        if (vmm_virt_to_phys(page) != 0) { /* something already owns the VA */
            for (uint64_t j = 0; j < i; j++) {
                uintptr_t p = (uintptr_t)(virt + j * PAGE_SIZE);
                uintptr_t phys = vmm_virt_to_phys(p);
                vmm_unmap_page(p);
                if (phys) pmm_free_frame(phys);
            }
            gfx_back_buffer_error = -2;
            return NULL;
        }

        uintptr_t frame = pmm_alloc_frame();
        if (frame == 0) {
            for (uint64_t j = 0; j < i; j++) {
                uintptr_t p = (uintptr_t)(virt + j * PAGE_SIZE);
                uintptr_t phys = vmm_virt_to_phys(p);
                vmm_unmap_page(p);
                if (phys) pmm_free_frame(phys);
            }
            gfx_back_buffer_error = -3;
            return NULL;
        }

        if (vmm_map_page(page, frame, VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE) != 0) {
            pmm_free_frame(frame);
            for (uint64_t j = 0; j < i; j++) {
                uintptr_t p = (uintptr_t)(virt + j * PAGE_SIZE);
                uintptr_t phys = vmm_virt_to_phys(p);
                vmm_unmap_page(p);
                if (phys) pmm_free_frame(phys);
            }
            gfx_back_buffer_error = -4;
            return NULL;
        }
    }

    gfx_back_buffer_frames = pages;
    return (uint8_t*)(uintptr_t)virt;
}

/* ------------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

void graphics_init(void) {
    font_init();
    gfx_active = 0;
    gfx_back_buffer = NULL;
    gfx_back_buffer_size = 0;
    gfx_back_buffer_frames = 0;
    gfx_back_buffer_ok = 0;
    gfx_back_buffer_error = 0;
}

int graphics_enter_ex(uint32_t width, uint32_t height, uint32_t bpp) {
    if (gfx_active) return 0;

    int rc = framebuffer_enter_mode_ex(width, height, bpp);
    if (rc != 0) return rc;

    const framebuffer_info_t* fb = framebuffer_get_info();
    gfx_width = fb->width;
    gfx_height = fb->height;
    gfx_pitch = fb->pitch;
    gfx_bpp = fb->bpp;
    gfx_bytes_pp = fb->bytes_per_pixel ? fb->bytes_per_pixel : 4;

    /* Re-acquire a back buffer for the mode that is actually active. */
    gfx_free_back_buffer();
    uint8_t* buffer = gfx_alloc_back_buffer((size_t)fb->size);
    if (buffer) {
        gfx_back_buffer = buffer;
        gfx_back_buffer_size = fb->size;
        gfx_back_buffer_ok = 1;
    }

    gfx_active = 1;
    gfx_in_frame = 0;
    gfx_frame_dirty = 0;
    gfx_target = gfx_back_buffer ? gfx_back_buffer : (uint8_t*)(uintptr_t)fb->address;
    gfx_frames = 0;
    gfx_present_count = 0;

    graphics_clear_clip();
    return 0;
}

int graphics_enter(void) {
    return graphics_enter_ex(0, 0, 0);
}

int graphics_leave(void) {
    if (!gfx_active) return 0;

    gfx_active = 0;
    gfx_in_frame = 0;
    gfx_target = NULL;
    gfx_back_buffer_ok = 0;
    gfx_width = gfx_height = gfx_pitch = gfx_bpp = 0;

    /* Release the buffer pool only after the display is back in text mode so
     * no drawing can touch freed frames. */
    framebuffer_leave_mode();
    gfx_free_back_buffer();
    return 0;
}

int graphics_is_active(void) {
    return gfx_active;
}

int graphics_has_back_buffer(void) {
    return gfx_back_buffer != NULL;
}

int graphics_get_back_buffer_error(void) {
    return gfx_back_buffer_error;
}

void* graphics_get_back_buffer(void) {
    return gfx_back_buffer;
}

size_t graphics_get_back_buffer_size(void) {
    return gfx_back_buffer_size;
}

uint64_t graphics_get_present_count(void) {
    return gfx_present_count;
}

int graphics_get_width(void)  { return (int)gfx_width; }
int graphics_get_height(void) { return (int)gfx_height; }
int graphics_get_pitch(void)  { return (int)gfx_pitch; }
int graphics_get_bpp(void)    { return (int)gfx_bpp; }
void* graphics_get_target(void) { return gfx_target; }
void  graphics_mark_dirty(void) { gfx_frame_dirty = 1; }

void graphics_damage_rows(int y0, int y1) {
    if (y0 < 0) y0 = 0;
    if (y1 >= (int)gfx_height) y1 = (int)gfx_height - 1;
    if (y0 < gfx_dirty_min_y) gfx_dirty_min_y = y0;
    if (y1 > gfx_dirty_max_y) gfx_dirty_max_y = y1;
    gfx_frame_dirty = 1;
}

void graphics_begin_frame(void) {
    if (!gfx_active) return;
    gfx_frames++;
    gfx_frame_dirty = 0;
    gfx_dirty_min_y = (int)gfx_height;
    gfx_dirty_max_y = 0;
    gfx_target = gfx_back_buffer ? gfx_back_buffer : (uint8_t*)(uintptr_t)framebuffer_get_info()->address;
    graphics_clear_clip();
    gfx_in_frame = 1;
}

void graphics_end_frame(void) {
    gfx_in_frame = 0;
}

void graphics_present(void) {
    if (!gfx_active) return;
    if (!gfx_frame_dirty) return;

    const framebuffer_info_t* fb = framebuffer_get_info();
    if (gfx_back_buffer) {
        uint8_t* dst = (uint8_t*)(uintptr_t)fb->address;
        uint8_t* src = gfx_back_buffer;

        uint32_t y0 = (gfx_dirty_min_y < 0) ? 0 : (uint32_t)gfx_dirty_min_y;
        uint32_t y1 = (gfx_dirty_max_y >= (int)gfx_height) ? gfx_height : (uint32_t)(gfx_dirty_max_y + 1);
        if (y0 >= y1) {
            y0 = 0;
            y1 = gfx_height;
        }

        if (gfx_pitch == fb->pitch) {
            /* Contiguous memory copy using 64-bit words */
            uint64_t offset = (uint64_t)y0 * gfx_pitch;
            uint64_t* d = (uint64_t*)(dst + offset);
            const uint64_t* s = (const uint64_t*)(src + offset);
            size_t qwords = ((size_t)(y1 - y0) * gfx_pitch) / sizeof(uint64_t);
            for (size_t i = 0; i < qwords; i++) {
                d[i] = s[i];
            }
        } else {
            size_t line_qwords = gfx_pitch / sizeof(uint64_t);
            for (uint32_t y = y0; y < y1; y++) {
                uint64_t* d = (uint64_t*)(dst + (uint64_t)y * fb->pitch);
                const uint64_t* s = (const uint64_t*)(src + (uint64_t)y * gfx_pitch);
                for (size_t i = 0; i < line_qwords; i++) d[i] = s[i];
            }
        }
    }
    gfx_present_count++;
    gfx_frame_dirty = 0;
    gfx_dirty_min_y = (int)gfx_height;
    gfx_dirty_max_y = 0;
}

void graphics_force_present(void) {
    gfx_dirty_min_y = 0;
    gfx_dirty_max_y = (int)gfx_height - 1;
    gfx_frame_dirty = 1;
    graphics_present();
}

/* ------------------------------------------------------------------------------
 * Clipping
 * -------------------------------------------------------------------------- */

void graphics_set_clip(int x, int y, int w, int h) {
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = x + w, y1 = y + h;
    if (x1 > (int)gfx_width) x1 = (int)gfx_width;
    if (y1 > (int)gfx_height) y1 = (int)gfx_height;
    if (x0 > x1) x0 = x1;
    if (y0 > y1) y0 = y1;
    clip_x = x0; clip_y = y0; clip_w = x1 - x0; clip_h = y1 - y0;
}

void graphics_clear_clip(void) {
    clip_x = 0; clip_y = 0; clip_w = (int)gfx_width; clip_h = (int)gfx_height;
}

int graphics_get_clip(int* x, int* y, int* w, int* h) {
    if (x) *x = clip_x;
    if (y) *y = clip_y;
    if (w) *w = clip_w;
    if (h) *h = clip_h;
    return (clip_w > 0 && clip_h > 0);
}

/* ------------------------------------------------------------------------------
 * Pixels
 * -------------------------------------------------------------------------- */

static inline int gfx_point_visible(int x, int y) {
    if (!gfx_active || !gfx_target) return 0;
    if (x < clip_x || y < clip_y) return 0;
    if (x >= clip_x + clip_w || y >= clip_y + clip_h) return 0;
    return 1;
}

void graphics_put_pixel(int x, int y, uint32_t color) {
    if (!gfx_point_visible(x, y)) return;
    if ((uint32_t)x >= gfx_width || (uint32_t)y >= gfx_height) return;

    uint8_t* p = gfx_target + (uint64_t)y * gfx_pitch + (uint64_t)x * gfx_bytes_pp;
    switch (gfx_bpp) {
        case 32:
            *(volatile uint32_t*)p = ((((color >> 16) & 0xFF) << 16) |
                                      (((color >> 8) & 0xFF) << 8) |
                                      (color & 0xFF));
            break;
        case 24:
            p[0] = (uint8_t)(color >> 16);
            p[1] = (uint8_t)(color >> 8);
            p[2] = (uint8_t)color;
            break;
        case 16: {
            uint32_t v = (((color >> 19) & 0x1F) << 11) | (((color >> 10) & 0x3F) << 5) | ((color >> 3) & 0x1F);
            *(volatile uint16_t*)p = (uint16_t)v;
            break;
        }
        case 8:
            p[0] = (uint8_t)color;
            break;
        default:
            return;
    }
    if (y < gfx_dirty_min_y) gfx_dirty_min_y = y;
    if (y > gfx_dirty_max_y) gfx_dirty_max_y = y;
    gfx_frame_dirty = 1;
}

int graphics_get_pixel(int x, int y, uint32_t* out_color) {
    if (!out_color) return -1;
    if (!gfx_active || !gfx_target) return -1;
    if (x < 0 || y < 0 || (uint32_t)x >= gfx_width || (uint32_t)y >= gfx_height) return -1;

    const uint8_t* p = gfx_target + (uint64_t)y * gfx_pitch + (uint64_t)x * gfx_bytes_pp;
    switch (gfx_bpp) {
        case 32: {
            uint32_t v = *(volatile uint32_t*)p;
            *out_color = v & 0xFFFFFF;
            return 0;
        }
        case 24:
            *out_color = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
            return 0;
        case 16: {
            uint16_t v = *(volatile uint16_t*)p;
            *out_color = (((v >> 11) & 0x1F) << 19) | (((v >> 5) & 0x3F) << 10) | ((v & 0x1F) << 3);
            return 0;
        }
        case 8:
            *out_color = p[0];
            return 0;
        default:
            return -1;
    }
}

void graphics_clear(uint32_t color) {
    graphics_fill_rect(0, 0, (int)gfx_width, (int)gfx_height, color);
}

/* ------------------------------------------------------------------------------
 * Rectangles, lines, circles
 * -------------------------------------------------------------------------- */

void graphics_draw_hline(int x, int y, int w, uint32_t color) {
    for (int i = 0; i < w; i++) graphics_put_pixel(x + i, y, color);
}

void graphics_draw_vline(int x, int y, int h, uint32_t color) {
    for (int i = 0; i < h; i++) graphics_put_pixel(x, y + i, color);
}

void graphics_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!gfx_active || !gfx_target || w <= 0 || h <= 0) return;

    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = x + w, y1 = y + h;
    if (x1 > (int)gfx_width) x1 = (int)gfx_width;
    if (y1 > (int)gfx_height) y1 = (int)gfx_height;
    if (x0 >= x1 || y0 >= y1) return;

    /* Intersect with the clip rectangle so wide fills stay cheap. */
    if (x0 < clip_x) x0 = clip_x;
    if (y0 < clip_y) y0 = clip_y;
    if (x1 > clip_x + clip_w) x1 = clip_x + clip_w;
    if (y1 > clip_y + clip_h) y1 = clip_y + clip_h;
    if (x0 >= x1 || y0 >= y1) return;

    for (int yy = y0; yy < y1; yy++) {
        uint8_t* p = gfx_target + (uint64_t)yy * gfx_pitch;
        switch (gfx_bpp) {
            case 32: {
                uint32_t v = ((((color >> 16) & 0xFF) << 16) |
                              (((color >> 8) & 0xFF) << 8) | (color & 0xFF));
                volatile uint32_t* row = (volatile uint32_t*)(p + (uint64_t)x0 * 4);
                for (int xx = 0; xx < x1 - x0; xx++) row[xx] = v;
                break;
            }
            case 24: {
                uint8_t* row = p + (uint64_t)x0 * 3;
                for (int xx = 0; xx < x1 - x0; xx++) {
                    row[xx * 3 + 0] = (uint8_t)(color >> 16);
                    row[xx * 3 + 1] = (uint8_t)(color >> 8);
                    row[xx * 3 + 2] = (uint8_t)color;
                }
                break;
            }
            case 16: {
                uint16_t v = (uint16_t)((((color >> 19) & 0x1F) << 11) |
                                        (((color >> 10) & 0x3F) << 5) |
                                        ((color >> 3) & 0x1F));
                volatile uint16_t* row = (volatile uint16_t*)(p + (uint64_t)x0 * 2);
                for (int xx = 0; xx < x1 - x0; xx++) row[xx] = v;
                break;
            }
            case 8: {
                uint8_t* row = p + (uint64_t)x0;
                for (int xx = 0; xx < x1 - x0; xx++) row[xx] = (uint8_t)color;
                break;
            }
            default:
                return;
        }
    }
    if (y0 < gfx_dirty_min_y) gfx_dirty_min_y = y0;
    if (y1 - 1 > gfx_dirty_max_y) gfx_dirty_max_y = y1 - 1;
    gfx_frame_dirty = 1;
}

void graphics_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    graphics_draw_hline(x, y, w, color);
    graphics_draw_hline(x, y + h - 1, w, color);
    graphics_draw_vline(x, y + 1, h - 2, color);
    graphics_draw_vline(x + w - 1, y + 1, h - 2, color);
}

void graphics_draw_rect_thick(int x, int y, int w, int h, uint32_t color, int thickness) {
    if (thickness < 1) thickness = 1;
    for (int i = 0; i < thickness; i++) {
        graphics_draw_hline(x + i, y + i, w - 2 * i, color);
        graphics_draw_hline(x + i, y + h - 1 - i, w - 2 * i, color);
        graphics_draw_vline(x + i, y + i, h - 2 * i, color);
        graphics_draw_vline(x + w - 1 - i, y + i, h - 2 * i, color);
    }
}

void graphics_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x1 > x0) ? 1 : -1;
    int sy = (y1 > y0) ? 1 : -1;
    int err = dx - dy;

    for (int guard = 0; guard < 8192; guard++) {
        graphics_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err << 1;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

void graphics_draw_circle(int cx, int cy, int radius, uint32_t color) {
    if (radius <= 0) return;
    int x = radius, y = 0, err = 1 - radius;

    while (x >= y) {
        graphics_put_pixel(cx + x, cy + y, color);
        graphics_put_pixel(cx + y, cy + x, color);
        graphics_put_pixel(cx - y, cy + x, color);
        graphics_put_pixel(cx - x, cy + y, color);
        graphics_put_pixel(cx - x, cy - y, color);
        graphics_put_pixel(cx - y, cy - x, color);
        graphics_put_pixel(cx + y, cy - x, color);
        graphics_put_pixel(cx + x, cy - y, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void graphics_fill_circle(int cx, int cy, int radius, uint32_t color) {
    if (radius <= 0) return;
    for (int dy = -radius; dy <= radius; dy++) {
        int span = (int)(radius * radius - dy * dy);
        int dx = 0;
        while ((dx + 1) * (dx + 1) <= span) dx++;
        graphics_draw_hline(cx - dx, cy + dy, 2 * dx + 1, color);
    }
}

uint32_t graphics_rgb(int r, int g, int b) {
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

uint32_t graphics_blend(uint32_t a, uint32_t b, uint32_t numerator, uint32_t denominator) {
    if (denominator == 0) return a;
    uint32_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    uint32_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    uint32_t r = (ar * numerator + br * (denominator - numerator)) / denominator;
    uint32_t g = (ag * numerator + bg * (denominator - numerator)) / denominator;
    uint32_t bl = (ab * numerator + bb * (denominator - numerator)) / denominator;
    return (r << 16) | (g << 8) | bl;
}

void graphics_fill_gradient_v(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    if (h <= 0) return;
    for (int i = 0; i < h; i++) {
        uint32_t c = (h == 1) ? top : graphics_blend(top, bottom, (uint32_t)(h - 1 - i), (uint32_t)(h - 1));
        graphics_fill_rect(x, y + i, w, 1, c);
    }
}

void graphics_fill_gradient_h(int x, int y, int w, int h, uint32_t left, uint32_t right) {
    if (w <= 0) return;
    for (int i = 0; i < w; i++) {
        uint32_t c = (w == 1) ? left : graphics_blend(left, right, (uint32_t)(w - 1 - i), (uint32_t)(w - 1));
        graphics_fill_rect(x + i, y, 1, h, c);
    }
}

void graphics_blit(int x, int y, int w, int h, const uint32_t* pixels, int src_pitch) {
    if (!pixels || w <= 0 || h <= 0) return;
    int sp = (src_pitch > 0) ? src_pitch : w * 4;

    for (int yy = 0; yy < h; yy++) {
        const uint32_t* src = (const uint32_t*)((const uint8_t*)pixels + (int64_t)yy * sp);
        for (int xx = 0; xx < w; xx++) {
            graphics_put_pixel(x + xx, y + yy, src[xx]);
        }
    }
}

static int isqrt(int val) {
    if (val <= 0) return 0;
    int x = val;
    int y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + val / x) / 2;
    }
    return x;
}

void graphics_draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (r <= 0) {
        graphics_draw_rect(x, y, w, h, color);
        return;
    }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    /* Straight horizontal lines */
    graphics_draw_hline(x + r, y, w - 2 * r, color);
    graphics_draw_hline(x + r, y + h - 1, w - 2 * r, color);

    /* Straight vertical lines */
    graphics_draw_vline(x, y + r, h - 2 * r, color);
    graphics_draw_vline(x + w - 1, y + r, h - 2 * r, color);

    /* Corner arcs */
    int cx0 = x + r, cy0 = y + r;
    int cx1 = x + w - 1 - r, cy1 = y + h - 1 - r;

    int cur_x = r, cur_y = 0, err = 1 - r;
    while (cur_x >= cur_y) {
        /* Top-Left */
        graphics_put_pixel(cx0 - cur_x, cy0 - cur_y, color);
        graphics_put_pixel(cx0 - cur_y, cy0 - cur_x, color);
        /* Top-Right */
        graphics_put_pixel(cx1 + cur_x, cy0 - cur_y, color);
        graphics_put_pixel(cx1 + cur_y, cy0 - cur_x, color);
        /* Bottom-Left */
        graphics_put_pixel(cx0 - cur_x, cy1 + cur_y, color);
        graphics_put_pixel(cx0 - cur_y, cy1 + cur_x, color);
        /* Bottom-Right */
        graphics_put_pixel(cx1 + cur_x, cy1 + cur_y, color);
        graphics_put_pixel(cx1 + cur_y, cy1 + cur_x, color);

        cur_y++;
        if (err < 0) {
            err += 2 * cur_y + 1;
        } else {
            cur_x--;
            err += 2 * (cur_y - cur_x) + 1;
        }
    }
}

void graphics_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (r <= 0) {
        graphics_fill_rect(x, y, w, h, color);
        return;
    }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    int r2 = r * r;
    for (int dy = 0; dy < h; dy++) {
        int span_dx = 0;
        if (dy < r) {
            int d = r - 1 - dy;
            span_dx = r - isqrt(r2 - d * d);
        } else if (dy >= h - r) {
            int d = dy - (h - r);
            span_dx = r - isqrt(r2 - d * d);
        }
        int line_w = w - 2 * span_dx;
        if (line_w > 0) {
            graphics_draw_hline(x + span_dx, y + dy, line_w, color);
        }
    }
}

void graphics_fill_rounded_gradient_v(int x, int y, int w, int h, int r, uint32_t top, uint32_t bottom) {
    if (w <= 0 || h <= 0) return;
    if (r <= 0) {
        graphics_fill_gradient_v(x, y, w, h, top, bottom);
        return;
    }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    int r2 = r * r;
    for (int dy = 0; dy < h; dy++) {
        int span_dx = 0;
        if (dy < r) {
            int d = r - 1 - dy;
            span_dx = r - isqrt(r2 - d * d);
        } else if (dy >= h - r) {
            int d = dy - (h - r);
            span_dx = r - isqrt(r2 - d * d);
        }
        int line_w = w - 2 * span_dx;
        if (line_w > 0) {
            uint32_t c = (h == 1) ? top : graphics_blend(top, bottom, (uint32_t)(h - 1 - dy), (uint32_t)(h - 1));
            graphics_draw_hline(x + span_dx, y + dy, line_w, c);
        }
    }
}

void graphics_fill_glass_panel(int x, int y, int w, int h, int r, uint32_t bg_top, uint32_t bg_bot, uint32_t border_col, uint32_t highlight_col) {
    if (w <= 0 || h <= 0) return;
    /* 1. Main glass body gradient */
    graphics_fill_rounded_gradient_v(x, y, w, h, r, bg_top, bg_bot);
    /* 2. Top inner specular highlight */
    if (h > 4 && w > 2 * r) {
        int hr = (r > 1) ? r - 1 : 0;
        graphics_draw_hline(x + hr + 1, y + 1, w - 2 * hr - 2, highlight_col);
    }
    /* 3. Outer crisp border */
    graphics_draw_rounded_rect(x, y, w, h, r, border_col);
}
