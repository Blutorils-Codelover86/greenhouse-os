/* ==============================================================================
 * Greenhouse OS - Phase 7: PS/2 Mouse Driver
 *
 * Owns the auxiliary port: enable it, switch it to byte packets with data
 * reporting, and decode IRQ 12 traffic into input events.  Handles the classic
 * 3 byte packet, the QEMU/Bochs 4 byte variant (4th wheel packet) and packet
 * overflow.  No window manager or cursor knowledge lives here.
 * ==============================================================================
 */

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

#define MOUSE_IRQ 12

#define MOUSE_STATUS_PORT   0x64
#define MOUSE_DATA_PORT     0x60
#define MOUSE_CNTRL_PORT    0x64

/* Auxiliary commands */
#define MOUSE_CMD_RESET              0xFF
#define MOUSE_CMD_DEFAULTS           0xF6
#define MOUSE_CMD_ENABLE_REPORTING   0xF4
#define MOUSE_CMD_DISABLE_REPORTING  0xF5
#define MOUSE_CMD_READ_ID            0xF2

/* Packet flags */
#define MOUSE_FLAG_LEFT     0x01
#define MOUSE_FLAG_RIGHT    0x02
#define MOUSE_FLAG_MIDDLE   0x04
#define MOUSE_FLAG_X_SIGN   0x10
#define MOUSE_FLAG_Y_SIGN   0x20
#define MOUSE_FLAG_OVERFLOW 0x40
#define MOUSE_FLAG_XY7      0x80

int  mouse_init(void);
int  mouse_is_present(void);
int  mouse_is_enabled(void);

uint32_t mouse_get_packet_count(void);
uint32_t mouse_get_dropped_count(void);
uint32_t mouse_get_wheel_count(void);
uint32_t mouse_get_buttons(void);
int      mouse_get_device_id(void);
const char* mouse_get_name(void);

#endif /* MOUSE_H */
