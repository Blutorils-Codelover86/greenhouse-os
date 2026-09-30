/* ==============================================================================
 * Greenhouse OS - Phase 7: PS/2 Keyboard Driver (implementation)
 * ==============================================================================
 */

#include "kbd.h"
#include "input.h"
#include "../io.h"
#include "../irq.h"

static int      kbd_extended = 0;
static int      kbd_shift = 0;
static int      kbd_ctrl = 0;
static int      kbd_alt = 0;
static int      kbd_caps = 0;
static int      kbd_seed = 0;
static uint32_t kbd_scancodes = 0;
static uint32_t kbd_overruns = 0;

/* Set 1 scancode set, unshifted and shifted.  Index = make code. */
static const char kbd_map_lower[128] = {
    0,    27,   '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  '-',  '=',  INPUT_KEY_BACKSPACE,
    INPUT_KEY_TAB, 'q', 'w',  'e',  'r',  't',  'y',  'u',  'i',  'o',  'p',  '[',  ']',  '\n',
    0,    'a',  's',  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  '\'', '`',
    0,    '\\', 'z',  'x',  'c',  'v',  'b',  'n',  'm',  ',',  '.',  '/',  0,
    '*',  0,    ' '
};

static const char kbd_map_upper[128] = {
    0,    27,   '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',  '_',  '+',  INPUT_KEY_BACKSPACE,
    INPUT_KEY_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,    'A',  'S',  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  '"',  '~',
    0,    '|',  'Z',  'X',  'C',  'V',  'B',  'N',  'M',  '<',  '>',  '?',  0,
    '*',  0,    ' '
};

static uint16_t kbd_current_modifiers(void) {
    uint16_t mods = 0;
    if (kbd_shift) mods |= INPUT_MOD_SHIFT;
    if (kbd_ctrl)  mods |= INPUT_MOD_CTRL;
    if (kbd_alt)   mods |= INPUT_MOD_ALT;
    if (kbd_caps)  mods |= INPUT_MOD_CAPS;
    if (kbd_seed)  mods |= INPUT_MOD_SEED;
    return mods;
}

static uint8_t kbd_translate(uint8_t make, int extended, int shift) {
    if (extended) {
        switch (make) {
            case KBD_SCAN_UP:     return INPUT_KEY_UP;
            case KBD_SCAN_DOWN:   return INPUT_KEY_DOWN;
            case KBD_SCAN_LEFT:   return INPUT_KEY_LEFT;
            case KBD_SCAN_RIGHT:  return INPUT_KEY_RIGHT;
            case KBD_SCAN_DELETE: return INPUT_KEY_DELETE;
            case KBD_SCAN_LGUI:
            case KBD_SCAN_RGUI:   return INPUT_KEY_SEED;
            default:              return 0;
        }
    }

    if (make == KBD_SCAN_ESC) return 27;
    if (make >= 128) return 0;

    if (kbd_map_lower[make] == 0) return 0;

    return shift ? (uint8_t)kbd_map_upper[make] : (uint8_t)kbd_map_lower[make];
}

static void kbd_post(uint8_t scancode, int pressed) {
    uint8_t make = (uint8_t)(scancode & 0x7F);
    uint16_t mods = kbd_current_modifiers();
    int shift = (mods & (INPUT_MOD_SHIFT | INPUT_MOD_CAPS)) ? 1 : 0;
    uint8_t ascii = pressed ? kbd_translate(make, kbd_extended, shift) : 0;

    input_event_t ev;
    ev.type = pressed ? INPUT_EVENT_KEY_DOWN : INPUT_EVENT_KEY_UP;
    ev.scancode = make;
    ev.ascii = ascii;
    ev.extended = (uint8_t)(kbd_extended ? 1 : 0);
    ev.modifiers = mods;
    ev.buttons = (uint16_t)input_get_buttons();
    ev.dx = 0;
    ev.dy = 0;
    input_get_pointer(&ev.x, &ev.y);
    ev.timestamp = 0;

    input_post_event(&ev);
}

static void kbd_irq_handler(irq_registers_t* regs) {
    (void)regs;

    uint8_t status = inb(KBD_STATUS_PORT);
    if (!(status & 0x01)) {              /* nothing waiting */
        kbd_overruns++;
        pic_send_eoi(KBD_IRQ);
        return;
    }

    uint8_t scancode = inb(KBD_DATA_PORT);
    kbd_scancodes++;

    if (scancode == 0xE0) {
        kbd_extended = 1;
        pic_send_eoi(KBD_IRQ);
        return;
    }

    int pressed = (scancode & 0x80) == 0;
    uint8_t make = (uint8_t)(scancode & 0x7F);

    if (make == 0x2A || make == 0x36) {
        kbd_shift = pressed;
    } else if (make == 0x1D) {
        kbd_ctrl = pressed;
    } else if (make == 0x38) {
        kbd_alt = pressed;
    } else if (!kbd_extended && make == KBD_SCAN_CAPSLOCK) {
        if (pressed) kbd_caps = !kbd_caps;
    } else if (kbd_extended && (make == KBD_SCAN_LGUI || make == KBD_SCAN_RGUI)) {
        kbd_seed = pressed;
    }

    kbd_post(scancode, pressed);
    kbd_extended = 0;

    pic_send_eoi(KBD_IRQ);
}

void kbd_init(void) {
    kbd_extended = 0;
    kbd_shift = kbd_ctrl = kbd_alt = 0;
    kbd_caps = 0;
    kbd_seed = 0;
    kbd_scancodes = 0;
    kbd_overruns = 0;

    kbd_flush_buffer();
    irq_install_handler(KBD_IRQ, kbd_irq_handler);
}

int kbd_flush_buffer(void) {
    int drained = 0;
    for (int i = 0; i < 64; i++) {
        if (!(inb(KBD_STATUS_PORT) & 0x01)) break;
        (void)inb(KBD_DATA_PORT);
        drained++;
    }
    return drained;
}

int kbd_is_shift_down(void) { return kbd_shift; }
int kbd_is_ctrl_down(void)  { return kbd_ctrl; }
int kbd_is_alt_down(void)   { return kbd_alt; }
int kbd_is_caps_lock(void)  { return kbd_caps; }
uint32_t kbd_get_scancode_count(void) { return kbd_scancodes; }
uint32_t kbd_get_overrun_count(void)  { return kbd_overruns; }
