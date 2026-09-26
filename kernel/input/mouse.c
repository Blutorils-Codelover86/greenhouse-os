/* ==============================================================================
 * Greenhouse OS - Phase 7: PS/2 Mouse Driver (implementation)
 * ==============================================================================
 */

#include "mouse.h"
#include "input.h"
#include "../io.h"
#include "../irq.h"

#define MOUSE_ACK          0xFA
#define MOUSE_SELFTEST_OK  0xAA

static int      mouse_present = 0;
static int      mouse_enabled = 0;
static int      mouse_device_id = -1;
static int      mouse_buttons = 0;
static uint32_t mouse_packets = 0;
static uint32_t mouse_dropped = 0;
static uint32_t mouse_wheels = 0;

/* Packet assembly */
static uint8_t  packet[4];
static int      packet_index = 0;

/* ------------------------------------------------------------------------------
 * Controller / device command plumbing
 * -------------------------------------------------------------------------- */

static int mouse_wait_write(void) {
    for (int spin = 0; spin < 100000; spin++) {
        if (!(inb(MOUSE_CNTRL_PORT) & 0x02)) return 0;
    }
    return -1;    /* controller never became writable */
}

static int mouse_wait_read(void) {
    for (int spin = 0; spin < 100000; spin++) {
        if (inb(MOUSE_STATUS_PORT) & 0x01) return 0;
    }
    return -1;
}

/* The controller has two ports: 0x64 takes commands, 0x60 takes payload bytes.
 * Sending a payload to 0x64 by mistake is interpreted as a controller command
 * (QEMU even turns 0xF4/0xF6 into a CPU reset pulse), so keep them apart. */
static int mouse_write_command(uint8_t cmd) {
    if (mouse_wait_write() != 0) return -1;
    outb(MOUSE_CNTRL_PORT, cmd);
    return 0;
}

static int mouse_write_data(uint8_t value) {
    if (mouse_wait_write() != 0) return -1;
    outb(MOUSE_DATA_PORT, value);
    return 0;
}

static int mouse_read_ack(void) {
    if (mouse_wait_read() != 0) return -1;
    uint8_t ack = inb(MOUSE_DATA_PORT);
    if (ack == MOUSE_ACK) return 0;

    /* 0xAA is a self test result, 0xFE is a resend request. */
    if (ack == 0xFE) return -2;
    if (ack == MOUSE_SELFTEST_OK) return 1;
    return -3;
}

static int mouse_send_device(uint8_t cmd) {
    /* 0xFE from the device means "resend", so retry a bounded number of times. */
    for (int attempt = 0; attempt < 4; attempt++) {
        /* 0xD4 prefix: the next payload byte is for the auxiliary device. */
        if (mouse_write_command(0xD4) != 0) return -1;
        if (mouse_write_data(cmd) != 0) return -1;

        int rc = mouse_read_ack();
        if (rc == -2) continue;      /* resend requested */
        if (rc < 0) return rc;
        return 0;                    /* 0xFA ack, or a self-test result */
    }
    return -1;
}

/* Drain anything the device queued before we started decoding. */
static void mouse_drain_queue(void) {
    for (int i = 0; i < 64; i++) {
        if (!(inb(MOUSE_STATUS_PORT) & 0x01)) break;
        (void)inb(MOUSE_DATA_PORT);
    }
}

/* ------------------------------------------------------------------------------
 * Packet decoding
 *
 * A classic packet is three bytes; QEMU and Bochs append a fourth byte when
 * the emulated device reports a wheel.  The two cases are told apart by bit 3:
 * it is always set on the first byte of a packet and always clear on the wheel
 * byte, so a wheel byte is consumed as such whenever one follows a packet.
 * -------------------------------------------------------------------------- */

static int mouse_fourth_pending = 0;

static void mouse_apply_wheel_byte(uint8_t byte) {
    if (!(byte & 0x80)) return;      /* only when a wheel bit is present */
    int wheel = (byte & 0x10) ? -1 : 1;
    mouse_wheels++;
    input_post_mouse_wheel(wheel);
}

static void mouse_decode_packet(void) {
    uint8_t flags = packet[0];
    mouse_packets++;

    /* Bit 6 flags X overflow, bit 7 Y overflow. Either one means the delta
     * bytes cannot be trusted. */
    if (flags & (MOUSE_FLAG_OVERFLOW | MOUSE_FLAG_XY7)) {
        /* Movement cannot be trusted: drop the packet but keep the button set. */
        mouse_dropped++;
        return;
    }

    int dx = (int)(int8_t)packet[1];
    /* The PS/2 Y axis points up, screen coordinates point down, so the raw
     * byte has to be negated. Without this every downward move arrives as a
     * negative delta and the pointer sticks to the top of the screen. */
    int dy = -(int)(int8_t)packet[2];

    int new_buttons = 0;
    if (flags & MOUSE_FLAG_LEFT)   new_buttons |= INPUT_MOUSE_LEFT;
    if (flags & MOUSE_FLAG_RIGHT)  new_buttons |= INPUT_MOUSE_RIGHT;
    if (flags & MOUSE_FLAG_MIDDLE) new_buttons |= INPUT_MOUSE_MIDDLE;

    int changed = new_buttons ^ mouse_buttons;

    if (changed) {
        /* Report the first changed button; further ones arrive in later packets. */
        for (int bit = 0; bit < 3; bit++) {
            int mask = 1 << bit;
            if (!(changed & mask)) continue;
            input_post_mouse_button((uint16_t)new_buttons, (new_buttons & mask) ? 1 : 0, dx, dy);
            break;
        }
        mouse_buttons = new_buttons;
    } else {
        input_post_mouse_move(dx, dy);
    }
}

static void mouse_irq_handler(irq_registers_t* regs) {
    (void)regs;

    uint8_t status = inb(MOUSE_STATUS_PORT);
    if (!(status & 0x01)) {          /* spurious IRQ, nothing to read */
        pic_send_eoi(MOUSE_IRQ);
        return;
    }

    uint8_t byte = inb(MOUSE_DATA_PORT);

    /* A fourth (wheel) byte directly following a packet? */
    if (mouse_fourth_pending) {
        mouse_fourth_pending = 0;
        if (!(byte & 0x08)) {
            mouse_apply_wheel_byte(byte);
            pic_send_eoi(MOUSE_IRQ);
            return;
        }
        /* Otherwise it already was the first byte of the next packet. */
    }

    if (packet_index == 0) {
        /* The first byte of a packet must have bit 3 set; anything else is
         * noise or a resynchronisation artefact. */
        if (!(byte & 0x08)) {
            pic_send_eoi(MOUSE_IRQ);
            return;
        }
    }

    packet[packet_index++] = byte;
    if (packet_index == 3) {
        mouse_decode_packet();
        packet_index = 0;
        mouse_fourth_pending = 1;
    }

    pic_send_eoi(MOUSE_IRQ);
}

/* ------------------------------------------------------------------------------
 * Public interface
 * -------------------------------------------------------------------------- */

int mouse_init(void) {
    mouse_present = 0;
    mouse_enabled = 0;
    mouse_device_id = -1;
    mouse_buttons = 0;
    mouse_packets = 0;
    mouse_dropped = 0;
    mouse_wheels = 0;
    packet_index = 0;
    mouse_fourth_pending = 0;

    /* 1. enable the auxiliary port on the controller */
    if (mouse_write_command(0xA8) != 0) return -2;

    mouse_drain_queue();

    /* 2. tell the mouse to use the controller, not its own IRQ */
    if (mouse_write_command(0x20) != 0) return -3;
    if (mouse_wait_read() != 0) return -4;
    uint8_t config = inb(MOUSE_DATA_PORT);
    config |= 0x02;                 /* enable aux interrupt */
    config &= (uint8_t)~0x20;       /* clear "enable aux clock" bit (irrelevant here) */
    if (mouse_write_command(0x60) != 0) return -5;
    if (mouse_write_data(config) != 0) return -6;
    /* The controller does not acknowledge a config byte write; it only updates
     * its IRQ output, so nothing is queued for us to read back here. */

    /* 3. defaults + enable data reporting */
    if (mouse_send_device(MOUSE_CMD_DEFAULTS) < 0) return -7;
    if (mouse_send_device(MOUSE_CMD_ENABLE_REPORTING) < 0) return -8;

    mouse_present = 1;
    mouse_enabled = 1;

    /* 4. try to learn the device id (0x00 = PS/2, 0x03 = scroll wheel) */
    if (mouse_send_device(MOUSE_CMD_READ_ID) == 0) {
        if (mouse_wait_read() == 0) mouse_device_id = (int)inb(MOUSE_DATA_PORT);
    }

    mouse_drain_queue();
    packet_index = 0;
    mouse_fourth_pending = 0;

    irq_install_handler(MOUSE_IRQ, mouse_irq_handler);
    return 0;
}

int mouse_is_present(void)   { return mouse_present; }
int mouse_is_enabled(void)  { return mouse_enabled; }
int mouse_get_device_id(void) { return mouse_device_id; }
uint32_t mouse_get_packet_count(void)  { return mouse_packets; }
uint32_t mouse_get_dropped_count(void) { return mouse_dropped; }
uint32_t mouse_get_wheel_count(void)   { return mouse_wheels; }
uint32_t mouse_get_buttons(void)       { return (uint32_t)mouse_buttons; }

const char* mouse_get_name(void) {
    if (!mouse_present) return "absent";
    if (mouse_device_id == 3) return "PS/2 wheel mouse";
    if (mouse_device_id == 0) return "PS/2 mouse";
    return "PS/2 mouse (id unknown)";
}
