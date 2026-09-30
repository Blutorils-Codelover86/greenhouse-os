/* ==============================================================================
 * Greenhouse OS - Phase 6: Video Mode Driver (Bochs VBE / DISPI registers)
 *
 * All hardware specific video mode programming lives here.  The driver talks
 * to the "Bochs VBE" display interface that QEMU's standard VGA adapter
 * implements (hw/display/vga.c: vbe_ioport_*), which is also understood by
 * Bochs and by a number of other VBE compatible adapters:
 *
 *      0x1CE  index port (8 bit)
 *      0x1CF  data port  (16 bit)
 *
 * Register indices (include/hw/display/bochs-vbe.h in QEMU):
 *
 *      0x0  ID            0xB0C5 = VBE 2.0+ (write, then read back to confirm)
 *      0x1  XRES          horizontal resolution in pixels
 *      0x2  YRES          vertical resolution in pixels
 *      0x3  BPP           bits per pixel (4/8/16/24/32; 15 is clamped to 16)
 *      0x4  ENABLE        bit0 enable, bit1 getcaps, bit5 8 bit DAC,
 *                         bit6 linear framebuffer, bit7 do not clear memory
 *      0x5  BANK          bank switch (0 = linear framebuffer)
 *      0x6  VIRT_WIDTH    virtual width in pixels, the stride follows it
 *      0x7  VIRT_HEIGHT   read only: maximum height for the current width
 *      0x8  X_OFFSET      pan offset, 0 here
 *      0x9  Y_OFFSET      pan offset, 0 here
 *      0xA  VIDEO_MEMORY_64K  read only: framebuffer size in 64 KiB units
 *
 * There is no stride register: the adapter derives the line length from
 * VIRT_WIDTH and BPP.  The linear framebuffer address is read from BAR0 of the
 * PCI configuration space.  Nothing about the active mode is assumed - the
 * resolution, depth and stride are all read back after programming.
 *
 * Leaving graphics mode (ENABLE = 0) restores the adapter's own text mode, and
 * the classic 80x25 VGA programming is repeated so the Greenhouse text shell
 * works on adapters without the DISPI register set as well.
 * ==============================================================================
 */

#ifndef VBE_H
#define VBE_H

#include <stdint.h>

/* Bochs VBE / DISPI index-data register pair */
#define VBE_INDEX_PORT   0x1CE
#define VBE_DATA_PORT    0x1CF

/* Register indices */
#define VBE_REG_ID            0x00
#define VBE_REG_XRES          0x01
#define VBE_REG_YRES          0x02
#define VBE_REG_BPP           0x03
#define VBE_REG_ENABLE        0x04
#define VBE_REG_BANK          0x05
#define VBE_REG_VIRT_WIDTH    0x06
#define VBE_REG_VIRT_HEIGHT   0x07
#define VBE_REG_X_OFFSET      0x08
#define VBE_REG_Y_OFFSET      0x09
#define VBE_REG_VIDEO_MEMORY  0x0A

/* ENABLE bits */
#define VBE_ENABLE_OFF         0x00
#define VBE_ENABLE_ON          0x01
#define VBE_ENABLE_GETCAPS     0x02
#define VBE_ENABLE_8BIT_DAC    0x20
#define VBE_ENABLE_LFB         0x40
#define VBE_ENABLE_NOCLEARMEM  0x80

/* VBE_DISPI_ID0 .. VBE_DISPI_ID5 */
#define VBE_ID_BOCHS           0xB0C5

/* Driver policy when the boot loader supplies no framebuffer.  Only the request
 * is a policy: the accepted mode is whatever the adapter reads back. */
#define VBE_PREFERRED_WIDTH    1024
#define VBE_PREFERRED_HEIGHT   768
#define VBE_PREFERRED_BPP      32

typedef struct {
    int      detected;       /* a PCI display adapter with a memory BAR0     */
    int      dispi;          /* the DISPI (Bochs VBE) register set answered   */
    uint16_t id_value;       /* ID register read back                        */
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  bus, device, function;
    uint64_t bar0;           /* linear framebuffer address from the PCI BAR  */
    uint32_t bar0_size;      /* advertised BAR size, in bytes                 */
    int      bar0_is_64bit;
} vbe_adapter_t;

/* Result of a mode switch: everything read back from the hardware. */
typedef struct {
    int      ok;
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    uint32_t bytes_per_pixel;
    uint32_t stride;         /* bytes per scanline, derived from VIRT_WIDTH  */
    uint32_t virt_width;     /* virtual width reported by the adapter        */
    uint32_t fb_bytes;       /* framebuffer size reported by the adapter     */
    uint64_t address;        /* linear framebuffer address (PCI BAR0)        */
} vbe_mode_info_t;

/* Snapshot of the VGA register file, used to put the adapter back exactly the
 * way the firmware/boot loader left it.  Turning the linear framebuffer off
 * does *not* restore the text timings: the CRTC keeps whatever the graphics
 * mode programmed, so the display comes back as a garbled 1024x768 "text"
 * screen.  Saving and replaying the real state is both shorter and safer than
 * hardcoding a register table. */
#define VGA_SEQ_REGS   8    /* 0x3C4/0x3C5: 0x00-0x07                     */
#define VGA_CRTC_REGS  32   /* 0x3D4/0x3D5: 0x00-0x1F                     */
#define VGA_GCTL_REGS  16   /* 0x3CE/0x3CF: 0x00-0x0F                     */
#define VGA_ATTR_REGS  32   /* 0x3C0/0x3C1: 0x00-0x1F                     */

typedef struct {
    uint8_t misc;                  /* 0x3CC read, 0x3C2 write               */
    uint8_t attr_index;            /* 0x3C0 index latch, see vbe.c          */
    uint8_t seq[VGA_SEQ_REGS];
    uint8_t crtc[VGA_CRTC_REGS];
    uint8_t gctl[VGA_GCTL_REGS];
    uint8_t attr[VGA_ATTR_REGS];
} vga_text_state_t;

void vbe_probe(vbe_adapter_t* out_adapter);
int  vbe_set_graphics_mode(uint32_t width, uint32_t height, uint32_t bits_per_pixel,
                           vbe_mode_info_t* out_mode);
int  vbe_restore_text_mode(void);
void vbe_save_text_state(vga_text_state_t* out);
int  vbe_restore_text_state(const vga_text_state_t* in);
void vbe_backup_font(void);
void vbe_restore_font(uint64_t bar0);

uint32_t vbe_read_register(uint8_t index);
void     vbe_write_register(uint8_t index, uint16_t value);

#endif /* VBE_H */

