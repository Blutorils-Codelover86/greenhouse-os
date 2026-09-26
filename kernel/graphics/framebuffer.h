/* ==============================================================================
 * Greenhouse OS - Phase 6: Framebuffer Abstraction
 *
 * The framebuffer geometry is never assumed.  It comes from the boot
 * environment (Multiboot2 framebuffer tag) and, when the boot loader did not
 * hand one over, from the VGA/VBE register interface of the emulated adapter
 * (see vbe.h).  All derived information (pitch, bytes per pixel, channel masks,
 * physical address) is read back from the hardware after the mode is set.
 * ==============================================================================
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>
#include "vbe.h"

typedef enum {
    FB_PIXEL_UNKNOWN = 0,
    FB_PIXEL_INDEXED8,     /* 8bpp, 256 colour DAC palette (VGA compatible)  */
    FB_PIXEL_INDEXED4,     /* 4bpp, 16 colour DAC palette (text mode style)  */
    FB_PIXEL_RGB555,       /* 16bpp, 5 bits per channel                      */
    FB_PIXEL_RGB565,       /* 16bpp, 6/5/5                                   */
    FB_PIXEL_RGB888,       /* 24bpp packed                                   */
    FB_PIXEL_XRGB8888,     /* 32bpp, byte per pixel, one channel ignored     */
    FB_PIXEL_BGRX8888      /* 32bpp with swapped red/blue (BGR order)        */
} fb_pixel_format_t;

typedef enum {
    FB_BACKEND_NONE = 0,
    FB_BACKEND_MULTIBOOT2,  /* geometry taken from the Multiboot2 fb tag     */
    FB_BACKEND_BOCHS        /* geometry obtained from a Bochs VBE mode setup  */
} fb_backend_t;

typedef struct {
    uint64_t address;        /* linear (identity mapped) framebuffer address */
    uint32_t width;
    uint32_t height;
    uint32_t pitch;          /* bytes per scanline, >= width * bytes_per_pixel */
    uint32_t bpp;            /* bits per pixel                                */
    uint32_t bytes_per_pixel;
    uint32_t size;           /* pitch * height in bytes                       */
    uint32_t red_offset, red_size;
    uint32_t green_offset, green_size;
    uint32_t blue_offset, blue_size;
    fb_pixel_format_t format;
    fb_backend_t backend;
    int indexed_palette_valid;
    uint32_t palette[256];   /* 0x00RRGGBB entries for indexed modes          */
    int mode_active;         /* hardware is currently in this graphics mode  */

    /* Adapter description (from the PCI scan) */
    uint16_t adapter_vendor;
    uint16_t adapter_device;
    uint32_t adapter_bar_size;
    uint8_t  adapter_bus, adapter_device_number, adapter_function;
    int      adapter_dispi;  /* adapter implements the Bochs VBE registers   */
} framebuffer_info_t;

/* Probe the boot environment and remember the framebuffer description.
 * Called once during kernel start-up; does not change the video mode. */
void framebuffer_init(uint32_t mb2_magic, uint64_t mb2_info_addr);

/* Bring the probed framebuffer on screen (enables the graphics mode). */
int  framebuffer_enter_mode(void);

/* Same, but requests a specific mode.  Any argument may be 0 to keep the
 * driver's default for that field.  Returns negative on refusal. */
int  framebuffer_enter_mode_ex(uint32_t width, uint32_t height, uint32_t bpp);

/* Return the display to the original text console. */
int  framebuffer_leave_mode(void);

/* Number of VGA registers that differ from the state captured before the mode
 * switch; 0 means the adapter is exactly back in the firmware text mode.
 * Returns -1 when no snapshot is available. */
int  framebuffer_verify_text_state(void);

/* Per group detail behind framebuffer_verify_text_state(). */
typedef struct {
    int misc_diff;
    int seq_diff;
    int crtc_diff;
    int gctl_diff;
    int attr_diff;
    int first_seq;    /* first differing sequencer index, -1 if none      */
    int first_crtc;   /* first differing CRTC index, -1 if none           */
    int first_gctl;   /* first differing graphics controller index       */
    int first_attr;   /* first differing attribute index                 */
    uint8_t first_crtc_now;
    uint8_t first_crtc_want;
    uint8_t first_attr_now;
    uint8_t first_attr_want;
} framebuffer_text_diff_t;

int  framebuffer_verify_text_state_ex(framebuffer_text_diff_t* out);

const framebuffer_info_t* framebuffer_get_info(void);
int  framebuffer_is_ready(void);

/* Diagnostic string for framebuffer_get_info()->backend. */
const char* framebuffer_backend_name(fb_backend_t backend);
const char* framebuffer_format_name(fb_pixel_format_t format);

/* Called when the graphics mode is torn down: the mapping itself is kept (the
 * address is reserved by the adapter) but any access is refused. */
int  framebuffer_invalidate_mapping(void);

/* True when a legacy VBE (Bochs/DISPI) adapter was found on the PCI bus. */
int  framebuffer_has_vbe(void);

/* True when any display adapter was found while probing. */
int  framebuffer_adapter_present(void);

/* Raw pixel access (identity mapped, bounds checked against the active mode). */
int  framebuffer_put_pixel(int x, int y, uint32_t color);
int  framebuffer_get_pixel(int x, int y, uint32_t* out_color);
int  framebuffer_fill_rect(int x, int y, int w, int h, uint32_t color);

/* Serialisable snapshot used by syscalls and the shell (no pointers). */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t bytes_per_pixel;
    uint32_t size;
    uint32_t format;
    uint32_t red_offset, red_size;
    uint32_t green_offset, green_size;
    uint32_t blue_offset, blue_size;
    uint32_t mode_active;
} framebuffer_report_t;

void framebuffer_get_report(framebuffer_report_t* out);

#endif /* FRAMEBUFFER_H */
