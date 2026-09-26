/* ==============================================================================
 * Greenhouse OS - Phase 6: Framebuffer Abstraction (implementation)
 * ==============================================================================
 */

#include "framebuffer.h"
#include "vbe.h"
#include "../io.h"
#include "../pmm.h"
#include "../vmm.h"
#include "../input/input.h"

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289
#define MB2_TAG_TYPE_END   0
#define MB2_TAG_TYPE_FRAMEBUFFER 8

static framebuffer_info_t fb_info;
static int fb_ready = 0;
static int fb_probed = 0;

/* Text mode register file captured before the first mode switch, so leaving
 * graphics mode puts the adapter back exactly as the firmware had it. */
static vga_text_state_t fb_text_state;
static int fb_text_state_valid = 0;

/* ------------------------------------------------------------------------------
 * Multiboot2 framebuffer tag parsing
 * -------------------------------------------------------------------------- */

static int fb_parse_multiboot2(uint32_t mb2_magic, uint64_t mb2_info_addr) {
    if (mb2_magic != MULTIBOOT2_BOOTLOADER_MAGIC || mb2_info_addr == 0) return -1;

    uint32_t total_size = *(volatile uint32_t*)mb2_info_addr;
    if (total_size < 8 || total_size > (4 * 1024 * 1024)) return -1;

    uint8_t* tag_ptr = (uint8_t*)(mb2_info_addr + 8);
    uint8_t* end_ptr = (uint8_t*)(mb2_info_addr + total_size);

    while (tag_ptr + 8 <= end_ptr) {
        uint32_t tag_type = *(volatile uint32_t*)tag_ptr;
        uint32_t tag_size = *(volatile uint32_t*)(tag_ptr + 4);
        if (tag_size < 8) break;

        if (tag_type == MB2_TAG_TYPE_FRAMEBUFFER) {
            if (tag_size < 32) { tag_ptr += (tag_size + 7) & ~7u; continue; }

            uint64_t addr   = *(volatile uint64_t*)(tag_ptr + 8);
            uint32_t pitch  = *(volatile uint32_t*)(tag_ptr + 16);
            uint32_t width  = *(volatile uint32_t*)(tag_ptr + 20);
            uint32_t height = *(volatile uint32_t*)(tag_ptr + 24);
            uint8_t  bpp    = *(volatile uint8_t*)(tag_ptr + 28);
            uint8_t  fb_type = *(volatile uint8_t*)(tag_ptr + 29);
            /* Only the direct RGB layout is present when the tag is longer. */
            uint16_t r_off, r_size, g_off, g_size, b_off, b_size;

            /* struct multiboot2_tag_framebuffer_direct:
             *   tag header (8) + common fields (20) + 6 x uint16 channel masks.
             * The masks therefore start at tag + 28 and the whole tag is
             * 8 + 20 + 24 = 52 bytes long. */
            if (fb_type == 1 && tag_size >= 52) {
                r_off   = (uint16_t)*(tag_ptr + 28);
                r_size  = (uint16_t)*(tag_ptr + 30);
                g_off   = (uint16_t)*(tag_ptr + 32);
                g_size  = (uint16_t)*(tag_ptr + 34);
                b_off   = (uint16_t)*(tag_ptr + 36);
                b_size  = (uint16_t)*(tag_ptr + 38);
            } else {
                r_off = 16; r_size = 8;
                g_off = 8;  g_size = 8;
                b_off = 0;  b_size = 8;
            }

            if (addr == 0 || width == 0 || height == 0 || pitch == 0) {
                tag_ptr += (tag_size + 7) & ~7u;
                continue;
            }
            if (width > 8192 || height > 4096) { /* sanity bound */
                tag_ptr += (tag_size + 7) & ~7u;
                continue;
            }

            fb_info.address = addr;
            fb_info.width = width;
            fb_info.height = height;
            fb_info.pitch = pitch;
            fb_info.bpp = bpp;
            fb_info.red_offset = r_off;
            fb_info.red_size = r_size;
            fb_info.green_offset = g_off;
            fb_info.green_size = g_size;
            fb_info.blue_offset = b_off;
            fb_info.blue_size = b_size;
            fb_info.backend = FB_BACKEND_MULTIBOOT2;
            return 0;
        }

        if (tag_type == MB2_TAG_TYPE_END) break;
        tag_ptr += (tag_size + 7) & ~7u;
    }
    return -1;
}

/* ------------------------------------------------------------------------------
 * Format classification
 * -------------------------------------------------------------------------- */

static void fb_classify_format(void) {
    uint32_t bpp = fb_info.bpp;
    uint32_t r_size = fb_info.red_size ? fb_info.red_size : 8;
    uint32_t g_size = fb_info.green_size ? fb_info.green_size : 8;
    uint32_t b_size = fb_info.blue_size ? fb_info.blue_size : 8;
    uint32_t r_off = fb_info.red_offset;
    uint32_t g_off = fb_info.green_offset;
    uint32_t b_off = fb_info.blue_offset;

    fb_info.format = FB_PIXEL_UNKNOWN;
    fb_info.bytes_per_pixel = 0;

    if (bpp == 32) {
        if (r_off == 16 && g_off == 8 && b_off == 0) {
            fb_info.format = FB_PIXEL_XRGB8888;
            fb_info.bytes_per_pixel = 4;
        } else if (r_off == 0 && g_off == 8 && b_off == 16) {
            fb_info.format = FB_PIXEL_BGRX8888;
            fb_info.bytes_per_pixel = 4;
        }
    } else if (bpp == 24) {
        if (r_off == 16 && g_off == 8 && b_off == 0) {
            fb_info.format = FB_PIXEL_RGB888;
            fb_info.bytes_per_pixel = 3;
        }
    } else if (bpp == 16) {
        if (r_size == 5 && g_size == 6 && b_size == 5) {
            fb_info.format = FB_PIXEL_RGB565;
            fb_info.bytes_per_pixel = 2;
        } else if (r_size == 5 && g_size == 5 && b_size == 5) {
            fb_info.format = FB_PIXEL_RGB555;
            fb_info.bytes_per_pixel = 2;
        }
    } else if (bpp == 8) {
        fb_info.format = FB_PIXEL_INDEXED8;
        fb_info.bytes_per_pixel = 1;
        fb_info.red_offset = fb_info.green_offset = fb_info.blue_offset = 0;
    } else if (bpp == 4) {
        fb_info.format = FB_PIXEL_INDEXED4;
        fb_info.bytes_per_pixel = 1; /* 2 pixels per byte */
    } else if (bpp == 1 || bpp == 2) {
        fb_info.format = FB_PIXEL_UNKNOWN;
    }
}

/* Read the VGA DAC so indexed modes can be resolved to real colours. */
static void fb_read_dac_palette(void) {
    for (int i = 0; i < 256; i++) {
        outb(0x3C7, (uint8_t)i);
        uint8_t r = (uint8_t)(inb(0x3C9) & 0x3F);
        uint8_t g = (uint8_t)(inb(0x3C9) & 0x3F);
        uint8_t b = (uint8_t)(inb(0x3C9) & 0x3F);
        /* 6 bit DAC to 8 bit: replicate the top bits, as VGA hardware does. */
        fb_info.palette[i] = (uint32_t)((r << 2) | (r >> 4)) << 16 |
                             (uint32_t)((g << 2) | (g >> 4)) << 8 |
                             (uint32_t)((b << 2) | (b >> 4));
    }
    fb_info.indexed_palette_valid = 1;
}

/* Nearest DAC entry for a colour.  Indexed modes are only used by the text
 * console and by mode detection; the GUI always runs in a direct colour mode,
 * but the mapping has to be correct if an indexed mode is selected. */
static uint8_t fb_nearest_palette_index(uint32_t color) {
    if (!fb_info.indexed_palette_valid) return 0;

    int best = 0;
    long best_d = -1;
    int limit = (fb_info.format == FB_PIXEL_INDEXED4) ? 16 : 256;

    for (int i = 0; i < limit; i++) {
        uint32_t p = fb_info.palette[i];
        long dr = (long)((p >> 16) & 0xFF) - (long)((color >> 16) & 0xFF);
        long dg = (long)((p >> 8) & 0xFF) - (long)((color >> 8) & 0xFF);
        long db = (long)(p & 0xFF) - (long)(color & 0xFF);
        long d = dr * dr + dg * dg + db * db;
        if (best_d < 0 || d < best_d) { best_d = d; best = i; }
    }
    return (uint8_t)best;
}

/* ------------------------------------------------------------------------------
 * Memory mapping
 * -------------------------------------------------------------------------- */

int framebuffer_invalidate_mapping(void) {
    fb_info.mode_active = 0;
    fb_ready = 0;
    return 0;
}

int framebuffer_has_vbe(void) {
    return fb_info.adapter_dispi;
}

int framebuffer_adapter_present(void) {
    return fb_probed || fb_info.backend != FB_BACKEND_NONE;
}

/* Identity map the framebuffer through the kernel's page tables.  The boot
 * loader identity maps only the first 1 GiB, while video adapters place their
 * linear framebuffer in the PCI hole well above that, so the existing VMM is
 * reused instead of adding a second mapping mechanism. */
static int fb_map_memory(uint64_t addr, uint64_t length) {
    if (addr == 0 || length == 0) return -1;
    if (addr + length < addr) return -1;              /* wraps the address space */
    if (addr + length > 0x1000000000ULL) return -1;    /* above the 64 GiB window */

    uint64_t start = addr & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t end   = (addr + length + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

    for (uint64_t page = start; page < end; page += PAGE_SIZE) {
        uintptr_t mapped = vmm_virt_to_phys((uintptr_t)page);
        if (mapped == (uintptr_t)page) continue; /* already identity mapped */
        if (mapped != 0) continue;              /* mapped elsewhere: leave it */

        if (vmm_map_page((uintptr_t)page, (uintptr_t)page,
                         VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_NO_CACHE) != 0) {
            return -1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------------
 * Public interface
 * -------------------------------------------------------------------------- */

void framebuffer_init(uint32_t mb2_magic, uint64_t mb2_info_addr) {
    fb_info.format = FB_PIXEL_UNKNOWN;
    fb_info.backend = FB_BACKEND_NONE;
    fb_info.mode_active = 0;
    fb_ready = 0;

    if (fb_parse_multiboot2(mb2_magic, mb2_info_addr) == 0) {
        fb_classify_format();
        if (fb_info.format == FB_PIXEL_UNKNOWN) {
            /* Unsupported geometry from the boot loader: forget it and let the
             * video driver look for a mode of its own. */
            fb_info.backend = FB_BACKEND_NONE;
            fb_info.address = 0;
        }
    }

    if (fb_info.backend == FB_BACKEND_NONE) {
        /* Nothing usable from the boot environment.  Probe the adapter so that
         * GFXINFO can describe it, but stay in text mode until graphics are
         * asked for explicitly. */
        vbe_adapter_t adapter;
        vbe_probe(&adapter);
        fb_probed = adapter.detected;

        fb_info.adapter_vendor = adapter.vendor_id;
        fb_info.adapter_device = adapter.device_id;
        fb_info.adapter_bar_size = adapter.bar0_size;
        fb_info.adapter_bus = adapter.bus;
        fb_info.adapter_device_number = adapter.device;
        fb_info.adapter_function = adapter.function;
        fb_info.adapter_dispi = adapter.dispi;
    }
}

int framebuffer_enter_mode_ex(uint32_t width, uint32_t height, uint32_t bpp) {
    if (fb_info.mode_active) return 0;

    /* Snapshot the current (text) register file before anything is reprogrammed
     * and before the linear framebuffer is enabled. */
    vbe_save_text_state(&fb_text_state);
    fb_text_state_valid = 1;

    if (fb_info.backend == FB_BACKEND_NONE || fb_info.address == 0) {
        /* Fall back to a driver policy mode on the emulated adapter and use
         * whatever the hardware reports back.  Zero means "keep the default". */
        vbe_mode_info_t mode;
        int rc = vbe_set_graphics_mode(width ? width : VBE_PREFERRED_WIDTH,
                                      height ? height : VBE_PREFERRED_HEIGHT,
                                      bpp ? bpp : VBE_PREFERRED_BPP, &mode);
        if (rc != 0 || !mode.ok) {
            fb_info.backend = FB_BACKEND_NONE;
            return rc;
        }

        fb_info.address = mode.address;
        fb_info.width = mode.width;
        fb_info.height = mode.height;
        fb_info.pitch = mode.stride;
        fb_info.bpp = mode.bits_per_pixel;

        /* DISPI only offers a byte based direct colour layout, but the channel
         * positions move with the pixel depth.  Assuming 8/8/8 for a 16bpp
         * mode would classify it as unknown and hand out red and blue swapped
         * or shifted, so describe each depth the adapter actually offers. */
        switch (fb_info.bpp) {
            case 16:  /* 5-6-5, little endian BGR565 in memory */
                fb_info.red_offset = 11; fb_info.red_size = 5;
                fb_info.green_offset = 5; fb_info.green_size = 6;
                fb_info.blue_offset = 0; fb_info.blue_size = 5;
                break;
            case 24:
            case 32:
                fb_info.red_offset = 16; fb_info.red_size = 8;
                fb_info.green_offset = 8; fb_info.green_size = 8;
                fb_info.blue_offset = 0; fb_info.blue_size = 8;
                break;
            default:
                return -8;
        }

        fb_info.backend = FB_BACKEND_BOCHS;
        fb_info.adapter_dispi = 1;
        fb_classify_format();
        if (fb_info.format == FB_PIXEL_UNKNOWN) return -6;
    } else if (fb_info.backend == FB_BACKEND_MULTIBOOT2) {
        /* The geometry came from the boot loader; re-arm the same mode on the
         * adapter and verify the stride the hardware really uses. */
        vbe_mode_info_t mode;
        int rc = vbe_set_graphics_mode(fb_info.width, fb_info.height, fb_info.bpp, &mode);
        if (rc == 0 && mode.ok) {
            /* Trust the hardware: the loader reported what the firmware
             * planned, the registers report what is really programmed. */
            if (mode.width != 0)  fb_info.width = mode.width;
            if (mode.height != 0) fb_info.height = mode.height;
            if (mode.bits_per_pixel != 0) fb_info.bpp = mode.bits_per_pixel;
            fb_info.pitch = mode.stride;
            if (mode.address != 0) fb_info.address = mode.address;
        }
        if (fb_info.pitch < (fb_info.width * fb_info.bpp) / 8) return -5;
    }

    if (fb_info.bytes_per_pixel == 0) {
        fb_info.bytes_per_pixel = fb_info.bpp / 8;
        if (fb_info.bytes_per_pixel == 0) fb_info.bytes_per_pixel = 1;
    }

    fb_info.size = fb_info.pitch * fb_info.height;

    if (fb_info.format == FB_PIXEL_INDEXED8 || fb_info.format == FB_PIXEL_INDEXED4) {
        fb_read_dac_palette();
    }

    if (fb_map_memory(fb_info.address, fb_info.size) != 0) return -7;

    /* The pointer range belongs to the display mode: a program drawing through
     * the graphics syscalls must be able to address the whole surface. */
    input_set_pointer_bounds((int)fb_info.width, (int)fb_info.height);

    fb_info.mode_active = 1;
    fb_ready = 1;
    return 0;
}

int framebuffer_enter_mode(void) {
    return framebuffer_enter_mode_ex(0, 0, 0);
}

/* Text geometry the pointer needs after the console comes back: the saved CRTC
 * holds the cell count and the character cell size, which is what the shell and
 * the GUI use to place the cursor. */
static uint32_t fb_text_geometry_width(const vga_text_state_t* st) {
    if (!st) return 0;
    uint32_t cols = (uint32_t)st->crtc[0x00] + 1u;
    uint32_t cell = (st->crtc[0x07] & 0x80) ? 9u : 8u;
    return cols * cell;
}

static uint32_t fb_text_geometry_height(const vga_text_state_t* st) {
    if (!st) return 0;
    uint32_t rows = (uint32_t)st->crtc[0x12] + 1u;
    uint32_t cell = (st->crtc[0x09] & 0x20) ? 16u : 8u;
    return rows * cell;
}

int framebuffer_leave_mode(void) {
    if (!fb_info.mode_active) return 0;

    if (fb_text_state_valid) {
        vbe_restore_text_state(&fb_text_state);
    } else {
        /* No snapshot (e.g. mode set before the text state could be read):
         * fall back to programming the classic 80x25 register table. */
        vbe_restore_text_mode();
    }
    input_set_pointer_bounds((int)fb_text_geometry_width(&fb_text_state),
                             (int)fb_text_geometry_height(&fb_text_state));
    fb_info.mode_active = 0;
    fb_ready = 0;
    return 0;
}

int framebuffer_verify_text_state_ex(framebuffer_text_diff_t* out) {
    /* Compares the live VGA register file against the snapshot taken before the
     * mode switch.  A matching register file means the adapter is back in the
     * text mode the firmware set up, which is what the shell console needs. */
    if (out) {
        out->misc_diff = out->seq_diff = out->crtc_diff = 0;
        out->gctl_diff = out->attr_diff = 0;
        out->first_seq = out->first_crtc = -1;
        out->first_gctl = out->first_attr = -1;
        out->first_crtc_now = out->first_crtc_want = 0;
        out->first_attr_now = out->first_attr_want = 0;
    }
    if (!fb_text_state_valid) return -1;

    vga_text_state_t now;
    vbe_save_text_state(&now);

    int diff = 0;
    if (now.misc != fb_text_state.misc) diff++;
    for (int i = 0; i < VGA_SEQ_REGS; i++) {
        if (now.seq[i] != fb_text_state.seq[i]) {
            diff++;
            if (out) { out->seq_diff++; if (out->first_seq < 0) out->first_seq = i; }
        }
    }
    for (int i = 0; i < VGA_CRTC_REGS; i++) {
        if (now.crtc[i] != fb_text_state.crtc[i]) {
            diff++;
            if (out) {
                out->crtc_diff++;
                if (out->first_crtc < 0) {
                    out->first_crtc = i;
                    out->first_crtc_now = now.crtc[i];
                    out->first_crtc_want = fb_text_state.crtc[i];
                }
            }
        }
    }
    for (int i = 0; i < VGA_GCTL_REGS; i++) {
        if (now.gctl[i] != fb_text_state.gctl[i]) {
            diff++;
            if (out) { out->gctl_diff++; if (out->first_gctl < 0) out->first_gctl = i; }
        }
    }
    for (int i = 0; i < VGA_ATTR_REGS; i++) {
        /* Attribute index 0x10 bit 5 is the write only "palette access enable"
         * latch: it never reads back as 1, so it is not a real difference. */
        uint8_t a = now.attr[i], b = fb_text_state.attr[i];
        if (i == 0x10) { a &= 0xDF; b &= 0xDF; }
        if (a != b) {
            diff++;
            if (out) {
                out->attr_diff++;
                if (out->first_attr < 0) {
                    out->first_attr = i;
                    out->first_attr_now = a;
                    out->first_attr_want = b;
                }
            }
        }
    }
    return diff;
}

int framebuffer_verify_text_state(void) {
    return framebuffer_verify_text_state_ex(NULL);
}

const framebuffer_info_t* framebuffer_get_info(void) {
    return &fb_info;
}

int framebuffer_is_ready(void) {
    return fb_ready;
}

const char* framebuffer_backend_name(fb_backend_t backend) {
    switch (backend) {
        case FB_BACKEND_MULTIBOOT2: return "Multiboot2 framebuffer tag";
        case FB_BACKEND_BOCHS:      return "Bochs VBE adapter registers";
        default:                    return "none (text mode only)";
    }
}

const char* framebuffer_format_name(fb_pixel_format_t format) {
    switch (format) {
        case FB_PIXEL_INDEXED8:   return "Indexed 8bpp (256 colour DAC)";
        case FB_PIXEL_INDEXED4:   return "Indexed 4bpp (16 colour DAC)";
        case FB_PIXEL_RGB555:     return "RGB555 16bpp";
        case FB_PIXEL_RGB565:     return "RGB565 16bpp";
        case FB_PIXEL_RGB888:     return "RGB888 24bpp";
        case FB_PIXEL_XRGB8888:   return "XRGB8888 32bpp";
        case FB_PIXEL_BGRX8888:   return "BGRX8888 32bpp";
        default:                  return "unknown";
    }
}

void framebuffer_get_report(framebuffer_report_t* out) {
    if (!out) return;
    out->width = fb_info.width;
    out->height = fb_info.height;
    out->pitch = fb_info.pitch;
    out->bpp = fb_info.bpp;
    out->bytes_per_pixel = fb_info.bytes_per_pixel;
    out->size = fb_info.size;
    out->format = (uint32_t)fb_info.format;
    out->red_offset = fb_info.red_offset;
    out->red_size = fb_info.red_size;
    out->green_offset = fb_info.green_offset;
    out->green_size = fb_info.green_size;
    out->blue_offset = fb_info.blue_offset;
    out->blue_size = fb_info.blue_size;
    out->mode_active = (uint32_t)fb_info.mode_active;
}

/* ------------------------------------------------------------------------------
 * Raw pixel access
 * -------------------------------------------------------------------------- */

static inline uint8_t* fb_base(void) {
    return (uint8_t*)(uintptr_t)fb_info.address;
}

static int fb_bounds_ok(int x, int y) {
    if (!fb_ready || fb_info.address == 0) return 0;
    if (x < 0 || y < 0) return 0;
    if ((uint32_t)x >= fb_info.width || (uint32_t)y >= fb_info.height) return 0;
    return 1;
}

int framebuffer_put_pixel(int x, int y, uint32_t color) {
    if (!fb_bounds_ok(x, y)) return -1;

    uint8_t* row = fb_base() + (uint64_t)y * fb_info.pitch;
    switch (fb_info.format) {
        case FB_PIXEL_XRGB8888:
        case FB_PIXEL_BGRX8888: {
            uint32_t r = (color >> 16) & 0xFF, g = (color >> 8) & 0xFF, b = color & 0xFF;
            uint32_t v = (r << fb_info.red_offset) | (g << fb_info.green_offset) | (b << fb_info.blue_offset);
            *(volatile uint32_t*)(row + (uint64_t)x * 4) = v;
            return 0;
        }
        case FB_PIXEL_RGB565: {
            uint32_t r = (color >> 19) & 0x1F, g = (color >> 10) & 0x3F, b = (color >> 3) & 0x1F;
            *(volatile uint16_t*)(row + (uint64_t)x * 2) = (uint16_t)((r << 11) | (g << 5) | b);
            return 0;
        }
        case FB_PIXEL_RGB555: {
            uint32_t r = (color >> 19) & 0x1F, g = (color >> 11) & 0x1F, b = (color >> 3) & 0x1F;
            *(volatile uint16_t*)(row + (uint64_t)x * 2) = (uint16_t)((r << 10) | (g << 5) | b);
            return 0;
        }
        case FB_PIXEL_RGB888: {
            uint8_t* p = row + (uint64_t)x * 3;
            p[0] = (uint8_t)(color >> 16);
            p[1] = (uint8_t)(color >> 8);
            p[2] = (uint8_t)color;
            return 0;
        }
        case FB_PIXEL_INDEXED8:
            row[x] = (uint8_t)fb_nearest_palette_index(color);
            return 0;
        case FB_PIXEL_INDEXED4: {
            /* Two pixels per byte: the high nibble is the even column. */
            uint8_t idx = 0;
            if (fb_info.indexed_palette_valid) idx = (uint8_t)fb_nearest_palette_index(color);
            uint8_t* p = row + (uint64_t)(x / 2);
            if (x & 1) *p = (uint8_t)((*p & 0xF0) | (idx & 0x0F));
            else      *p = (uint8_t)((*p & 0x0F) | ((idx & 0x0F) << 4));
            return 0;
        }
        default:
            return -1;
    }
}

int framebuffer_get_pixel(int x, int y, uint32_t* out_color) {
    if (!fb_bounds_ok(x, y) || !out_color) return -1;

    uint8_t* row = fb_base() + (uint64_t)y * fb_info.pitch;
    uint32_t color = 0;

    switch (fb_info.format) {
        case FB_PIXEL_XRGB8888:
        case FB_PIXEL_BGRX8888: {
            uint32_t v = *(volatile uint32_t*)(row + (uint64_t)x * 4);
            uint32_t r = (v >> fb_info.red_offset) & 0xFF;
            uint32_t g = (v >> fb_info.green_offset) & 0xFF;
            uint32_t b = (v >> fb_info.blue_offset) & 0xFF;
            color = (r << 16) | (g << 8) | b;
            break;
        }
        case FB_PIXEL_RGB565: {
            uint16_t v = *(volatile uint16_t*)(row + (uint64_t)x * 2);
            color = (((v >> 11) & 0x1F) << 19) | (((v >> 5) & 0x3F) << 10) | ((v & 0x1F) << 3);
            break;
        }
        case FB_PIXEL_RGB555: {
            uint16_t v = *(volatile uint16_t*)(row + (uint64_t)x * 2);
            color = (((v >> 10) & 0x1F) << 19) | (((v >> 5) & 0x1F) << 11) | ((v & 0x1F) << 3);
            break;
        }
        case FB_PIXEL_RGB888: {
            uint8_t* p = row + (uint64_t)x * 3;
            color = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
            break;
        }
        case FB_PIXEL_INDEXED8:
            color = fb_info.palette[row[x]];
            break;

        case FB_PIXEL_INDEXED4: {
            uint8_t byte = row[x / 2];
            uint8_t idx = (x & 1) ? (uint8_t)(byte & 0x0F) : (uint8_t)(byte >> 4);
            color = fb_info.palette[idx & 0x0F];
            break;
        }
        default:
            return -1;
    }

    *out_color = color & 0xFFFFFF;
    return 0;
}

int framebuffer_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!fb_ready || w <= 0 || h <= 0) return -1;

    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = x + w, y1 = y + h;
    if (x1 > (int)fb_info.width) x1 = (int)fb_info.width;
    if (y1 > (int)fb_info.height) y1 = (int)fb_info.height;
    if (x0 >= x1 || y0 >= y1) return 0;

    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            framebuffer_put_pixel(xx, yy, color);
        }
    }
    return 0;
}
