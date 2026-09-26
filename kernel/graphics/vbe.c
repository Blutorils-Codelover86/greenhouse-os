/* ==============================================================================
 * Greenhouse OS - Phase 6: Video Mode Driver (implementation)
 * ==============================================================================
 */

#include "vbe.h"
#include "../io.h"

/* ------------------------------------------------------------------------------
 * PCI configuration space access (mechanism 1)
 * -------------------------------------------------------------------------- */

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static uint32_t pci_config_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t offset) {
    uint32_t addr = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) |
                    ((uint32_t)fn << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_config_read16(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t offset) {
    uint32_t v = pci_config_read32(bus, dev, fn, offset);
    return (uint16_t)((v >> ((offset & 2) * 8)) & 0xFFFF);
}

static void pci_config_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t offset, uint32_t value) {
    uint32_t addr = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) |
                    ((uint32_t)fn << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, value);
}

/* Size of a 32 bit memory BAR, probed the standard way: write all ones, read
 * back the mask, then restore the original value.  Returns 0 when the probe is
 * not applicable (64 bit BAR or read only header). */
static uint32_t pci_bar32_size(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t offset, uint32_t original) {
    if (original == 0) return 0;

    pci_config_write32(bus, dev, fn, offset, 0xFFFFFFFFu);
    uint32_t mask = pci_config_read32(bus, dev, fn, offset);
    pci_config_write32(bus, dev, fn, offset, original);

    if (!(mask & 0xFFFF0000u)) return 0;      /* no size bits: not probeable */

    uint32_t bits = 0;
    while (mask & 0x80000000u) { mask <<= 1; bits++; }
    return 1u << (32 - bits);
}

/* ------------------------------------------------------------------------------
 * DISPI register access
 * -------------------------------------------------------------------------- */

void vbe_write_register(uint8_t index, uint16_t value) {
    outw(VBE_INDEX_PORT, index);
    outw(VBE_DATA_PORT, value);
}

uint32_t vbe_read_register(uint8_t index) {
    outw(VBE_INDEX_PORT, index);
    return (uint32_t)inw(VBE_DATA_PORT);
}

/* ------------------------------------------------------------------------------
 * Adapter discovery
 * -------------------------------------------------------------------------- */

static int vbe_dispi_present(void) {
    /* The ID register only accepts the known DISPI ids; writing the highest one
     * and reading it back is the documented way to detect the register set. */
    vbe_write_register(VBE_REG_ID, VBE_ID_BOCHS);
    return (vbe_read_register(VBE_REG_ID) == VBE_ID_BOCHS);
}

void vbe_probe(vbe_adapter_t* out) {
    if (!out) return;

    out->detected = 0;
    out->dispi = 0;
    out->id_value = 0;
    out->vendor_id = 0;
    out->device_id = 0;
    out->bar0 = 0;
    out->bar0_size = 0;
    out->bar0_is_64bit = 0;
    out->bus = out->device = out->function = 0;

    /* The standard VGA adapter of the PC machine sits on bus 0; scan all
     * functions of every slot so other placements still work. */
    for (int dev = 0; dev < 32 && !out->detected; dev++) {
        for (int fn = 0; fn < 8; fn++) {
            uint32_t id = pci_config_read32(0, (uint8_t)dev, (uint8_t)fn, 0x00);
            if (id == 0xFFFFFFFFu) break;                 /* no device in this slot */
            if ((id & 0xFFFF) == 0xFFFF) continue;        /* absent */

            uint16_t class_reg = pci_config_read16(0, (uint8_t)dev, (uint8_t)fn, 0x0A);
            uint8_t class_code = (uint8_t)(class_reg >> 8);
            uint8_t subclass    = (uint8_t)(class_reg & 0xFF);
            if (class_code != 0x03) {                     /* display controller */
                if (!(class_code == 0x00 && fn == 0)) continue;
            }

            uint32_t bar0_raw = pci_config_read32(0, (uint8_t)dev, (uint8_t)fn, 0x10);
            if (bar0_raw & 0x01) continue;                /* I/O space BAR */

            uint32_t type = (bar0_raw >> 1) & 0x03;
            uint64_t bar = bar0_raw & ~0x0Fu;
            uint32_t size = 0;

            if (type == 0x02) {                           /* 64 bit memory BAR */
                uint32_t bar1_raw = pci_config_read32(0, (uint8_t)dev, (uint8_t)fn, 0x14);
                if (bar1_raw & 0x01) continue;           /* low half is I/O: skip */
                bar |= ((uint64_t)(bar1_raw & ~0x0Fu)) << 32;
                out->bar0_is_64bit = 1;
            } else {
                size = pci_bar32_size(0, (uint8_t)dev, (uint8_t)fn, 0x10, bar0_raw);
            }

            if (bar == 0) continue;

            out->detected = 1;
            out->vendor_id = (uint16_t)(id & 0xFFFF);
            out->device_id = (uint16_t)(id >> 16);
            out->bus = 0;
            out->device = (uint8_t)dev;
            out->function = (uint8_t)fn;
            out->bar0 = bar;
            out->bar0_size = size;
            (void)subclass;

            /* Confirm the register set with the documented ID handshake. */
            out->dispi = vbe_dispi_present();
            out->id_value = (uint16_t)vbe_read_register(VBE_REG_ID);
            break;
        }
    }
}

/* ------------------------------------------------------------------------------
 * Mode programming
 * -------------------------------------------------------------------------- */

int vbe_set_graphics_mode(uint32_t width, uint32_t height, uint32_t bits_per_pixel,
                          vbe_mode_info_t* out_mode) {
    if (!out_mode) return -1;

    out_mode->ok = 0;
    out_mode->width = 0;
    out_mode->height = 0;
    out_mode->bits_per_pixel = 0;
    out_mode->bytes_per_pixel = 0;
    out_mode->stride = 0;
    out_mode->virt_width = 0;
    out_mode->fb_bytes = 0;
    out_mode->address = 0;

    vbe_adapter_t adapter;
    vbe_probe(&adapter);
    if (!adapter.detected) return -1;
    if (!adapter.dispi)   return -2;
    if (adapter.bar0 == 0) return -3;

    /* Leave any banked mode: the linear framebuffer is at BAR0. */
    vbe_write_register(VBE_REG_BANK, 0);
    vbe_write_register(VBE_REG_X_OFFSET, 0);
    vbe_write_register(VBE_REG_Y_OFFSET, 0);

    /* Enable first: the adapter only validates the geometry while the DISPI
     * interface is enabled.  NOCLEARMEM keeps a full framebuffer memset out of
     * the picture; the drawing code paints over the screen anyway. */
    vbe_write_register(VBE_REG_ENABLE,
                       VBE_ENABLE_ON | VBE_ENABLE_LFB | VBE_ENABLE_NOCLEARMEM);

    vbe_write_register(VBE_REG_XRES, (uint16_t)width);
    vbe_write_register(VBE_REG_YRES, (uint16_t)height);
    vbe_write_register(VBE_REG_BPP, (uint16_t)bits_per_pixel);
    vbe_write_register(VBE_REG_VIRT_WIDTH, (uint16_t)width);

    /* Read the mode back: the adapter is the only authority on what it set up
     * (it may clamp the resolution to what the framebuffer can hold). */
    uint32_t rb_width  = vbe_read_register(VBE_REG_XRES);
    uint32_t rb_height = vbe_read_register(VBE_REG_YRES);
    uint32_t rb_bpp    = vbe_read_register(VBE_REG_BPP);
    uint32_t rb_virt   = vbe_read_register(VBE_REG_VIRT_WIDTH);
    uint32_t rb_enable = vbe_read_register(VBE_REG_ENABLE);
    uint32_t rb_memory = vbe_read_register(VBE_REG_VIDEO_MEMORY) * 64 * 1024;

    if (!(rb_enable & VBE_ENABLE_ON)) return -4;
    if (rb_width == 0 || rb_height == 0) return -5;

    uint32_t bytes_pp = (rb_bpp + 7) / 8;
    if (bytes_pp == 0) bytes_pp = 1;
    if (rb_virt < rb_width) rb_virt = rb_width;

    uint32_t stride = rb_virt * bytes_pp;

    out_mode->width = rb_width;
    out_mode->height = rb_height;
    out_mode->bits_per_pixel = rb_bpp;
    out_mode->bytes_per_pixel = bytes_pp;
    out_mode->stride = stride;
    out_mode->virt_width = rb_virt;
    out_mode->fb_bytes = rb_memory;
    out_mode->address = adapter.bar0;

    if (rb_memory && (uint64_t)stride * rb_height > rb_memory) return -6;  /* clipped */
    if (out_mode->address == 0) return -7;

    out_mode->ok = 1;
    return 0;
}

/* ------------------------------------------------------------------------------
 * Restore the 80x25 VGA text mode used by the Greenhouse shell
 * -------------------------------------------------------------------------- */

static const uint8_t crtc_80x25_text[40] = {
    0x67, 0x03, 0x04, 0x82, 0x55, 0x81, 0xFB, 0x80,
    0x0F, 0x20, 0x00, 0x0F, 0x28, 0x1E, 0x96, 0xB9,
    0xA3, 0x8F, 0x2E, 0xBF, 0x1E, 0x28, 0x2B, 0x96,
    0xB7, 0x24, 0xB0, 0x28, 0x2F, 0x96, 0xB7, 0xCF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void vbe_save_text_state(vga_text_state_t* out) {
    if (!out) return;

    /* MISC bit 0 selects the active CRTC port (0x3D4 colour, 0x3B4 mono).
     * A graphics mode may have left it cleared, which would make every 0x3D4
     * access a no-op, so force colour decode for the duration of the read. */
    uint8_t misc = inb(0x3CC);
    outb(0x3C2, (uint8_t)(misc | 0x01));
    out->misc = misc;

    for (int i = 0; i < VGA_SEQ_REGS; i++) {
        outb(0x3C4, (uint8_t)i);
        out->seq[i] = inb(0x3C5);
    }
    for (int i = 0; i < VGA_CRTC_REGS; i++) {
        outb(0x3D4, (uint8_t)i);
        out->crtc[i] = inb(0x3D5);
    }
    for (int i = 0; i < VGA_GCTL_REGS; i++) {
        outb(0x3CE, (uint8_t)i);
        out->gctl[i] = inb(0x3CF);
    }

    /* Attribute controller: 0x3C0 and 0x3C1 are the same register.  A flip-flop
     * decides what the next access means, and only reading the input status
     * port 0x3DA is guaranteed to clear it - reading 0x3C1 is not, so the
     * status port has to be polled before every index write.  Without that the
     * writes alternate between index and data and quietly wreck the palette.
     * With the flip-flop clear, reading 0x3C0 returns the index latch itself,
     * whose bit 5 is what tells the adapter the display is live at all. */
    inb(0x3DA);
    out->attr_index = inb(0x3C0);
    for (int i = 0; i < VGA_ATTR_REGS; i++) {
        inb(0x3DA);                    /* index write follows a cleared flop */
        outb(0x3C0, (uint8_t)i);
        out->attr[i] = inb(0x3C1);
    }
    inb(0x3DA);
    outb(0x3C0, out->attr_index);  /* put the index latch back, flip-flop clear */
    outb(0x3C2, misc);              /* give the adapter its original MISC back */
}

int vbe_restore_text_state(const vga_text_state_t* in) {
    if (!in) return -1;

    /* Stop the linear framebuffer first: from here on the register file alone
     * decides what the display shows. */
    vbe_write_register(VBE_REG_ENABLE, VBE_ENABLE_OFF);
    vbe_write_register(VBE_REG_BANK, 0);

    /* Same CRTC port decode problem as in the save path: bring the colour
     * decode back before touching any register, the real MISC is written last. */
    outb(0x3C2, (uint8_t)(in->misc | 0x01));

    for (int i = 0; i < VGA_SEQ_REGS; i++) {
        outb(0x3C4, (uint8_t)i);
        outb(0x3C5, in->seq[i]);
    }

    /* CRTC index 0x11 bit 7 is the "CR0-CR7 write lock".  A graphics mode
     * normally leaves it set, and while it is set the horizontal timing
     * registers cannot be written at all - which is exactly what has to change
     * when coming back from a 1024 pixel wide mode.  Clear it for the duration
     * of the restore and put the original value back afterwards. */
    outb(0x3D4, 0x11);
    outb(0x3D5, (uint8_t)(in->crtc[0x11] & 0x7F));
    for (int i = 0; i < VGA_CRTC_REGS; i++) {
        outb(0x3D4, (uint8_t)i);
        outb(0x3D5, in->crtc[i]);
    }

    for (int i = 0; i < VGA_GCTL_REGS; i++) {
        outb(0x3CE, (uint8_t)i);
        outb(0x3CF, in->gctl[i]);
    }

    /* Attribute data has to go out through 0x3C0 as well: a write there is an
     * index write while the flip-flop is clear and a data write while it is
     * set.  Leaving the index latch at the saved value matters more than it
     * looks - its bit 5 is what tells the adapter whether the display is in
     * text or graphics mode, and clearing it blanks the screen entirely. */
    inb(0x3DA);
    for (int i = 0; i < VGA_ATTR_REGS; i++) {
        inb(0x3DA);                    /* index write follows a cleared flop */
        outb(0x3C0, (uint8_t)i);
        outb(0x3C0, in->attr[i]);
    }
    inb(0x3DA);
    outb(0x3C0, in->attr_index);

    outb(0x3C2, in->misc); /* MISC output last: it resets several controllers */
    return 0;
}

int vbe_restore_text_mode(void) {
    /* 1. Ask the DISPI interface to switch back: this puts the adapter back
     *    into its own 80x25 text mode and stops any linear graphics mode. */
    vbe_write_register(VBE_REG_ENABLE, VBE_ENABLE_OFF);
    vbe_write_register(VBE_REG_BANK, 0);

    /* 2. Program the classic VGA text mode explicitly as well, so the restore
     *    also works on adapters without the DISPI register set. */
    inb(0x3DA);  /* reset the attribute controller flip-flop */

    /* Attribute controller: 16 colour palette, 8 bit colour output. */
    outb(0x3C0, 0x10);
    for (int i = 0; i < 16; i++) {
        outb(0x3C0, (uint8_t)(0x11 + i));
        outb(0x3C1, (uint8_t)i);
    }
    outb(0x3C0, 0x01);
    outb(0x3C1, 0x00);

    /* Misc output: 80x25 colour text. */
    outb(0x3C4, 0x00);
    outb(0x3C5, 0x67);

    /* CRTC: text mode timing and a visible cursor. */
    for (int i = 0; i < 40; i++) {
        outb(0x3D4, (uint8_t)i);
        outb(0x3D5, crtc_80x25_text[i]);
    }
    inb(0x3DA);

    /* Enable video generation. */
    outb(0x3C4, 0x01);
    outb(0x3C5, (uint8_t)(inb(0x3C5) | 0x01));

    return 0;
}
