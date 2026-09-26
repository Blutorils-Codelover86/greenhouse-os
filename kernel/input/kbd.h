/* ==============================================================================
 * Greenhouse OS - Phase 7: PS/2 Keyboard Driver
 *
 * Hardware specific code only: the IRQ 1 handler reads port 0x60, tracks the
 * E0 extended prefix and the modifier keys, and turns every make/break code
 * into an input event.  It contains no shell, GUI or window manager logic.
 * ==============================================================================
 */

#ifndef KBD_H
#define KBD_H

#include <stdint.h>

#define KBD_IRQ 1
#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

void kbd_init(void);
int  kbd_flush_buffer(void);
int  kbd_is_shift_down(void);
int  kbd_is_ctrl_down(void);
int  kbd_is_alt_down(void);
int  kbd_is_caps_lock(void);

uint32_t kbd_get_scancode_count(void);
uint32_t kbd_get_overrun_count(void);

#endif /* KBD_H */
