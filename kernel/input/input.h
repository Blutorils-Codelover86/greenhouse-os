/* ==============================================================================
 * Greenhouse OS - Phase 7: Input Subsystem
 *
 * Hardware drivers (PS/2 keyboard, PS/2 mouse) do not know anything about the
 * window manager or any GUI application: they translate their hardware
 * specific traffic into input events and push them into this single queue.
 * Consumers (the text shell, the window manager, userland applications)
 * pull events from the queue, which decouples drivers from consumers.
 *
 *   keyboard/mouse IRQ  ->  driver ISR  ->  input_post_event()  ->  queue
 *                                                          |
 *                          text shell / window manager / userland pop events
 * ==============================================================================
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stddef.h>

#define INPUT_QUEUE_SIZE     256
#define INPUT_QUEUE_MASK     (INPUT_QUEUE_SIZE - 1)

/* PS/2 set-1 scancodes for keys the GUI treats as special */
#define KBD_SCAN_ESC          0x01
#define KBD_SCAN_ENTER        0x1C
#define KBD_SCAN_LCTRL        0x1D
#define KBD_SCAN_LSHIFT       0x2A
#define KBD_SCAN_RSHIFT       0x36
#define KBD_SCAN_CAPSLOCK     0x3A
#define KBD_SCAN_F1           0x3B
#define KBD_SCAN_F10          0x44
#define KBD_SCAN_UP           0x48
#define KBD_SCAN_LEFT         0x4B
#define KBD_SCAN_RIGHT        0x4D
#define KBD_SCAN_DOWN         0x50
#define KBD_SCAN_DELETE       0x53
#define KBD_SCAN_LGUI         0x5B   /* Left Windows / Super key (Seed Key) */
#define KBD_SCAN_RGUI         0x5C   /* Right Windows / Super key (Seed Key) */
#define KBD_SCAN_SEED         0x5B   /* Greenhouse Seed Key */

/* Legacy codes returned by input_getchar() for the text shell */
#define INPUT_KEY_UP          0x81
#define INPUT_KEY_DOWN        0x82
#define INPUT_KEY_LEFT        0x83
#define INPUT_KEY_RIGHT       0x84
#define INPUT_KEY_ESC         0x85
#define INPUT_KEY_ENTER       0x86
#define INPUT_KEY_BACKSPACE   0x08
#define INPUT_KEY_TAB         0x09
#define INPUT_KEY_DELETE      0x87
#define INPUT_KEY_SEED        0x88   /* Greenhouse Seed Key */

/* Modifier bits reported with every event */
#define INPUT_MOD_SHIFT       0x0001
#define INPUT_MOD_CTRL        0x0002
#define INPUT_MOD_ALT         0x0004
#define INPUT_MOD_CAPS        0x0008
#define INPUT_MOD_SEED        0x0010 /* Seed / Windows / Super Modifier */
#define INPUT_MOD_GUI         INPUT_MOD_SEED

/* Mouse buttons */
#define INPUT_MOUSE_LEFT      0x01
#define INPUT_MOUSE_RIGHT     0x02
#define INPUT_MOUSE_MIDDLE    0x04

typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_KEY_DOWN,
    INPUT_EVENT_KEY_UP,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_BUTTON_DOWN,
    INPUT_EVENT_MOUSE_BUTTON_UP,
    INPUT_EVENT_MOUSE_WHEEL
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    uint8_t  scancode;      /* PS/2 set 1 make code (keyboard events)          */
    uint8_t  ascii;         /* translated character, 0 when not printable      */
    uint8_t  extended;      /* 1 when the key came with the 0xE0 prefix        */
    uint16_t modifiers;     /* INPUT_MOD_*                                      */
    uint16_t buttons;       /* mouse button state at the time of the event     */
    int32_t  x, y;          /* absolute pointer position                       */
    int32_t  dx, dy;        /* relative movement (wheel uses dy)               */
    uint64_t timestamp;     /* kernel PIT ticks                                */
} input_event_t;

typedef struct {
    uint32_t posted;
    uint32_t delivered;
    uint32_t dropped;       /* queue overflow                                  */
    uint32_t key_down;
    uint32_t key_up;
    uint32_t mouse_move;
    uint32_t mouse_buttons;
    uint32_t pointer_x;
    uint32_t pointer_y;
} input_stats_t;

void  input_init(void);

/* Producers (hardware drivers).  Never blocks, never drops silently. */
int   input_post_event(const input_event_t* ev);
int   input_post_key(uint8_t scancode, int pressed, uint8_t ascii, int extended, uint16_t modifiers);
int   input_post_mouse_move(int dx, int dy);
int   input_post_mouse_button(uint16_t buttons, int pressed, int dx, int dy);
int   input_post_mouse_wheel(int dz);

/* Consumers */
int   input_poll_event(input_event_t* out);          /* 1 = event, 0 = empty  */
int   input_wait_event(input_event_t* out, int max_wait_ticks);
int   input_event_available(void);
void  input_flush(void);

/* Pointer state - maintained by the subsystem, bounded by the display. */
void  input_set_pointer_bounds(int width, int height);
void  input_get_pointer(int* x, int* y);
int   input_set_pointer(int x, int y);
int   input_get_buttons(void);

/* Text console helper shared by the shell and the userland read path: blocks
 * (idle) until a printable key, a newline or a special navigation key. */
uint8_t input_getchar(void);

uint16_t input_get_modifiers(void);
void     input_get_stats(input_stats_t* out);

#endif /* INPUT_H */
