/* ==============================================================================
 * Greenhouse OS - Phase 7: Input Subsystem (implementation)
 * ==============================================================================
 */

#include "input.h"
#include "../io.h"
#include "../irq.h"

static volatile input_event_t input_queue[INPUT_QUEUE_SIZE];
static volatile uint32_t input_head = 0;   /* producer index */
static volatile uint32_t input_tail = 0;   /* consumer index */

static volatile uint32_t input_posted = 0;
static volatile uint32_t input_delivered = 0;
static volatile uint32_t input_dropped = 0;
static volatile uint32_t input_key_down_count = 0;
static volatile uint32_t input_key_up_count = 0;
static volatile uint32_t input_mouse_move_count = 0;
static volatile uint32_t input_mouse_button_count = 0;

static int32_t  pointer_x = 0;
static int32_t  pointer_y = 0;
static int      pointer_max_x = 0;
static int      pointer_max_y = 0;
static uint16_t pointer_buttons = 0;
static uint16_t active_modifiers = 0;

void input_init(void) {
    input_head = 0;
    input_tail = 0;
    input_posted = input_delivered = input_dropped = 0;
    input_key_down_count = input_key_up_count = 0;
    input_mouse_move_count = input_mouse_button_count = 0;
    pointer_x = pointer_y = 0;
    pointer_buttons = 0;
    active_modifiers = 0;
    pointer_max_x = 0;
    pointer_max_y = 0;
}

int input_post_event(const input_event_t* ev) {
    if (!ev) return -1;

    uint32_t next = (input_head + 1) & INPUT_QUEUE_MASK;
    if (next == (uint32_t)input_tail) {
        input_dropped++;   /* full: the newest event is the one we lose */
        return -1;
    }

    input_event_t item = *ev;
    item.timestamp = timer_get_ticks();
    input_queue[input_head] = item;
    input_head = next;
    input_posted++;

    switch (ev->type) {
        case INPUT_EVENT_KEY_DOWN:            input_key_down_count++; break;
        case INPUT_EVENT_KEY_UP:              input_key_up_count++; break;
        case INPUT_EVENT_MOUSE_MOVE:          input_mouse_move_count++; break;
        case INPUT_EVENT_MOUSE_BUTTON_DOWN:
        case INPUT_EVENT_MOUSE_BUTTON_UP:     input_mouse_button_count++; break;
        default: break;
    }
    return 0;
}

int input_poll_event(input_event_t* out) {
    if (!out) return -1;
    if ((uint32_t)input_tail == input_head) return 0;

    *out = input_queue[input_tail];
    input_tail = (input_tail + 1) & INPUT_QUEUE_MASK;
    input_delivered++;

    if (out->type == INPUT_EVENT_KEY_DOWN || out->type == INPUT_EVENT_KEY_UP) {
        active_modifiers = out->modifiers;
    } else if (out->type == INPUT_EVENT_MOUSE_MOVE ||
               out->type == INPUT_EVENT_MOUSE_BUTTON_DOWN ||
               out->type == INPUT_EVENT_MOUSE_BUTTON_UP ||
               out->type == INPUT_EVENT_MOUSE_WHEEL) {
        /* Only the modifier state is still derived here. The pointer itself is
         * owned by the posting side, so a slow consumer cannot rewind it to a
         * position the mouse already left. */
        if (out->type != INPUT_EVENT_MOUSE_MOVE) {
            pointer_buttons = out->buttons;
        }
    }
    return 1;
}

int input_event_available(void) {
    return (uint32_t)input_tail != input_head;
}

void input_flush(void) {
    cpu_cli();
    input_tail = input_head;
    cpu_sti();
}

int input_wait_event(input_event_t* out, int max_wait_ticks) {
    if (!out) return -1;

    uint64_t start = timer_get_ticks();
    for (;;) {
        if (input_poll_event(out)) return 1;
        if (max_wait_ticks >= 0) {
            uint64_t elapsed = timer_get_ticks() - start;
            if (elapsed >= (uint64_t)max_wait_ticks) return 0;
        }
        cpu_hlt();   /* idle until the next timer tick or input interrupt */
    }
}

/* ------------------------------------------------------------------------------
 * Pointer state
 * -------------------------------------------------------------------------- */

void input_set_pointer_bounds(int width, int height) {
    pointer_max_x = (width > 0) ? width - 1 : 0;
    pointer_max_y = (height > 0) ? height - 1 : 0;

    if (pointer_max_x < 0) pointer_max_x = 0;
    if (pointer_max_y < 0) pointer_max_y = 0;

    if (pointer_x > pointer_max_x) pointer_x = pointer_max_x;
    if (pointer_y > pointer_max_y) pointer_y = pointer_max_y;
    if (pointer_x < 0) pointer_x = 0;
    if (pointer_y < 0) pointer_y = 0;
}

static inline int32_t clamp_axis(int32_t value, int max) {
    if (value < 0) return 0;
    if (max > 0 && value > max) return max;
    return value;
}

/* The driver owns the absolute pointer position and advances it the moment a
 * packet arrives, not when the event is finally polled. A single monitor
 * mouse_move turns into a burst of packets, and every one of them has to keep
 * counting towards the same position - otherwise each event reports the same
 * stale base and the pointer never gets near where it was moved to. */
int input_post_mouse_move(int dx, int dy) {
    input_event_t ev;
    pointer_x = clamp_axis(pointer_x + dx, pointer_max_x);
    pointer_y = clamp_axis(pointer_y + dy, pointer_max_y);

    ev.type = INPUT_EVENT_MOUSE_MOVE;
    ev.scancode = 0;
    ev.ascii = 0;
    ev.extended = 0;
    ev.modifiers = 0;
    ev.buttons = pointer_buttons;
    ev.dx = dx;
    ev.dy = dy;
    ev.x = pointer_x;
    ev.y = pointer_y;
    ev.timestamp = 0;
    return input_post_event(&ev);
}

int input_post_mouse_button(uint16_t buttons, int pressed, int dx, int dy) {
    input_event_t ev;
    pointer_x = clamp_axis(pointer_x + dx, pointer_max_x);
    pointer_y = clamp_axis(pointer_y + dy, pointer_max_y);

    ev.type = pressed ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP;
    ev.scancode = 0;
    ev.ascii = 0;
    ev.extended = 0;
    ev.modifiers = 0;
    ev.buttons = buttons;
    ev.dx = dx;
    ev.dy = dy;
    ev.x = pointer_x;
    ev.y = pointer_y;
    ev.timestamp = 0;

    pointer_buttons = buttons;
    return input_post_event(&ev);
}

int input_post_mouse_wheel(int dz) {
    input_event_t ev;
    ev.type = INPUT_EVENT_MOUSE_WHEEL;
    ev.scancode = 0;
    ev.ascii = 0;
    ev.extended = 0;
    ev.modifiers = 0;
    ev.buttons = pointer_buttons;
    ev.dx = 0;
    ev.dy = dz;
    ev.x = pointer_x;
    ev.y = pointer_y;
    ev.timestamp = 0;
    return input_post_event(&ev);
}

void input_get_pointer(int* x, int* y) {
    if (x) *x = (int)pointer_x;
    if (y) *y = (int)pointer_y;
}

int input_set_pointer(int x, int y) {
    int32_t nx = clamp_axis(x, pointer_max_x);
    int32_t ny = clamp_axis(y, pointer_max_y);
    int moved = (nx != pointer_x) || (ny != pointer_y);
    pointer_x = nx;
    pointer_y = ny;
    return moved;
}

int input_get_buttons(void) {
    return (int)pointer_buttons;
}

uint16_t input_get_modifiers(void) {
    return active_modifiers;
}

void input_get_stats(input_stats_t* out) {
    if (!out) return;
    out->posted = input_posted;
    out->delivered = input_delivered;
    out->dropped = input_dropped;
    out->key_down = input_key_down_count;
    out->key_up = input_key_up_count;
    out->mouse_move = input_mouse_move_count;
    out->mouse_buttons = input_mouse_button_count;
    out->pointer_x = (uint32_t)pointer_x;
    out->pointer_y = (uint32_t)pointer_y;
}

/* ------------------------------------------------------------------------------
 * Console character input (used by the text shell and SYS_READ on stdin)
 * -------------------------------------------------------------------------- */

/* The keyboard driver already translated the scancode, so the console only has
 * to look at the ASCII column of the event.  Navigation keys arrive as the
 * INPUT_KEY_* codes, matching what the text shell expects. */
uint8_t input_getchar(void) {
    input_event_t ev;

    for (;;) {
        if (!input_poll_event(&ev)) {
            cpu_hlt();
            continue;
        }
        if (ev.type != INPUT_EVENT_KEY_DOWN) continue;
        if (ev.ascii) return ev.ascii;
    }
}
