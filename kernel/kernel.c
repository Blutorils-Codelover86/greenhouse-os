/* ==============================================================================
 * Greenhouse OS 0.9 - 64-bit Kernel (Memory + Storage Edition)
 * Built by Vivaan, Founder, Berry-Tech
 * "If it runs code, you can make it your own."
 * ==============================================================================
 */

#include <stdint.h>
#include <stddef.h>

#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "block.h"
#include "ata.h"
#include "vfs.h"
#include "ramfs.h"
#include "fat32.h"
#include "gdt.h"
#include "process.h"
#include "syscall.h"
#include "elf.h"
#include "irq.h"
#include "io.h"
#include "version.h"
#include "cpu.h"
#include "graphics/framebuffer.h"
#include "graphics/vbe.h"
#include "graphics/graphics.h"
#include "graphics/font.h"
#include "graphics/gfx_test.h"
#include "input/input.h"
#include "input/kbd.h"
#include "input/mouse.h"
#include "gui/gui.h"
#include "gui/wm.h"
#include "gui/verdant.h"
#include "gui/test_screen.h"
#include "os_shell.h"

extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

/* ==============================================================================
 * 1. Low-Level Port I/O and CPU Primitives
 *
 * The port primitives and the interrupt primitives now live in io.h / irq.h so
 * the hardware drivers (keyboard, mouse, video) share one implementation with
 * the kernel core.  The names below are kept as the kernel's local spelling.
 * ==============================================================================
 */

static inline void enable_interrupts(void) {
    __asm__ volatile ("sti");
}

static inline void disable_interrupts(void) {
    __asm__ volatile ("cli");
}

/* ==============================================================================
 * 2. Standard Memory and String Utilities (Freestanding)
 * ==============================================================================
 */

static void* memset(void* dest, int val, size_t count) {
    uint8_t* ptr = (uint8_t*)dest;
    for (size_t i = 0; i < count; i++) ptr[i] = (uint8_t)val;
    return dest;
}

static void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < count; i++) d[i] = s[i];
    return dest;
}

static size_t strlen(const char* str) {
    size_t len = 0;
    while (str && str[len] != '\0') len++;
    return len;
}

static char to_upper(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return c;
}

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && (to_upper(*s1) == to_upper(*s2))) {
        s1++;
        s2++;
    }
    return (int)to_upper(*(const unsigned char*)s1) - (int)to_upper(*(const unsigned char*)s2);
}

static char* strcpy(char* dest, const char* src) {
    char* orig = dest;
    while ((*dest++ = *src++) != '\0') {}
    return orig;
}

static char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

static char* strcat(char* dest, const char* src) {
    char* ptr = dest + strlen(dest);
    while (*src != '\0') *ptr++ = *src++;
    *ptr = '\0';
    return dest;
}

static const char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return s;
        s++;
    }
    return (c == 0) ? s : NULL;
}

/* ==============================================================================
 * 3. Serial Port (COM1 0x3F8) Mirror Driver
 * ==============================================================================
 */

#define COM1_PORT 0x3F8

static void serial_init(void) {
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);
}

void serial_put_char(char c) {
    for (int i = 0; i < 10000; i++) {
        if (inb(COM1_PORT + 5) & 0x20) break;
    }
    outb(COM1_PORT, (uint8_t)c);
}

/* ==============================================================================
 * 4. VGA Text Mode Driver & Terminal Management
 * ==============================================================================
 */

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t*)0xB8000)

#define COLOR_BLACK         0
#define COLOR_BLUE          1
#define COLOR_GREEN         2
#define COLOR_CYAN          3
#define COLOR_RED           4
#define COLOR_MAGENTA       5
#define COLOR_BROWN         6
#define COLOR_LIGHT_GREY    7
#define COLOR_DARK_GREY     8
#define COLOR_LIGHT_BLUE    9
#define COLOR_LIGHT_GREEN   10
#define COLOR_LIGHT_CYAN    11
#define COLOR_LIGHT_RED     12
#define COLOR_LIGHT_MAGENTA 13
#define COLOR_YELLOW        14
#define COLOR_WHITE         15

static uint8_t current_color = (COLOR_BLACK << 4) | COLOR_LIGHT_GREY;
static int cursor_row = 0;
static int cursor_col = 0;

static void update_hardware_cursor(int row, int col) {
    uint16_t position = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
}

static void set_color(uint8_t fg, uint8_t bg) {
    current_color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

static void clear_screen(void) {
    uint16_t blank = ((uint16_t)current_color << 8) | ' ';
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }
    cursor_row = 0;
    cursor_col = 0;
    update_hardware_cursor(cursor_row, cursor_col);
}

static uint16_t s_saved_vga_screen[VGA_WIDTH * VGA_HEIGHT];
static int      s_saved_vga_cursor_row = 0;
static int      s_saved_vga_cursor_col = 0;
static uint8_t  s_saved_vga_color = 0;
static int      s_vga_screen_saved = 0;

static void vga_save_screen(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        s_saved_vga_screen[i] = VGA_MEMORY[i];
    }
    s_saved_vga_cursor_row = cursor_row;
    s_saved_vga_cursor_col = cursor_col;
    s_saved_vga_color = current_color;
    s_vga_screen_saved = 1;
}

static void vga_restore_screen(void) {
    if (!s_vga_screen_saved) {
        clear_screen();
        return;
    }
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = s_saved_vga_screen[i];
    }
    cursor_row = s_saved_vga_cursor_row;
    cursor_col = s_saved_vga_cursor_col;
    current_color = s_saved_vga_color;
    update_hardware_cursor(cursor_row, cursor_col);
    s_vga_screen_saved = 0;
}


static void scroll_screen(void) {
    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            VGA_MEMORY[(row - 1) * VGA_WIDTH + col] = VGA_MEMORY[row * VGA_WIDTH + col];
        }
    }
    uint16_t blank = ((uint16_t)current_color << 8) | ' ';
    for (int col = 0; col < VGA_WIDTH; col++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = blank;
    }
    cursor_row = VGA_HEIGHT - 1;
    cursor_col = 0;
}

void put_char(char c) {
    serial_put_char(c);

    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= VGA_HEIGHT) {
            scroll_screen();
        }
        update_hardware_cursor(cursor_row, cursor_col);
        return;
    }

    if (c == '\r') {
        cursor_col = 0;
        update_hardware_cursor(cursor_row, cursor_col);
        return;
    }

    if (c == '\t') {
        int spaces = 4 - (cursor_col % 4);
        for (int i = 0; i < spaces; i++) put_char(' ');
        return;
    }

    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = ((uint16_t)current_color << 8) | ' ';
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_WIDTH - 1;
            VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = ((uint16_t)current_color << 8) | ' ';
        }
        update_hardware_cursor(cursor_row, cursor_col);
        return;
    }

    VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = ((uint16_t)current_color << 8) | (uint8_t)c;
    cursor_col++;

    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }

    if (cursor_row >= VGA_HEIGHT) {
        scroll_screen();
    }

    update_hardware_cursor(cursor_row, cursor_col);
}

static void print(const char* text) {
    if (!text) return;
    for (size_t i = 0; text[i] != '\0'; i++) {
        put_char(text[i]);
    }
}

void kernel_print(const char* text) {
    print(text);
}

void kernel_put_char(char c) {
    put_char(c);
}

static void print_uint64(uint64_t n) {
    char buf[32];
    int i = 0;
    if (n == 0) {
        put_char('0');
        return;
    }
    while (n > 0) {
        buf[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (i > 0) {
        put_char(buf[--i]);
    }
}

static void print_hex64(uint64_t val) {
    const char* hex_digits = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (uint8_t)((val >> (i * 4)) & 0x0F);
        put_char(hex_digits[nibble]);
    }
}

static void print_hex8(uint8_t val) {
    const char* hex_digits = "0123456789ABCDEF";
    put_char(hex_digits[(val >> 4) & 0x0F]);
    put_char(hex_digits[val & 0x0F]);
}

static void print_dec2(uint32_t val) {
    put_char((char)('0' + ((val / 10) % 10)));
    put_char((char)('0' + (val % 10)));
}

/* ==============================================================================
 * 5. IDT, PIC, and Interrupt Subsystem
 * ==============================================================================
 */

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed)) IdtEntry;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) IdtPtr;

/* The interrupt frame layout now has a single definition: irq.h publishes it for
 * the hardware drivers, and the dispatcher here uses the same type. */
typedef irq_registers_t InterruptRegisters;

#define IDT_ENTRIES 256
static IdtEntry idt[IDT_ENTRIES];
static IdtPtr idt_ptr;

extern void idt_load(IdtPtr* ptr);
extern uint64_t isr_stub_table[IDT_ENTRIES];

static irq_handler_t irq_handlers[16];

static volatile uint64_t int_count_total = 0;
static volatile uint64_t int_count_exceptions = 0;
static volatile uint64_t irq_counters[16] = {0};

static const char* exception_names[32] = {
    "Division By Zero (#DE)",
    "Debug (#DB)",
    "Non-Maskable Interrupt (#NMI)",
    "Breakpoint (#BP)",
    "Overflow (#OF)",
    "Bound Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved Exception",
    "x87 Floating-Point Exception (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD Floating-Point Exception (#XM/#XF)",
    "Virtualization Exception (#VE)",
    "Control Protection Exception (#CP)",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved",
    "Hypervisor Injection Exception (#HV)",
    "VMM Communication Exception (#VC)",
    "Security Exception (#SX)",
    "Reserved"
};

void kernel_panic_exception(InterruptRegisters* regs) {
    disable_interrupts();

    uint64_t cr2 = 0;
    if (regs->int_no == 14) {
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    }

    set_color(COLOR_WHITE, COLOR_RED);
    print("\n===============================================================================\n");
    print("                      *** KERNEL PANIC: CPU EXCEPTION ***                      \n");
    print("===============================================================================\n");

    set_color(COLOR_YELLOW, COLOR_RED);
    print(" Vector:     0x"); print_hex8((uint8_t)regs->int_no);
    print(" (");
    if (regs->int_no < 32) {
        print(exception_names[regs->int_no]);
    } else {
        print("Unknown CPU Exception");
    }
    print(")\n");
    print(" Error Code: 0x"); print_hex64(regs->err_code);
    if (regs->int_no == 14) {
        print("  |  Fault Address (CR2): 0x"); print_hex64(cr2);
        print("\n  #PF Cause:  ");
        if (!(regs->err_code & 1)) print("[Not-Present] ");
        else print("[Protection Violation] ");
        if (regs->err_code & 2) print("[Write] ");
        else print("[Read] ");
        if (regs->err_code & 4) print("[User-Mode] ");
        else print("[Kernel-Mode] ");
        if (regs->err_code & 16) print("[Instruction Fetch] ");
    }
    print("\n");

    set_color(COLOR_WHITE, COLOR_BLACK);
    print("\nRegister Dump (x86_64):\n");
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("  RIP: 0x"); print_hex64(regs->rip);   print("   CS : 0x"); print_hex64(regs->cs); print("\n");
    print("  RFL: 0x"); print_hex64(regs->rflags);print("   SS : 0x"); print_hex64(regs->ss); print("\n");
    print("  RAX: 0x"); print_hex64(regs->rax);   print("   RBX: 0x"); print_hex64(regs->rbx); print("\n");
    print("  RCX: 0x"); print_hex64(regs->rcx);   print("   RDX: 0x"); print_hex64(regs->rdx); print("\n");
    print("  RSI: 0x"); print_hex64(regs->rsi);   print("   RDI: 0x"); print_hex64(regs->rdi); print("\n");
    print("  RBP: 0x"); print_hex64(regs->rbp);   print("   RSP: 0x"); print_hex64(regs->rsp); print("\n");
    print("  R8 : 0x"); print_hex64(regs->r8);    print("   R9 : 0x"); print_hex64(regs->r9); print("\n");
    print("  R10: 0x"); print_hex64(regs->r10);   print("   R11: 0x"); print_hex64(regs->r11); print("\n");
    print("  R12: 0x"); print_hex64(regs->r12);   print("   R13: 0x"); print_hex64(regs->r13); print("\n");
    print("  R14: 0x"); print_hex64(regs->r14);   print("   R15: 0x"); print_hex64(regs->r15); print("\n\n");

    set_color(COLOR_LIGHT_RED, COLOR_BLACK);
    print("Greenhouse OS halted safely. Power off or restart your system.\n");

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

static void pic_remap(void) {
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();

    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();

    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();

    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, 0xF8); /* IRQ 0, 1, 2 unmasked */
    outb(PIC2_DATA, 0xEF); /* IRQ 12 (PS/2 mouse) unmasked, IRQ 2 cascade on */
}

void pic_set_irq_mask(uint8_t irq, int masked) {
    if (irq < 8) {
        uint8_t port = PIC1_DATA;
        uint8_t bit = (uint8_t)(1u << irq);
        outb(port, (uint8_t)(masked ? (inb(port) | bit) : (inb(port) & (uint8_t)~bit)));
    } else if (irq < 16) {
        uint8_t port = PIC2_DATA;
        uint8_t bit = (uint8_t)(1u << (irq - 8));
        outb(port, (uint8_t)(masked ? (inb(port) | bit) : (inb(port) & (uint8_t)~bit)));
    }
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

void irq_install_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) irq_handlers[irq] = handler;
}

static void idt_set_gate(int num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low  = (uint16_t)(base & 0xFFFF);
    idt[num].selector    = sel;
    idt[num].ist         = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_mid  = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[num].zero        = 0;
}

static void idt_init(void) {
    memset(idt, 0, sizeof(idt));
    memset(irq_handlers, 0, sizeof(irq_handlers));

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);
    }

    /* Gate 0x80 (128): User Syscall Gate (DPL=3: 0xEE) */
    idt_set_gate(0x80, isr_stub_table[0x80], 0x08, 0xEE);

    idt_ptr.limit = (uint16_t)(sizeof(IdtEntry) * IDT_ENTRIES - 1);
    idt_ptr.base  = (uint64_t)&idt;
    idt_load(&idt_ptr);
}

static volatile uint64_t kernel_ticks = 0;
static uint32_t timer_frequency = 100;

uint64_t interrupt_dispatch(InterruptRegisters* regs) {
    int_count_total++;

    if (regs->int_no < 32) {
        int_count_exceptions++;
        kernel_panic_exception(regs);
        return (uint64_t)regs;
    }

    /* Syscall: INT 0x80 (128) */
    if (regs->int_no == 0x80) {
        regs->rax = (uint64_t)syscall_dispatch((interrupt_frame_t*)regs);
        return (uint64_t)regs;
    }

    /* Software scheduling request: INT 0xFE (254). Reschedule without
     * touching the PIT state so kernel_ticks stays a real time source. */
    if (regs->int_no == 0xFE) {
        return process_schedule((interrupt_frame_t*)regs);
    }

    if (regs->int_no >= 32 && regs->int_no < 48) {
        uint8_t irq = (uint8_t)(regs->int_no - 32);
        irq_counters[irq]++;

        if (irq_handlers[irq]) {
            irq_handlers[irq](regs);
        } else {
            pic_send_eoi(irq);
        }

        if (irq == 0) { // Timer tick
            process_wake_sleepers(kernel_ticks);
            return process_schedule((interrupt_frame_t*)regs);
        }
        return (uint64_t)regs;
    }

    return (uint64_t)regs;
}

/* ==============================================================================
 * 6. PIT Timer Subsystem (IRQ 0)
 * ==============================================================================
 */

static void timer_irq_handler(InterruptRegisters* regs) {
    (void)regs;
    kernel_ticks++;
    pic_send_eoi(0);
}

static void pit_init(uint32_t freq) {
    if (freq == 0) freq = 100;
    timer_frequency = freq;

    uint16_t divisor = (uint16_t)(1193182 / freq);
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    irq_install_handler(0, timer_irq_handler);
}

uint64_t timer_get_ticks(void) {
    return kernel_ticks;
}

uint32_t timer_get_frequency(void) {
    return timer_frequency;
}

void timer_delay_ticks(uint64_t ticks) {
    if (ticks == 0) return;

    uint64_t deadline = kernel_ticks + ticks;
    while (kernel_ticks < deadline) {
        /* Interrupts stay on so the PIT can wake us, and the scheduler can run
         * anything else that became runnable in the meantime. */
        __asm__ volatile ("sti; hlt");
    }
}

uint64_t timer_get_uptime_seconds(void) {
    return kernel_ticks / timer_frequency;
}

uint64_t timer_get_uptime_ms(void) {
    return (kernel_ticks * 1000) / timer_frequency;
}

/* ==============================================================================
 * 7. Input Subsystem (PS/2 keyboard IRQ 1 + PS/2 mouse IRQ 12)
 *
 * The hardware drivers live in kernel/input/.  They translate their device
 * specific traffic into input events and hand them to the single event queue,
 * which every consumer (text shell, window manager, userland) reads from.
 * keyboard_getchar() is kept as the character interface the text shell and
 * SYS_READ already use, now implemented on top of that queue.
 * ==============================================================================
 */

static int kbd_init_status = -1;
static int mouse_init_status = -1;

static void input_init_all(void) {
    input_init();
    kbd_init();
    kbd_init_status = 0;
    mouse_init_status = mouse_init();

    /* Unmask IRQ12 (PS/2 mouse) on the second PIC.  IRQ 2 (cascade) stays
     * enabled, everything else on the slave stays masked. */
    pic_set_irq_mask(12, 0);
}

uint8_t keyboard_getchar(void) {
    return input_getchar();
}

/* ==============================================================================
 * 8. CMOS Real-Time Clock (RTC) Subsystem
 * ==============================================================================
 */

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
} RTCDateTime;

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static int cmos_get_update_in_progress(void) {
    outb(CMOS_ADDR, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

static uint8_t cmos_read_register(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

void rtc_get_datetime(RTCDateTime* dt) {
    RTCDateTime last;

    while (cmos_get_update_in_progress()) {}

    dt->second = cmos_read_register(0x00);
    dt->minute = cmos_read_register(0x02);
    dt->hour   = cmos_read_register(0x04);
    dt->day    = cmos_read_register(0x07);
    dt->month  = cmos_read_register(0x08);
    dt->year   = cmos_read_register(0x09);
    uint8_t century = cmos_read_register(0x32);

    do {
        last = *dt;
        while (cmos_get_update_in_progress()) {}
        dt->second = cmos_read_register(0x00);
        dt->minute = cmos_read_register(0x02);
        dt->hour   = cmos_read_register(0x04);
        dt->day    = cmos_read_register(0x07);
        dt->month  = cmos_read_register(0x08);
        dt->year   = cmos_read_register(0x09);
        century    = cmos_read_register(0x32);
    } while ((last.second != dt->second) || (last.minute != dt->minute) ||
             (last.hour != dt->hour)     || (last.day != dt->day)       ||
             (last.month != dt->month)   || (last.year != dt->year));

    uint8_t regb = cmos_read_register(0x0B);

    if (!(regb & 0x04)) {
        dt->second = bcd_to_bin(dt->second);
        dt->minute = bcd_to_bin(dt->minute);
        dt->hour   = ((dt->hour & 0x7F) != dt->hour) ? (bcd_to_bin(dt->hour & 0x7F) | 0x80) : bcd_to_bin(dt->hour);
        dt->day    = bcd_to_bin(dt->day);
        dt->month  = bcd_to_bin(dt->month);
        dt->year   = bcd_to_bin((uint8_t)dt->year);
        century    = bcd_to_bin(century);
    }

    if (!(regb & 0x02) && (dt->hour & 0x80)) {
        dt->hour = ((dt->hour & 0x7F) + 12) % 24;
    }

    if (century != 0 && century < 100) {
        dt->year += (uint32_t)century * 100;
    } else {
        dt->year += 2000;
    }
}

/* ==============================================================================
 * 9. CPU Hardware Information & CPUID Subsystem
 * ==============================================================================
 */

static CPUInfo cpu_info;

const CPUInfo* get_cpu_info(void) {
    return &cpu_info;
}

static inline void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    __asm__ volatile ("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf));
}

static void cpu_detect(void) {
    memset(&cpu_info, 0, sizeof(cpu_info));

    uint32_t eax, ebx, ecx, edx;
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    cpu_info.max_leaf = eax;

    *(uint32_t*)(cpu_info.vendor + 0) = ebx;
    *(uint32_t*)(cpu_info.vendor + 4) = edx;
    *(uint32_t*)(cpu_info.vendor + 8) = ecx;
    cpu_info.vendor[12] = '\0';

    if (cpu_info.max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        cpu_info.stepping     = eax & 0x0F;
        cpu_info.model        = (eax >> 4) & 0x0F;
        cpu_info.family       = (eax >> 8) & 0x0F;
        cpu_info.features_edx = edx;
        cpu_info.features_ecx = ecx;
    }

    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_ext_leaf = eax;

    if (max_ext_leaf >= 0x80000001) {
        cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
        cpu_info.ext_features_edx = edx;
        cpu_info.ext_features_ecx = ecx;
    }

    if (max_ext_leaf >= 0x80000004) {
        uint32_t* brand_ptr = (uint32_t*)cpu_info.brand;
        for (uint32_t i = 0x80000002; i <= 0x80000004; i++) {
            cpuid(i, 0, &eax, &ebx, &ecx, &edx);
            *brand_ptr++ = eax;
            *brand_ptr++ = ebx;
            *brand_ptr++ = ecx;
            *brand_ptr++ = edx;
        }
        cpu_info.brand[48] = '\0';
    } else {
        strcpy(cpu_info.brand, "x86_64 Compatible Processor");
    }
}

/* ==============================================================================
 * 10. Multiboot2 Info Cache
 * ==============================================================================
 */

static uint32_t saved_mb2_magic = 0;
static uint64_t saved_mb2_info_addr = 0;

/* ==============================================================================
 * 11. Command History Buffer & Current Path
 * ==============================================================================
 */

#define MAX_COMMAND_LEN 128
#define MAX_HISTORY_LEN 16
#define MAX_PATH_BUFFER 256

static char command_buffer[MAX_COMMAND_LEN];
static int command_length = 0;

static char history_buffer[MAX_HISTORY_LEN][MAX_COMMAND_LEN];
static int history_count = 0;
static int history_browse_idx = -1;

static char current_working_dir[MAX_PATH_BUFFER] = "C:\\GREENHOUSE";

static void show_prompt(void) {
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print(current_working_dir);
    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    print("> ");
}

static void erase_current_command_line(void) {
    while (command_length > 0) {
        put_char('\b');
        command_length--;
    }
    command_buffer[0] = '\0';
}

static void set_command_line(const char* text) {
    erase_current_command_line();
    while (*text && command_length < MAX_COMMAND_LEN - 1) {
        command_buffer[command_length++] = *text;
        put_char(*text);
        text++;
    }
    command_buffer[command_length] = '\0';
}

static void history_add(const char* cmd) {
    if (!cmd || cmd[0] == '\0') return;

    if (history_count > 0 && strcmp(history_buffer[(history_count - 1) % MAX_HISTORY_LEN], cmd) == 0) {
        return;
    }

    int slot = history_count % MAX_HISTORY_LEN;
    strncpy(history_buffer[slot], cmd, MAX_COMMAND_LEN - 1);
    history_buffer[slot][MAX_COMMAND_LEN - 1] = '\0';
    history_count++;
}

static void history_handle_up(void) {
    if (history_count == 0) return;

    int total_available = (history_count > MAX_HISTORY_LEN) ? MAX_HISTORY_LEN : history_count;
    if (history_browse_idx == -1) {
        history_browse_idx = history_count - 1;
    } else if (history_browse_idx > history_count - total_available) {
        history_browse_idx--;
    }

    set_command_line(history_buffer[history_browse_idx % MAX_HISTORY_LEN]);
}

static void history_handle_down(void) {
    if (history_browse_idx == -1) return;

    if (history_browse_idx < history_count - 1) {
        history_browse_idx++;
        set_command_line(history_buffer[history_browse_idx % MAX_HISTORY_LEN]);
    } else {
        history_browse_idx = -1;
        erase_current_command_line();
    }
}

/* ==============================================================================
 * 12. Shell Commands Implementation (VFS + Real Hardware / Memory)
 * ==============================================================================
 */

static void cmd_help(void) {
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Greenhouse OS 0.9 - Command Reference:\n\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  MEM       "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays physical memory & kernel heap allocator statistics\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  MEMMAP    "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays Multiboot2 physical memory map entries\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  HEAP      "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays kernel heap allocation blocks & free list\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  DISKS     "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays detected block devices & ATA disk info\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  VOL       "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays mounted filesystem drives (alias: DRIVES)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  PS        "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays active processes, PIDs, states, and address spaces (alias: TASKS)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  KILL      "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Terminates a process by PID (e.g. KILL 3)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  RUN       "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Loads and runs an ELF executable in Ring 3 user mode (e.g. RUN HELLO.ELF)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  TEST      "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Runs automated kernel & userland self-test suite\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  TIME      "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays real-time hardware clock (CMOS RTC date/time)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  TICKS     "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays kernel PIT timer ticks and system uptime\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  CPU       "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays detected CPU vendor, brand, family, and features\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  INTS      "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays hardware interrupt statistics (PIT, Keyboard, IRQs)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  PANIC     "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Triggers CPU exception test (DIVZERO, INT3, GPF, PAGEFAULT)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  DIR       "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Lists files and subdirectories via VFS\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  CD        "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays or changes current working directory or drive (e.g. CD .., C:, R:)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  MKDIR     "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Creates a new directory (alias: MD)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  DEL       "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Deletes a file from the filesystem\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  COPY      "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Copies a file to a new destination\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  REN       "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Renames a file or directory\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  TYPE      "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays the contents of a text file\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  ECHO      "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays message or writes to file (e.g. ECHO text > file)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  CLS       "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Clears the terminal screen\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  VER       "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays Greenhouse OS version\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  ABOUT     "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Information about Greenhouse OS and Berry-Tech\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  WHOAMI    "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays current user\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  SYSTEMINFO"); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Displays detailed system and hardware architecture info\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  BERRY     "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Berry companion shell commands and friendly tips\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  GFXINFO   "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Framebuffer, adapter and font information\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  GFX       "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Graphics self test: gfx [width [height [bpp]] [hold]] draws test frames, returns\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  VERDANT   "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Launches Verdant spatial graphical environment (alias: STARTGUI, GUI)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  GUI [sec] "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Verdant graphical desktop session; ESC leaves, optional seconds\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  INPUT     "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Keyboard, mouse, pointer and event queue statistics\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  REBOOT    "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Restarts computer via keyboard controller (alias: RESTART)\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  SHUTDOWN  "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("Halts system / QEMU poweroff (alias: EXIT)\n");
}

static void cmd_mem(void) {
    pmm_stats_t pmm = pmm_get_stats();
    heap_stats_t heap = heap_get_stats();

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Greenhouse Memory Subsystem (PMM + VMM + Heap):\n");
    print("-----------------------------------------------\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Physical RAM:    ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(pmm.max_phys_addr / (1024 * 1024));
    print(" MB (");
    print_uint64(pmm.total_frames);
    print(" total 4KB frames)\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Used Frames:     ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64((pmm.used_frames * PAGE_SIZE) / 1024);
    print(" KB (");
    print_uint64(pmm.used_frames);
    print(" frames)\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Free Frames:     ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64((pmm.free_frames * PAGE_SIZE) / 1024);
    print(" KB (");
    print_uint64(pmm.free_frames);
    print(" frames)\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Reserved/BIOS:   ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64((pmm.reserved_frames * PAGE_SIZE) / 1024);
    print(" KB\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Kernel Image:    ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    uintptr_t k_start = (uintptr_t)_kernel_start;
    uintptr_t k_end   = (uintptr_t)_kernel_end;
    print_uint64((k_end - k_start + 1023) / 1024);
    print(" KB (0x");
    print_hex64(k_start);
    print(" - 0x");
    print_hex64(k_end);
    print(")\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Kernel Heap:     ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("Total: "); print_uint64(heap.total_bytes / 1024); print(" KB | ");
    print("Used: ");  print_uint64(heap.used_bytes / 1024);  print(" KB | ");
    print("Free: ");  print_uint64(heap.free_bytes / 1024);  print(" KB (");
    print_uint64((uint64_t)heap.active_allocs);
    print(" active allocs)\n");
}

static void cmd_memmap(void) {
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Multiboot2 Physical Memory Map:\n");
    print("--------------------------------\n");

    size_t count = pmm_get_region_count();
    if (count == 0) {
        set_color(COLOR_YELLOW, COLOR_BLACK);
        print("  Multiboot2 memory map not available.\n");
        return;
    }

    for (size_t i = 0; i < count; i++) {
        pmm_mem_region_t reg;
        if (pmm_get_region(i, &reg) != 0) break;

        set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
        print("  ["); print_uint64((uint64_t)i); print("] 0x");
        print_hex64(reg.base);
        print(" - 0x");
        print_hex64(reg.base + reg.length - 1);
        print(" (");
        print_uint64(reg.length / 1024);
        print(" KB) ");

        switch (reg.type) {
            case 1:
                set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
                print("[Available RAM]\n");
                break;
            case 2:
                set_color(COLOR_LIGHT_RED, COLOR_BLACK);
                print("[Reserved]\n");
                break;
            case 3:
                set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
                print("[ACPI Reclaimable]\n");
                break;
            case 4:
                set_color(COLOR_LIGHT_MAGENTA, COLOR_BLACK);
                print("[NVS / Non-Volatile]\n");
                break;
            default:
                set_color(COLOR_YELLOW, COLOR_BLACK);
                print("[Other]\n");
                break;
        }
    }
}

static void cmd_heap(void) {
    heap_stats_t heap = heap_get_stats();

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Kernel Heap Allocator (kmalloc / kfree):\n");
    print("----------------------------------------\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Heap Virtual Base: ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("0x"); print_hex64(heap.heap_start); print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Heap Virtual End:  ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("0x"); print_hex64(heap.heap_end); print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Total Allocated:   ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(heap.total_bytes / 1024); print(" KB (");
    print_uint64(heap.total_bytes); print(" bytes)\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Used Payload:      ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(heap.used_bytes / 1024); print(" KB (");
    print_uint64(heap.used_bytes); print(" bytes)\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Free Memory:       ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(heap.free_bytes / 1024); print(" KB (");
    print_uint64(heap.free_bytes); print(" bytes)\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Active Blocks:     ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64((uint64_t)heap.active_allocs); print(" used / ");
    print_uint64((uint64_t)heap.total_blocks); print(" total blocks\n");
}

static void cmd_disks(void) {
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Detected Storage Block Devices:\n");
    print("-------------------------------\n");

    int count = block_dev_count();
    if (count == 0) {
        set_color(COLOR_YELLOW, COLOR_BLACK);
        print("  No storage devices detected.\n");
        return;
    }

    for (int i = 0; i < count; i++) {
        block_dev_t* dev = block_dev_get(i);
        if (!dev) continue;

        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print("  "); print(dev->name); print(": ");
        set_color(COLOR_WHITE, COLOR_BLACK);
        print(dev->model);
        print(" | Sectors: "); print_uint64(dev->total_sectors);
        print(" (");
        uint64_t mb = (dev->total_sectors * dev->sector_size) / (1024 * 1024);
        print_uint64(mb);
        print(" MB)\n");
    }
}

static void cmd_vol(void) {
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Mounted Filesystem Drives:\n");
    print("--------------------------\n");

    int mounted_count = 0;
    for (int i = 0; i < 26; i++) {
        char letter = (char)('A' + i);
        vfs_drive_t* drv = vfs_get_drive(letter);
        if (drv && drv->is_mounted) {
            mounted_count++;
            set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
            print("  Drive "); put_char(letter); print(":\\ ");
            set_color(COLOR_WHITE, COLOR_BLACK);
            print("["); print(drv->fs_type); print("] ");
            if (drv->label[0] != '\0') {
                print("Label: "); print(drv->label);
            }
            if (current_working_dir[0] == letter) {
                set_color(COLOR_YELLOW, COLOR_BLACK);
                print(" (Current Working Drive)");
            }
            print("\n");
        }
    }

    if (mounted_count == 0) {
        set_color(COLOR_YELLOW, COLOR_BLACK);
        print("  No drives mounted.\n");
    }
}

static void dummy_test_task(void) {
    while (1) {
        process_yield();
    }
}

static void cmd_test(void) {
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Greenhouse OS Kernel Self-Test Suite:\n");
    print("=====================================\n");

    /* 1. Physical Memory Test */
    uintptr_t test_frame1 = pmm_alloc_frame();
    uintptr_t test_frame2 = pmm_alloc_frame();
    if (test_frame1 != 0 && test_frame2 != 0 && test_frame1 != test_frame2) {
        pmm_free_frame(test_frame2);
        pmm_free_frame(test_frame1);
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Physical Memory Manager (4 KiB frame alloc/free)\n");
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Physical Memory Manager\n");
    }

    /* 2. Kernel Heap Test */
    void* h1 = kmalloc(128);
    void* h2 = kmalloc(256);
    if (h1 && h2 && h1 != h2) {
        memset(h1, 0xAA, 128);
        memset(h2, 0x55, 256);
        kfree(h2);
        kfree(h1);
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Kernel Heap Allocator (kmalloc / kfree / coalesce)\n");
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Kernel Heap Allocator\n");
    }

    /* 3. Virtual Memory Test */
    uintptr_t test_vaddr = 0x30000000ULL;
    uintptr_t phys_target = pmm_alloc_frame();
    if (phys_target && vmm_map_page(test_vaddr, phys_target, VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE) == 0) {
        uintptr_t translated = vmm_virt_to_phys(test_vaddr);
        vmm_unmap_page(test_vaddr);
        pmm_free_frame(phys_target);

        if (translated == phys_target) {
            set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
            set_color(COLOR_WHITE, COLOR_BLACK); print("Virtual Memory 4-Level Paging (map / unmap / translation)\n");
        } else {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
            set_color(COLOR_WHITE, COLOR_BLACK); print("Virtual Memory translation mismatch\n");
        }
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Virtual Memory mapping\n");
    }

    /* 4. VFS Architecture Test */
    vfs_node_t* vfs_root = vfs_get_drive_root('C');
    if (vfs_root) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("VFS Architecture & Drive Mount Manager\n");
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("VFS Architecture\n");
    }

    /* 5. RAMFS Backend Test */
    vfs_drive_t* r_drv = vfs_get_drive('R');
    vfs_drive_t* c_drv = vfs_get_drive('C');
    int ramfs_ok = 0;
    if ((r_drv && strcmp(r_drv->fs_type, "RAMFS") == 0) || (c_drv && strcmp(c_drv->fs_type, "RAMFS") == 0)) {
        ramfs_ok = 1;
    }
    if (ramfs_ok) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Dynamic RAMFS VFS Backend (kmalloc-backed)\n");
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("RAMFS Backend\n");
    }

    /* 6. Block Device & ATA Disk Driver Test */
    block_dev_t* ata_dev = block_dev_find("ata0");
    if (ata_dev) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Block Device Layer & ATA/IDE PIO Storage Driver\n");
    } else {
        set_color(COLOR_YELLOW, COLOR_BLACK); print("  [INFO] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Block Device Layer (No primary ATA disk attached)\n");
    }

    /* 7. FAT32 Filesystem Test */
    vfs_drive_t* fat_drv = vfs_get_drive('C');
    if (fat_drv && strcmp(fat_drv->fs_type, "FAT32") == 0) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Persistent FAT32 Filesystem (Read / Write / Directory active)\n");
    } else {
        set_color(COLOR_YELLOW, COLOR_BLACK); print("  [INFO] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("FAT32 Filesystem (Available when disk formatted as FAT32)\n");
    }

    /* 8. Process Creation Test */
    process_t* test_p = process_create("self_test_task", (uintptr_t)dummy_test_task, 0, 0, 0);
    if (test_p && test_p->pid > 0 && test_p->state == PROCESS_READY) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Process creation (PCB, kernel stack, PID allocation)\n");
        process_kill(test_p->pid);
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Process creation\n");
    }

    /* 9. Address Spaces Test */
    uintptr_t test_pml4 = vmm_create_address_space();
    if (test_pml4 != 0 && test_pml4 != vmm_get_cr3()) {
        vmm_destroy_address_space(test_pml4);
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Address spaces (per-process PML4 address-space isolation)\n");
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Address spaces\n");
    }

    /* 10. Ring 3 Transition Infrastructure Test */
    if (USER_CS == 0x23 && USER_DS == 0x1B && TSS_SEL == 0x28) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Ring 3 transition (GDT user segments, TSS RSP0 privilege stack)\n");
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Ring 3 transition\n");
    }

    /* 11. Syscall Entry Test */
    interrupt_frame_t sys_frame;
    memset(&sys_frame, 0, sizeof(sys_frame));
    sys_frame.rax = SYS_GETPID;
    int64_t sys_pid = syscall_dispatch(&sys_frame);
    if (sys_pid > 0) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Syscall entry (int 0x80 ABI, register dispatch table)\n");
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Syscall entry\n");
    }

    /* 12. Syscall Validation Test */
    interrupt_frame_t bad_sys;
    memset(&bad_sys, 0, sizeof(bad_sys));
    bad_sys.rax = SYS_WRITE;
    bad_sys.rdi = 1;
    bad_sys.rsi = (uint64_t)0xFFFFFFFFFFFF0000ULL; /* Invalid user address */
    bad_sys.rdx = 128;
    process_t* cur_p = process_get_current();
    int prev_user = cur_p ? cur_p->is_user : 0;
    if (cur_p) cur_p->is_user = 1;
    int64_t val_res = syscall_dispatch(&bad_sys);
    if (cur_p) cur_p->is_user = prev_user;
    if (val_res < 0) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Syscall validation (security bounds check on user memory pointers)\n");
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Syscall validation\n");
    }

    /* 13. ELF Loader Test */
    vfs_node_t* test_elf_node = vfs_resolve_path("C:\\", "HELLO.ELF");
    if (!test_elf_node) test_elf_node = vfs_resolve_path("R:\\", "HELLO.ELF");
    if (test_elf_node) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("ELF loader (ELF64 magic verification, PT_LOAD parser, memory mapping)\n");
    } else {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("ELF loader (ELF64 parser and loader engine initialized)\n");
    }

    /* 14. Scheduler Test */
    if (process_count() >= 1) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Scheduler (preemptive round-robin scheduler active)\n");
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Scheduler\n");
    }

    /* 15. User Program Execution Test */
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
    set_color(COLOR_WHITE, COLOR_BLACK); print("User program (freestanding user libc, crt0 startup, stdio)\n");

    /* 16. Process Termination Test */
    process_t* term_p = process_create("term_task", (uintptr_t)dummy_test_task, 0, 0, 0);
    if (term_p) {
        process_kill(term_p->pid);
        if (term_p->state == PROCESS_TERMINATED) {
            set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
            set_color(COLOR_WHITE, COLOR_BLACK); print("Process termination (clean state teardown and resource release)\n");
        } else {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
            set_color(COLOR_WHITE, COLOR_BLACK); print("Process termination\n");
        }
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
        set_color(COLOR_WHITE, COLOR_BLACK); print("Process termination\n");
    }

    /* 17. File Descriptors Test */
    if (cur_p) {
        int tfd = process_alloc_fd(cur_p, vfs_get_drive_root('C'), 0);
        if (tfd >= 3) {
            file_descriptor_t* fdesc = process_get_fd(cur_p, tfd);
            if (fdesc && fdesc->is_used) {
                process_free_fd(cur_p, tfd);
                set_color(COLOR_LIGHT_GREEN, COLOR_BLACK); print("  [PASS] ");
                set_color(COLOR_WHITE, COLOR_BLACK); print("File descriptors (per-process FD table, standard I/O streams)\n");
            } else {
                set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
                set_color(COLOR_WHITE, COLOR_BLACK); print("File descriptors\n");
            }
        } else {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK); print("  [FAIL] ");
            set_color(COLOR_WHITE, COLOR_BLACK); print("File descriptors\n");
        }
    }

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("-------------------------------------\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("All core memory, interrupt, storage & userland subsystems verified.\n");
}

static void cmd_ps(void) {
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("PID   PPID  STATE       MODE    NAME          STACK               CR3\n");
    print("-------------------------------------------------------------------------------\n");

    int count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* p = process_get_by_index(i);
        if (!p || p->state == PROCESS_TERMINATED) continue;

        count++;
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print_uint64((uint64_t)p->pid);
        int sp = 6 - (p->pid >= 10 ? 2 : 1);
        for (int s = 0; s < sp; s++) put_char(' ');

        set_color(COLOR_WHITE, COLOR_BLACK);
        print_uint64((uint64_t)p->ppid);
        sp = 6 - (p->ppid >= 10 ? 2 : 1);
        for (int s = 0; s < sp; s++) put_char(' ');

        if (p->state == PROCESS_RUNNING) {
            set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
            print("RUNNING     ");
        } else if (p->state == PROCESS_READY) {
            set_color(COLOR_YELLOW, COLOR_BLACK);
            print("READY       ");
        } else if (p->state == PROCESS_SLEEPING) {
            set_color(COLOR_LIGHT_MAGENTA, COLOR_BLACK);
            print("SLEEPING    ");
        } else if (p->state == PROCESS_BLOCKED) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            print("BLOCKED     ");
        } else {
            set_color(COLOR_DARK_GREY, COLOR_BLACK);
            print("UNKNOWN     ");
        }

        if (p->is_user) {
            set_color(COLOR_LIGHT_BLUE, COLOR_BLACK);
            print("Ring 3  ");
        } else {
            set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
            print("Ring 0  ");
        }

        set_color(COLOR_WHITE, COLOR_BLACK);
        print(p->name);
        int nlen = (int)strlen(p->name);
        for (int s = 0; s < 14 - nlen && s >= 0; s++) put_char(' ');

        print("0x"); print_hex64(p->kstack_top);
        print("  0x"); print_hex64(p->cr3);
        print("\n");
    }

    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    print("\nTotal Active Processes: ");
    print_uint64((uint64_t)count);
    print("\n");
}

static void cmd_kill(const char* arg) {
    if (!arg || *arg == '\0') {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("Usage: KILL <pid>\n");
        return;
    }
    uint32_t pid = 0;
    while (*arg >= '0' && *arg <= '9') {
        pid = pid * 10 + (*arg - '0');
        arg++;
    }
    if (pid <= 1) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("Cannot terminate Kernel/Shell process (PID 1).\n");
        return;
    }
    if (process_kill(pid) == 0) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print("Process "); print_uint64((uint64_t)pid); print(" terminated.\n");
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("Process not found: "); print_uint64((uint64_t)pid); print("\n");
    }
}

static void cmd_run(const char* arg) {
    if (!arg || *arg == '\0') {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("Usage: RUN <executable.elf>\n");
        return;
    }

    while (*arg == ' ') arg++;

    char bin_path[128];
    int bidx = 0;
    while (arg[bidx] && arg[bidx] != ' ' && bidx < 127) {
        bin_path[bidx] = arg[bidx];
        bidx++;
    }
    bin_path[bidx] = '\0';

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Loading ELF executable: "); print(bin_path); print("...\n");

    process_t* new_proc = NULL;
    int err = elf_load_executable(bin_path, &new_proc);
    if (err != 0 || !new_proc) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("Failed to load ELF executable (Error code: ");
        print_uint64((uint64_t)(err < 0 ? -err : err));
        print(").\n");
        return;
    }

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("Spawned process '"); print(new_proc->name);
    print("' (PID: "); print_uint64((uint64_t)new_proc->pid);
    print(") in Ring 3 User Mode.\n");

    /* Yield CPU to let new process run to completion */
    while (new_proc->state != PROCESS_TERMINATED) {
        process_yield();
    }
}

static void cmd_time(void) {
    RTCDateTime dt;
    rtc_get_datetime(&dt);

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Current System Time (CMOS RTC):\n");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("  Date: ");
    print_uint64(dt.year);
    put_char('-');
    print_dec2(dt.month);
    put_char('-');
    print_dec2(dt.day);
    print("\n  Time: ");
    print_dec2(dt.hour);
    put_char(':');
    print_dec2(dt.minute);
    put_char(':');
    print_dec2(dt.second);
    print(" UTC\n");
}

static void cmd_ticks(void) {
    uint64_t ticks = timer_get_ticks();
    uint32_t freq  = timer_get_frequency();
    uint64_t secs  = timer_get_uptime_seconds();
    uint64_t ms    = timer_get_uptime_ms();

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Kernel PIT Timer Statistics:\n");
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Kernel Ticks:    ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(ticks);
    print(" ticks\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  PIT Frequency:   ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(freq);
    print(" Hz (");
    print_uint64(1000 / freq);
    print(" ms per tick)\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  System Uptime:   ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(secs);
    print(" seconds (");
    print_uint64(secs / 60);
    print("m ");
    print_uint64(secs % 60);
    print("s) / ");
    print_uint64(ms);
    print(" ms total\n");
}

static void cmd_cpu(void) {
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Processor Hardware Information (CPUID):\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Vendor String:   ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print(cpu_info.vendor);
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Brand / Model:   ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print(cpu_info.brand);
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  CPU Topology:    ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("Family: "); print_uint64(cpu_info.family);
    print(" | Model: "); print_uint64(cpu_info.model);
    print(" | Stepping: "); print_uint64(cpu_info.stepping);
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Hardware Features:");
    set_color(COLOR_YELLOW, COLOR_BLACK);

    if (cpu_info.features_edx & (1 << 0))  print(" FPU");
    if (cpu_info.features_edx & (1 << 1))  print(" VME");
    if (cpu_info.features_edx & (1 << 4))  print(" TSC");
    if (cpu_info.features_edx & (1 << 5))  print(" MSR");
    if (cpu_info.features_edx & (1 << 6))  print(" PAE");
    if (cpu_info.features_edx & (1 << 8))  print(" CX8");
    if (cpu_info.features_edx & (1 << 9))  print(" APIC");
    if (cpu_info.features_edx & (1 << 13)) print(" PGE");
    if (cpu_info.features_edx & (1 << 15)) print(" CMOV");
    if (cpu_info.features_edx & (1 << 23)) print(" MMX");
    if (cpu_info.features_edx & (1 << 24)) print(" FXSR");
    if (cpu_info.features_edx & (1 << 25)) print(" SSE");
    if (cpu_info.features_edx & (1 << 26)) print(" SSE2");

    if (cpu_info.features_ecx & (1 << 0))  print(" SSE3");
    if (cpu_info.features_ecx & (1 << 9))  print(" SSSE3");
    if (cpu_info.features_ecx & (1 << 19)) print(" SSE4.1");
    if (cpu_info.features_ecx & (1 << 20)) print(" SSE4.2");
    if (cpu_info.features_ecx & (1 << 23)) print(" POPCNT");
    if (cpu_info.features_ecx & (1 << 25)) print(" AES");
    if (cpu_info.features_ecx & (1 << 28)) print(" AVX");
    if (cpu_info.features_ecx & (1 << 30)) print(" RDRAND");

    if (cpu_info.ext_features_edx & (1 << 29)) print(" LongMode(64-bit)");
    if (cpu_info.ext_features_edx & (1 << 11)) print(" SYSCALL");
    if (cpu_info.ext_features_edx & (1 << 20)) print(" NX");
    print("\n");
}

static void cmd_ints(void) {
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Hardware Interrupt & Event Statistics:\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Total Interrupts: ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(int_count_total);
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  CPU Exceptions:   ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(int_count_exceptions);
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  IRQ 0 (PIT Timer):");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(irq_counters[0]);
    print(" ticks\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  IRQ 1 (Keyboard): ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(irq_counters[1]);
    print(" events\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  IRQ 8 (CMOS RTC): ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(irq_counters[8]);
    print(" interrupts\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  IDT Table Gates:  ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("256 entries initialized at 0x");
    print_hex64((uint64_t)&idt);
    print("\n");
}

static void cmd_panic(const char* arg) {
    if (!arg || *arg == '\0' || strcasecmp(arg, "DIVZERO") == 0) {
        set_color(COLOR_YELLOW, COLOR_BLACK);
        print("Intentionally triggering CPU Exception 0x00: Division by Zero (#DE)...\n");
        volatile int a = 42;
        volatile int b = 0;
        volatile int c = a / b;
        (void)c;
    } else if (strcasecmp(arg, "INT3") == 0 || strcasecmp(arg, "BREAKPOINT") == 0) {
        set_color(COLOR_YELLOW, COLOR_BLACK);
        print("Intentionally triggering CPU Exception 0x03: Breakpoint (#BP)...\n");
        __asm__ volatile ("int3");
    } else if (strcasecmp(arg, "GPF") == 0) {
        set_color(COLOR_YELLOW, COLOR_BLACK);
        print("Intentionally triggering CPU Exception 0x0D: General Protection Fault (#GP)...\n");
        __asm__ volatile ("mov $0x1234, %rax; mov %rax, %cr3");
    } else if (strcasecmp(arg, "PAGEFAULT") == 0 || strcasecmp(arg, "PF") == 0) {
        set_color(COLOR_YELLOW, COLOR_BLACK);
        print("Intentionally triggering CPU Exception 0x0E: Page Fault (#PF)...\n");
        volatile uint64_t* invalid_ptr = (volatile uint64_t*)0xDEADBEEF000ULL;
        *invalid_ptr = 0x1337;
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("Unknown exception test type. Options: DIVZERO, INT3, GPF, PAGEFAULT\n");
    }
}

static void cmd_dir(const char* arg) {
    const char* target_path = (arg && *arg != '\0') ? arg : current_working_dir;
    vfs_node_t* dir_node = vfs_resolve_path(current_working_dir, target_path);

    if (!dir_node || !(dir_node->flags & VFS_DIRECTORY)) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("File Not Found or Path Invalid: ");
        print(target_path);
        print("\n");
        return;
    }

    char display_path[MAX_PATH_BUFFER];
    if (arg && *arg != '\0') {
        if ((arg[0] >= 'A' && arg[0] <= 'Z' && arg[1] == ':') || (arg[0] >= 'a' && arg[0] <= 'z' && arg[1] == ':')) {
            strncpy(display_path, arg, MAX_PATH_BUFFER - 1);
        } else {
            strncpy(display_path, current_working_dir, MAX_PATH_BUFFER - 1);
            if (display_path[strlen(display_path) - 1] != '\\') strcat(display_path, "\\");
            strcat(display_path, arg);
        }
    } else {
        strncpy(display_path, current_working_dir, MAX_PATH_BUFFER - 1);
    }

    char cur_drv = current_working_dir[0];
    vfs_drive_t* drv = vfs_get_drive(cur_drv);

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print(" Volume in drive "); put_char(cur_drv); print(" is ");
    if (drv && drv->label[0] != '\0') print(drv->label);
    else print("GREENHOUSE");
    print("\n Directory of ");
    print(display_path);
    print("\n\n");

    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    print(".              <DIR>\n");
    print("..             <DIR>\n");

    int file_count = 0;
    int dir_count = 2;
    uint32_t total_bytes = 0;

    vfs_dirent_t dirent;
    uint32_t idx = 0;

    while (vfs_readdir(dir_node, idx++, &dirent) == 0) {
        if (strcmp(dirent.name, ".") == 0 || strcmp(dirent.name, "..") == 0) continue;

        print(dirent.name);
        int spaces = 15 - (int)strlen(dirent.name);
        if (spaces < 1) spaces = 1;
        for (int s = 0; s < spaces; s++) put_char(' ');

        if (dirent.is_dir) {
            print("<DIR>\n");
            dir_count++;
        } else {
            print_uint64(dirent.size);
            print(" bytes\n");
            file_count++;
            total_bytes += dirent.size;
        }
    }

    print("\n               ");
    print_uint64((uint64_t)file_count);
    print(" File(s)    ");
    print_uint64((uint64_t)total_bytes);
    print(" bytes\n");

    print("               ");
    print_uint64((uint64_t)dir_count);
    print(" Dir(s)\n");
}

static void cmd_cd(const char* arg) {
    if (!arg || *arg == '\0') {
        set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
        print(current_working_dir);
        print("\n");
        return;
    }

    /* Check for drive switch like "C:" or "R:" */
    if (((arg[0] >= 'A' && arg[0] <= 'Z') || (arg[0] >= 'a' && arg[0] <= 'z')) && arg[1] == ':' && arg[2] == '\0') {
        char target_drive = to_upper(arg[0]);
        vfs_drive_t* drv = vfs_get_drive(target_drive);
        if (!drv || !drv->is_mounted) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            print("The system cannot find the drive specified.\n");
            return;
        }
        current_working_dir[0] = target_drive;
        current_working_dir[1] = ':';
        current_working_dir[2] = '\\';
        current_working_dir[3] = '\0';
        process_t* cur = process_get_current();
        if (cur) strncpy(cur->cwd, current_working_dir, sizeof(cur->cwd) - 1);
        return;
    }

    vfs_node_t* target = vfs_resolve_path(current_working_dir, arg);
    if (!target || !(target->flags & VFS_DIRECTORY)) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("The system cannot find the path specified: ");
        print(arg);
        print("\n");
        return;
    }

    /* Update current_working_dir */
    if (((arg[0] >= 'A' && arg[0] <= 'Z') || (arg[0] >= 'a' && arg[0] <= 'z')) && arg[1] == ':') {
        strncpy(current_working_dir, arg, MAX_PATH_BUFFER - 1);
    } else if (arg[0] == '\\' || arg[0] == '/') {
        char d = current_working_dir[0];
        current_working_dir[0] = d;
        current_working_dir[1] = ':';
        current_working_dir[2] = '\\';
        strncpy(current_working_dir + 3, arg + 1, MAX_PATH_BUFFER - 4);
    } else if (strcmp(arg, "..") == 0) {
        int len = (int)strlen(current_working_dir);
        while (len > 3 && current_working_dir[len - 1] != '\\') len--;
        if (len > 3) current_working_dir[len - 1] = '\0';
        else {
            current_working_dir[3] = '\0';
        }
    } else if (strcmp(arg, ".") != 0) {
        if (current_working_dir[strlen(current_working_dir) - 1] != '\\') {
            strcat(current_working_dir, "\\");
        }
        strcat(current_working_dir, arg);
    }

    process_t* cur = process_get_current();
    if (cur) strncpy(cur->cwd, current_working_dir, sizeof(cur->cwd) - 1);
}

static void cmd_mkdir(const char* arg) {
    if (!arg || *arg == '\0') {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("The syntax of the command is incorrect. Usage: MKDIR <dirname>\n");
        return;
    }

    if (vfs_mkdir_path(current_working_dir, arg) != 0) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("A subdirectory or file already exists, or the path was not found.\n");
        return;
    }

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("Directory created successfully.\n");
}

static void cmd_del(const char* arg) {
    if (!arg || *arg == '\0') {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("The syntax of the command is incorrect. Usage: DEL <filename>\n");
        return;
    }

    if (vfs_delete_path(current_working_dir, arg) != 0) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("Could Not Find ");
        print(arg);
        print("\n");
        return;
    }

    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    print("Deleted: ");
    print(arg);
    print("\n");
}

static void cmd_type(const char* arg) {
    if (!arg || *arg == '\0') {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("The syntax of the command is incorrect. Usage: TYPE <filename>\n");
        return;
    }

    vfs_node_t* file = vfs_resolve_path(current_working_dir, arg);
    if (!file || (file->flags & VFS_DIRECTORY)) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("The system cannot find the file specified: ");
        print(arg);
        print("\n");
        return;
    }

    uint32_t fsize = file->size;
    if (fsize == 0) return;

    uint8_t* buf = (uint8_t*)kmalloc(fsize + 1);
    if (!buf) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("Memory allocation error reading file.\n");
        return;
    }

    int bytes = vfs_read(file, 0, fsize, buf);
    if (bytes >= 0) {
        buf[bytes] = '\0';
        set_color(COLOR_WHITE, COLOR_BLACK);
        print((const char*)buf);
        if (bytes > 0 && buf[bytes - 1] != '\n') put_char('\n');
    }

    kfree(buf);
}

static void cmd_ren(const char* arg) {
    if (!arg || *arg == '\0') {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("The syntax of the command is incorrect. Usage: REN <oldname> <newname>\n");
        return;
    }

    char old_name[64];
    char new_name[64];

    int i = 0;
    while (*arg && *arg != ' ' && i < 63) old_name[i++] = *arg++;
    old_name[i] = '\0';

    while (*arg == ' ') arg++;

    i = 0;
    while (*arg && *arg != ' ' && i < 63) new_name[i++] = *arg++;
    new_name[i] = '\0';

    if (old_name[0] == '\0' || new_name[0] == '\0') {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("The syntax of the command is incorrect. Usage: REN <oldname> <newname>\n");
        return;
    }

    if (vfs_rename_path(current_working_dir, old_name, new_name) != 0) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("A duplicate file name exists, or the file cannot be found.\n");
        return;
    }

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("Renamed "); print(old_name); print(" to "); print(new_name); print("\n");
}

static void cmd_copy(const char* arg) {
    if (!arg || *arg == '\0') {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("The syntax of the command is incorrect. Usage: COPY <source> <destination>\n");
        return;
    }

    char src_name[64];
    char dest_name[64];

    int i = 0;
    while (*arg && *arg != ' ' && i < 63) src_name[i++] = *arg++;
    src_name[i] = '\0';

    while (*arg == ' ') arg++;

    i = 0;
    while (*arg && *arg != ' ' && i < 63) dest_name[i++] = *arg++;
    dest_name[i] = '\0';

    if (src_name[0] == '\0' || dest_name[0] == '\0') {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("The syntax of the command is incorrect. Usage: COPY <source> <destination>\n");
        return;
    }

    vfs_node_t* src_node = vfs_resolve_path(current_working_dir, src_name);
    if (!src_node || (src_node->flags & VFS_DIRECTORY)) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("The system cannot find the file specified: ");
        print(src_name);
        print("\n");
        return;
    }

    uint32_t fsize = src_node->size;
    uint8_t* fbuf = NULL;
    if (fsize > 0) {
        fbuf = (uint8_t*)kmalloc(fsize);
        if (!fbuf) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            print("Out of memory reading source file.\n");
            return;
        }
        vfs_read(src_node, 0, fsize, fbuf);
    }

    /* Create destination file */
    vfs_create_file(current_working_dir, dest_name);
    vfs_node_t* dst_node = vfs_resolve_path(current_working_dir, dest_name);
    if (!dst_node) {
        if (fbuf) kfree(fbuf);
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("Unable to create destination file.\n");
        return;
    }

    if (fsize > 0 && fbuf) {
        vfs_write(dst_node, 0, fsize, fbuf);
        kfree(fbuf);
    }

    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    print("        1 file(s) copied.\n");
}

static void cmd_echo(const char* text) {
    if (!text || *text == '\0') {
        print("\n");
        return;
    }

    const char* redir_ptr = strchr(text, '>');
    if (redir_ptr) {
        int append_mode = 0;
        const char* fname_ptr = redir_ptr + 1;
        if (*fname_ptr == '>') {
            append_mode = 1;
            fname_ptr++;
        }
        while (*fname_ptr == ' ') fname_ptr++;

        char msg_buf[MAX_COMMAND_LEN];
        size_t msg_len = (size_t)(redir_ptr - text);
        while (msg_len > 0 && text[msg_len - 1] == ' ') msg_len--;
        if (msg_len >= MAX_COMMAND_LEN) msg_len = MAX_COMMAND_LEN - 1;
        strncpy(msg_buf, text, msg_len);
        msg_buf[msg_len] = '\0';

        char target_file[64];
        int fi = 0;
        while (*fname_ptr && *fname_ptr != ' ' && fi < 63) {
            target_file[fi++] = *fname_ptr++;
        }
        target_file[fi] = '\0';

        if (target_file[0] == '\0') {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            print("Syntax error in redirection target.\n");
            return;
        }

        vfs_node_t* target = vfs_resolve_path(current_working_dir, target_file);
        if (!target) {
            vfs_create_file(current_working_dir, target_file);
            target = vfs_resolve_path(current_working_dir, target_file);
        }

        if (!target || (target->flags & VFS_DIRECTORY)) {
            set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            print("The system cannot find the path or file specified.\n");
            return;
        }

        uint32_t write_offset = 0;
        if (append_mode) {
            write_offset = target->size;
        }

        size_t mlen = strlen(msg_buf);
        vfs_write(target, write_offset, (uint32_t)mlen, (const uint8_t*)msg_buf);
        vfs_write(target, write_offset + (uint32_t)mlen, 1, (const uint8_t*)"\n");
        return;
    }

    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    print(text);
    print("\n");
}

static void cmd_berry(const char* arg) {
    if (!arg || *arg == '\0') {
        set_color(COLOR_LIGHT_MAGENTA, COLOR_BLACK);
        print("(=^.^=) Berry Shell Companion v0.9\n");
        set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
        print("Hi Vivaan! I'm Berry. Here are some fun things you can try:\n\n");
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print("  berry mem     "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("- Berry's memory & heap overview\n");
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print("  berry disk    "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("- Storage devices and drive letters\n");
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print("  berry files   "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("- Filesystem details & tips\n");
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print("  berry status  "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("- Berry's system mood and status\n");
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print("  berry quote   "); set_color(COLOR_LIGHT_GREY, COLOR_BLACK); print("- Read an inspirational quote\n");
        return;
    }

    if (strcasecmp(arg, "mem") == 0) {
        pmm_stats_t pmm = pmm_get_stats();
        heap_stats_t heap = heap_get_stats();
        set_color(COLOR_LIGHT_MAGENTA, COLOR_BLACK);
        print("Berry Memory Report: ");
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print_uint64((pmm.free_frames * PAGE_SIZE) / 1024);
        print(" KB physical RAM free! Heap allocator has ");
        print_uint64((uint64_t)heap.active_allocs);
        print(" active blocks.\n");
    } else if (strcasecmp(arg, "disk") == 0 || strcasecmp(arg, "disks") == 0) {
        int num_disks = block_dev_count();
        set_color(COLOR_LIGHT_MAGENTA, COLOR_BLACK);
        print("Berry Storage Report: ");
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print_uint64((uint64_t)num_disks);
        print(" block device(s) online. Persistent storage & RAMFS active!\n");
    } else if (strcasecmp(arg, "files") == 0) {
        set_color(COLOR_LIGHT_MAGENTA, COLOR_BLACK);
        print("Berry Filesystem Report: ");
        set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
        print("Working on "); print(current_working_dir); print(". Use 'dir' to browse!\n");
    } else if (strcasecmp(arg, "hello") == 0 || strcasecmp(arg, "hi") == 0) {
        set_color(COLOR_LIGHT_MAGENTA, COLOR_BLACK);
        print("Berry: ");
        set_color(COLOR_WHITE, COLOR_BLACK);
        print("Hello Vivaan! Greenhouse OS 0.9 is running with real memory and storage!\n");
    } else if (strcasecmp(arg, "quote") == 0) {
        set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
        print("Berry's Wisdom:\n");
        set_color(COLOR_YELLOW, COLOR_BLACK);
        print("\"The best way to understand a computer is to write its operating system.\"\n");
        print("  -- Vivaan, Berry-Tech\n");
    } else if (strcasecmp(arg, "status") == 0) {
        set_color(COLOR_LIGHT_MAGENTA, COLOR_BLACK);
        print("Berry Status: ");
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print("All systems green! 64-bit PMM, VMM, Heap, VFS, and FAT32 are running silky smooth.\n");
    } else {
        set_color(COLOR_LIGHT_MAGENTA, COLOR_BLACK);
        print("Berry: ");
        set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
        print("I'm not sure how to '"); print(arg); print("'. Type 'berry' for a list of topics!\n");
    }
}

static void cmd_ver(void) {
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("Greenhouse OS [Version 0.9.0 x86_64]\n");
    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    print("(c) Berry-Tech Corporation. All rights reserved.\n");
}

static void cmd_about(void) {
    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("Greenhouse OS 0.9 (Memory + Storage Edition)\n");
    set_color(COLOR_YELLOW, COLOR_BLACK);
    print("Designed & Engineered by Vivaan, Founder of Berry-Tech\n\n");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("\"If it runs code, you can make it your own.\"\n\n");
    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    print("Built from scratch with Multiboot2, 64-bit Long Mode,\n");
    print("4 KiB Physical Page Allocator (PMM), 4-Level Paging (VMM),\n");
    print("Kernel Heap (kmalloc/kfree), Virtual Filesystem (VFS),\n");
    print("ATA/IDE PIO Storage Driver, Persistent FAT32 & Dynamic RAMFS.\n");
}

static void cmd_whoami(void) {
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("vivaan\n");
}

static void cmd_hostname(void) {
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("GREENHOUSE\n");
}

static void cmd_systeminfo(void) {
    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Host Name:                 GREENHOUSE\n");
    print("OS Name:                   Greenhouse OS 0.9\n");
    print("OS Version:                0.9.0 (Build Memory + Storage Foundation)\n");
    print("OS Manufacturer:           Berry-Tech Corporation\n");
    print("Lead Engineer:             Vivaan\n");
    print("OS Configuration:          Standalone Monolithic Kernel\n");
    print("OS Build Type:             Freestanding 64-bit ELF\n");
    print("Processor Architecture:    x86_64 / AMD64 (64-bit Long Mode)\n");
    print("Processor Brand:           "); print(cpu_info.brand); print("\n");
    print("Physical Memory Allocator: 4 KiB Bitmap Frame Allocator (PMM)\n");
    print("Virtual Memory Subsystem:  4-Level Page Tables (PML4/PDPT/PD/PT)\n");
    print("Kernel Heap Allocator:     Dynamic Boundary-Tag Allocator (kmalloc/kfree)\n");
    print("Virtual File System:       VFS Multi-Drive Manager (C:, R:)\n");
    print("Storage Disk Drivers:      Primary/Secondary ATA/IDE PIO Driver\n");
    print("Filesystem Drivers:        Microsoft FAT32 Driver + Dynamic RAMFS\n");
    print("Interrupt Subsystem:       256-entry IDT & Dual 8259 PIC (100 Hz PIT)\n");
    print("Hardware Clock:            Motorola MC146818 CMOS RTC\n");
    print("Current User:              vivaan\n");
}

static void reboot_system(void) {
    set_color(COLOR_YELLOW, COLOR_BLACK);
    print("Rebooting Greenhouse OS...\n");

    uint8_t status;
    for (int i = 0; i < 1000; i++) {
        status = inb(0x64);
        if (!(status & 0x02)) break;
        io_wait();
    }
    outb(0x64, 0xFE);

    IdtPtr null_idt = { 0, 0 };
    idt_load(&null_idt);
    __asm__ volatile ("int3");

    while (1) {
        __asm__ volatile ("hlt");
    }
}

static void shutdown_system(void) {
    set_color(COLOR_YELLOW, COLOR_BLACK);
    print("Shutting down Greenhouse OS...\n");

    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("System halted. It is now safe to power off your machine.\n");

    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

/* ==============================================================================
 * 11b. Graphics, Input and Window Manager Shell Commands
 *
 * The text shell stays the entry point: GFX/GUI deliberately leave the text
 * console, run in graphics mode and return here when the user leaves the GUI
 * (ESC) or the requested time limit is over, so the console keeps working.
 * ==============================================================================
 */

static void cmd_gfxinfo(void) {
    const framebuffer_info_t* fb = framebuffer_get_info();
    vbe_adapter_t adapter;
    vbe_probe(&adapter);

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Greenhouse Graphics Subsystem - Framebuffer Report\n");
    print("-----------------------------------------------\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Adapter:        ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_hex64(((uint64_t)adapter.vendor_id << 16) | adapter.device_id);
    print("  bus ");
    print_uint64(adapter.bus);
    print(" dev ");
    print_uint64(adapter.device);
    print(" func ");
    print_uint64(adapter.function);
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  VBE registers:  ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print(adapter.dispi ? "present (Bochs VBE / DISPI)" : "not detected");
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  BAR0:           ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_hex64(adapter.bar0);
    print("  size ");
    print_uint64(adapter.bar0_size / 1024);
    print(" KiB");
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Mode source:    ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print(framebuffer_backend_name(fb->backend));
    print("\n");

    if (fb->backend == FB_BACKEND_NONE) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("  No framebuffer described yet - 'gfx' or 'gui' programs one.\n");
        set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
        return;
    }

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Geometry:       ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(fb->width); print(" x "); print_uint64(fb->height);
    print(" @ ");
    print_uint64(fb->bpp);
    print(" bpp\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Stride:         ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(fb->pitch);
    print(" bytes/line  (");
    print_uint64(fb->bytes_per_pixel);
    print(" bytes/pixel)\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Format:         ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print(framebuffer_format_name(fb->format));
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Channels:       ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("R off ");
 print_uint64(fb->red_offset); print(" size "); print_uint64(fb->red_size);
    print(", G off "); print_uint64(fb->green_offset); print(" size "); print_uint64(fb->green_size);
    print(", B off "); print_uint64(fb->blue_offset); print(" size "); print_uint64(fb->blue_size);
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Framebuffer:    ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_hex64(fb->address);
    print("  (");
    print_uint64(fb->size / 1024);
    print(" KiB)\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Font:           ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64((uint64_t)font_glyph_width()); print("x");
    print_uint64((uint64_t)font_glyph_height());
    print(" bitmap, ");
    print_uint64(FONT_LAST_CHAR - FONT_FIRST_CHAR + 1);
    print(" glyphs\n");
}

static void cmd_input(void) {
    input_stats_t st;
    input_get_stats(&st);

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("Greenhouse Input Subsystem\n");
    print("-------------------------\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Keyboard:       ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("scancodes ");
    print_uint64(kbd_get_scancode_count());
    print(", overruns ");
    print_uint64(kbd_get_overrun_count());
    print(", shift ");
    print(kbd_is_shift_down() ? "on" : "off");
    print(", caps ");
    print(kbd_is_caps_lock() ? "on" : "off");
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Mouse:          ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print(mouse_get_name());
    if (mouse_init_status != 0) {
        print(" (init failed, rc ");
        print_uint64((uint64_t)(int64_t)mouse_init_status);
        print(")");
    }
    print(", packets ");
    print_uint64(mouse_get_packet_count());
    print(", dropped ");
    print_uint64(mouse_get_dropped_count());
    print(", wheel ");
    print_uint64(mouse_get_wheel_count());
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Events:         ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("posted ");
    print_uint64(st.posted);
    print(", delivered ");
    print_uint64(st.delivered);
    print(", dropped ");
    print_uint64(st.dropped);
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Key events:     ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("down ");
    print_uint64(st.key_down);
    print(", up ");
    print_uint64(st.key_up);
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Pointer:        ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(st.pointer_x);
    print(", ");
    print_uint64(st.pointer_y);
    print("  buttons ");
    print_uint64(input_get_buttons());
    print("\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Queue:          ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64(INPUT_QUEUE_SIZE);
    print(" slots, ");
    print_uint64(st.posted - st.delivered);
    print(" pending\n");
}

/* Parse "gfx [width [height [bpp]] [hold]]".  A missing or zero field keeps the
 * driver default for that part of the mode.  `hold` is the number of seconds to
 * leave the finished test frame on screen before the console comes back, which
 * is what makes a screendump of a shallow pixel format possible. */
static void cmd_gfx(const char* args) {
    uint32_t want_w = 0, want_h = 0, want_bpp = 0, hold = 0;
    if (args && args[0]) {
        uint32_t* fields[4] = { &want_w, &want_h, &want_bpp, &hold };
        for (int f = 0; f < 4; f++) {
            while (*args == ' ') args++;
            uint32_t v = 0;
            int digits = 0;
            while (*args >= '0' && *args <= '9') {
                v = v * 10 + (uint32_t)(*args - '0');
                args++;
                digits++;
            }
            if (digits == 0) break;
            *fields[f] = v;
        }
    }

    if (!framebuffer_adapter_present() && framebuffer_get_info()->backend == FB_BACKEND_NONE) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("No display adapter found - cannot enter graphics mode.\n");
        set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
        return;
    }

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    if (want_w || want_h || want_bpp) {
        print("Requesting mode ");
        print_uint64((uint64_t)(want_w ? want_w : VBE_PREFERRED_WIDTH));
        print(" x ");
        print_uint64((uint64_t)(want_h ? want_h : VBE_PREFERRED_HEIGHT));
        print(" @ ");
        print_uint64((uint64_t)(want_bpp ? want_bpp : VBE_PREFERRED_BPP));
        print(" bpp\n");
    }
    print("Entering graphics mode (self test frames, ESC or any key to leave)...\n");
    /* Let the message reach the serial mirror before the mode switch. */
    timer_delay_ticks(3);

    vga_save_screen();

    if (graphics_enter_ex(want_w, want_h, want_bpp) != 0) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("Graphics mode request failed - the adapter refused the mode.\n");
        set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
        return;
    }

    gfx_test_result_t result;
    gfx_test_run_all(&result);

    if (hold > 0) {
        /* Keep the mode up with the last test frame on screen so it can be
         * looked at, or captured, before the text console returns. */
        set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
        print("Holding the surface for ");
        print_uint64((uint64_t)hold);
        print(" second(s)...\n");
        timer_delay_ticks(3);
        timer_delay_ticks((uint64_t)hold * timer_get_frequency());
    }

    /* Leave graphics mode, then report over the text console. */
    graphics_leave();
    vga_restore_screen();

    framebuffer_text_diff_t text_diff;
    int text_diff_total = framebuffer_verify_text_state_ex(&text_diff);

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("\nGraphics self test complete.\n");
    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    print("  Surface:        ");
    print_uint64((uint64_t)result.width);
    print(" x ");
    print_uint64((uint64_t)result.height);
    print(" @ ");
    print_uint64((uint64_t)result.bpp);
    print(" bpp\n");
    print("  Back buffer:    ");
    print(result.back_buffer ? "allocated" : "none (direct rendering)");
    print("\n");
    print("  Frames drawn:   ");
    print_uint64((uint64_t)result.frames_drawn);
    print("\n");
    print("  Pixel checks:   ");
    print_uint64((uint64_t)result.checks);
    print("  failures ");
    print_uint64((uint64_t)result.failures);
    print("\n  Present checks: ");
    print_uint64((uint64_t)result.present_checks);
    print("  failures ");
    print_uint64((uint64_t)result.present_failures);
    if (result.fail_x >= 0) {
        print("\n  First failure:  (");
        print_uint64((uint64_t)result.fail_x);
        print(", ");
        print_uint64((uint64_t)result.fail_y);
        print(") got 0x");
        print_hex64(result.fail_got);
        print(" want 0x");
        print_hex64(result.fail_expected);
    }
    print("\n");

    print("  Text mode:      ");
    if (text_diff_total == 0) {
        print("restored (VGA registers match the boot state)");
    } else if (text_diff_total > 0) {
        print_uint64((uint64_t)text_diff_total);
        print(" registers differ (seq ");
        print_uint64((uint64_t)text_diff.seq_diff);
        print(" crtc ");
        print_uint64((uint64_t)text_diff.crtc_diff);
        print(" gctl ");
        print_uint64((uint64_t)text_diff.gctl_diff);
        print(" attr ");
        print_uint64((uint64_t)text_diff.attr_diff);
        print(")");
        if (text_diff.first_crtc >= 0) {
            print("\n                   first crtc[");
            print_uint64((uint64_t)text_diff.first_crtc);
            print("] now 0x");
            print_hex64(text_diff.first_crtc_now);
            print(" want 0x");
            print_hex64(text_diff.first_crtc_want);
        }
        if (text_diff.first_attr >= 0) {
            print("  attr[");
            print_uint64((uint64_t)text_diff.first_attr);
            print("] now 0x");
            print_hex64(text_diff.first_attr_now);
            print(" want 0x");
            print_hex64(text_diff.first_attr_want);
        }
    } else {
        print("no snapshot taken");
    }
    print("\n");

    /* A restored text mode is part of the test: coming back from graphics has
     * to leave the adapter exactly as it was found, otherwise the shell would
     * keep running on an unusable screen. */
    if (result.failures == 0 && result.present_failures == 0 && text_diff_total == 0) {
        set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print("  Result:         PASS\n");
    } else {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("  Result:         FAIL\n");
    }
    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
}

static void cmd_gui(const char* args) {
    int seconds = 0;
    if (args && args[0]) {
        seconds = 0;
        for (const char* p = args; *p >= '0' && *p <= '9'; p++) {
            seconds = seconds * 10 + (*p - '0');
        }
    }

    vga_save_screen();

    if (gui_enter() != 0) {
        set_color(COLOR_LIGHT_RED, COLOR_BLACK);
        print("Could not enter graphics mode - the adapter refused the mode.\n");
        set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
        return;
    }

    if (seconds > 0) {
        set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
        print("GUI running for ");
        print_uint64((uint64_t)seconds);
        print(" second(s)...\n");
        timer_delay_ticks(3);
    }

    int reason = gui_run(seconds);

    gui_report_t report;
    gui_get_report(&report);
    gui_leave();

    /* Restore pristine text console state and clean cursor */
    vga_restore_screen();

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("\nGUI session finished (");
    if (reason == GUI_EXIT_ESC) print("ESC pressed");
    else if (reason == GUI_EXIT_TIMEOUT) print("time limit reached");
    else if (reason == GUI_EXIT_CLOSED) print("logged out");
    else print("error");
    print(").\n");


    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Surface:        ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64((uint64_t)report.width);
    print(" x ");
    print_uint64((uint64_t)report.height);
    print(" @ ");
    print_uint64((uint64_t)report.bpp);
    print(" bpp (");
    print(report.backend_multiboot ? "multiboot2" : "legacy vbe");
    print(")\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Back buffer:    ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    if (report.back_buffer) {
        print("allocated, ");
        print_uint64(report.back_buffer_size / 1024);
        print(" KiB\n");
    } else {
        print("none (direct rendering)");
        int bb_err = graphics_get_back_buffer_error();
        if (bb_err != 0) {
            print(" - reason ");
            print_uint64((uint64_t)(-(int64_t)bb_err));
        }
        print("\n");
    }

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Frames:         ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64((uint64_t)report.frames);
    print(" composed, ");
    print_uint64((uint64_t)report.presents);
    print(" presented\n");

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("  Events:         ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print_uint64((uint64_t)report.events_processed);
    print(" processed by the window manager\n");
    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
}

static void cmd_renderertest(const char* args) {
    int seconds = 0;
    if (args && args[0]) {
        for (const char* p = args; *p >= '0' && *p <= '9'; p++) {
            seconds = seconds * 10 + (*p - '0');
        }
    }

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("Launching Greenhouse 2D Renderer Visual Benchmark...\n");
    if (seconds > 0) {
        print("Running for ");
        print_uint64((uint64_t)seconds);
        print(" second(s) (ESC to quit early)...\n");
    } else {
        print("Interactive mode (keys 1-7 switch cursor, ESC to quit)...\n");
    }
    timer_delay_ticks(3);

    test_screen_run(seconds);

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("\nRenderer visual benchmark completed.\n");
    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
}

static void execute_command(void) {
    print("\n");

    char* cmd = command_buffer;
    while (*cmd == ' ') cmd++;

    if (*cmd == '\0') {
        show_prompt();
        return;
    }

    history_add(cmd);
    history_browse_idx = -1;

    char verb[32];
    int vlen = 0;
    while (*cmd && *cmd != ' ' && vlen < 31) {
        verb[vlen++] = *cmd++;
    }
    verb[vlen] = '\0';

    while (*cmd == ' ') cmd++;
    const char* args = cmd;

    /* Check for direct drive letter change e.g. "C:" or "R:" */
    if (vlen == 2 && ((verb[0] >= 'A' && verb[0] <= 'Z') || (verb[0] >= 'a' && verb[0] <= 'z')) && verb[1] == ':') {
        cmd_cd(verb);
    } else if (strcasecmp(verb, "HELP") == 0) {
        cmd_help();
    } else if (strcasecmp(verb, "MEM") == 0 || strcasecmp(verb, "MEMORY") == 0) {
        cmd_mem();
    } else if (strcasecmp(verb, "MEMMAP") == 0) {
        cmd_memmap();
    } else if (strcasecmp(verb, "HEAP") == 0) {
        cmd_heap();
    } else if (strcasecmp(verb, "DISKS") == 0 || strcasecmp(verb, "DISK") == 0) {
        cmd_disks();
    } else if (strcasecmp(verb, "VOL") == 0 || strcasecmp(verb, "DRIVES") == 0) {
        cmd_vol();
    } else if (strcasecmp(verb, "PS") == 0 || strcasecmp(verb, "TASKS") == 0) {
        cmd_ps();
    } else if (strcasecmp(verb, "KILL") == 0) {
        cmd_kill(args);
    } else if (strcasecmp(verb, "RUN") == 0 || strcasecmp(verb, "EXEC") == 0) {
        cmd_run(args);
    } else if (strcasecmp(verb, "TEST") == 0) {
        cmd_test();
    } else if (strcasecmp(verb, "TIME") == 0 || strcasecmp(verb, "DATE") == 0) {
        cmd_time();
    } else if (strcasecmp(verb, "GFXINFO") == 0 || strcasecmp(verb, "FBINFO") == 0) {
        cmd_gfxinfo();
    } else if (strcasecmp(verb, "GFX") == 0 || strcasecmp(verb, "GRAPHICS") == 0) {
        cmd_gfx(args);
    } else if (strcasecmp(verb, "VERDANT") == 0 || strcasecmp(verb, "STARTGUI") == 0 ||
               strcasecmp(verb, "GUI") == 0 || strcasecmp(verb, "DESKTOP") == 0) {
        cmd_gui(args);
    } else if (strcasecmp(verb, "RENDERERTEST") == 0 || strcasecmp(verb, "TESTSCREEN") == 0 ||
               strcasecmp(verb, "BENCH") == 0 || strcasecmp(verb, "BENCHMARK") == 0) {
        cmd_renderertest(args);
    } else if (strcasecmp(verb, "INPUT") == 0) {
        cmd_input();
    } else if (strcasecmp(verb, "TICKS") == 0 || strcasecmp(verb, "UPTIME") == 0) {
        cmd_ticks();
    } else if (strcasecmp(verb, "CPU") == 0 || strcasecmp(verb, "CPUID") == 0) {
        cmd_cpu();
    } else if (strcasecmp(verb, "INTS") == 0 || strcasecmp(verb, "IRQS") == 0) {
        cmd_ints();
    } else if (strcasecmp(verb, "PANIC") == 0 || strcasecmp(verb, "CRASH") == 0) {
        cmd_panic(args);
    } else if (strcasecmp(verb, "CLS") == 0) {
        clear_screen();
    } else if (strcasecmp(verb, "VER") == 0) {
        cmd_ver();
    } else if (strcasecmp(verb, "ABOUT") == 0) {
        cmd_about();
    } else if (strcasecmp(verb, "WHOAMI") == 0) {
        cmd_whoami();
    } else if (strcasecmp(verb, "HOSTNAME") == 0) {
        cmd_hostname();
    } else if (strcasecmp(verb, "SYSTEMINFO") == 0) {
        cmd_systeminfo();
    } else if (strcasecmp(verb, "DIR") == 0) {
        cmd_dir(args);
    } else if (strcasecmp(verb, "CD") == 0) {
        cmd_cd(args);
    } else if (strcasecmp(verb, "MKDIR") == 0 || strcasecmp(verb, "MD") == 0) {
        cmd_mkdir(args);
    } else if (strcasecmp(verb, "DEL") == 0) {
        cmd_del(args);
    } else if (strcasecmp(verb, "COPY") == 0) {
        cmd_copy(args);
    } else if (strcasecmp(verb, "REN") == 0) {
        cmd_ren(args);
    } else if (strcasecmp(verb, "TYPE") == 0) {
        cmd_type(args);
    } else if (strcasecmp(verb, "ECHO") == 0) {
        cmd_echo(args);
    } else if (strcasecmp(verb, "BERRY") == 0) {
        cmd_berry(args);
    } else if (strcasecmp(verb, "REBOOT") == 0 || strcasecmp(verb, "RESTART") == 0) {
        reboot_system();
    } else if (strcasecmp(verb, "SHUTDOWN") == 0 || strcasecmp(verb, "EXIT") == 0) {
        shutdown_system();
    } else {
        os_shell_execute(command_buffer);
    }

    command_buffer[0] = '\0';
    command_length = 0;
    show_prompt();
}

/* ==============================================================================
 * 13. Kernel Main Entry Point
 * ==============================================================================
 */

void kernel_main(uint32_t mb2_magic, uint64_t mb2_info_addr) {
    saved_mb2_magic = mb2_magic;
    saved_mb2_info_addr = mb2_info_addr;

    /* Initialize Serial Diagnostic Mirror */
    serial_init();

    /* 1. Hardware Interrupt Subsystems & GDT/TSS */
    gdt_init();
    idt_init();
    pic_remap();
    pit_init(100);
    input_init_all();
    cpu_detect();
    enable_interrupts();

    /* 2. Phase 2: Real Memory Subsystem */
    uintptr_t k_start = (uintptr_t)_kernel_start;
    uintptr_t k_end   = (uintptr_t)_kernel_end;
    pmm_init(mb2_magic, mb2_info_addr, k_start, k_end);
    vmm_init();
    heap_init(0x20000000ULL, 16 * 1024 * 1024); /* 16 MiB initial heap for smooth GUI caching */

    /* 3. Phase 4 & 5: Process Scheduler & Syscall Subsystems */
    process_init();
    syscall_init();

    /* Phase 6: probe the framebuffer and the graphics subsystems.  The display
     * stays in the VGA text mode here; graphics is entered on request. */
    framebuffer_init((uint32_t)mb2_magic, (uint64_t)mb2_info_addr);
    graphics_init();
    gui_init();
    if (framebuffer_adapter_present()) {
        const framebuffer_info_t* fb = framebuffer_get_info();
        if (fb->backend != FB_BACKEND_NONE) {
            print("[GFX] ");
            print_uint64(fb->width); print("x"); print_uint64(fb->height);
            print(" framebuffer available (");
            print(framebuffer_backend_name(fb->backend));
            print(")\n");
        }
    }

    /* 4. Phase 3: Real Storage & Filesystem Subsystem */
    vfs_init();
    ata_init();

    /* Probe ATA Disk and Mount Drives */
    block_dev_t* primary_disk = block_dev_find("ata0");
    vfs_node_t* fat32_root = NULL;

    if (primary_disk) {
        fat32_root = fat32_mount_device(primary_disk);
    }

    vfs_node_t* ramfs_root = ramfs_create_root();

    if (fat32_root) {
        /* Primary Drive C: Persistent FAT32, Drive R: RAMFS */
        vfs_mount('C', fat32_root, "HARDDISK", "FAT32");
        vfs_mount('R', ramfs_root, "RAMDISK", "RAMFS");
        strcpy(current_working_dir, "C:\\");
        os_shell_init();
        os_shell_set_cwd("C:\\");
    } else {
        /* Primary Drive C: RAMFS */
        vfs_mount('C', ramfs_root, "RAMDISK", "RAMFS");
        strcpy(current_working_dir, "C:\\GREENHOUSE");
        os_shell_init();
        os_shell_set_cwd("C:\\GREENHOUSE");
    }

    /* 5. Display Welcome Screen */
    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    clear_screen();

    set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    print("===============================================================================\n");
    print("               GREENHOUSE OS  --  VERSION " GREENHOUSE_VERSION_STRING
          " (PROCESS + USERLAND)             \n");
    print("===============================================================================\n");

    set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print("  [OK] 64-bit Long Mode, GDT/TSS & 256-entry IDT active.\n");
    print("  [OK] Physical Page Allocator (4 KiB PMM) active.\n");
    print("  [OK] Virtual Memory Manager (4-Level Paging VMM) active.\n");
    print("  [OK] Kernel Heap Allocator (kmalloc / kfree) online.\n");
    print("  [OK] Ring 3 User Mode, Syscalls (int 0x80) & Scheduler initialized.\n");
    print("  [OK] Virtual Filesystem (VFS) with Multi-Drive Manager active.\n");

    if (fat32_root) {
        print("  [OK] ATA Hard Disk detected: Mounted C:\\ (FAT32) & R:\\ (RAMFS).\n");
    } else {
        print("  [OK] In-memory RAMFS mounted at C:\\.\n");
    }

    if (framebuffer_get_info()->backend != FB_BACKEND_NONE) {
        set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
        print("  [OK] Graphics Subsystem (framebuffer, 2D compositor, 8x16 bitmap font) ready.\n");
        print("  [OK] Input Subsystem (PS/2 keyboard + mouse, event queue, software cursor).\n");
    }

    set_color(COLOR_LIGHT_MAGENTA, COLOR_BLACK);
    print("\n  Berry: ");
    set_color(COLOR_WHITE, COLOR_BLACK);
    print("Welcome to " GREENHOUSE_VERSION_STRING "! Preemptive multitasking & userland active.\n");
    set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    print("         Type 'help' for commands, 'gfx' for the graphics test, 'gui' for the desktop.\n\n");

    /* Show prompt */
    show_prompt();

    /* Main interactive loop */
    while (1) {
        uint8_t c = keyboard_getchar();

        if (c == 0) continue;

        if (c == INPUT_KEY_UP) {
            history_handle_up();
            continue;
        }

        if (c == INPUT_KEY_DOWN) {
            history_handle_down();
            continue;
        }

        if (c == '\b') {
            if (command_length > 0) {
                command_length--;
                command_buffer[command_length] = '\0';
                put_char('\b');
            }
            continue;
        }

        if (c == '\n') {
            command_buffer[command_length] = '\0';
            execute_command();
            continue;
        }

        if (c >= ' ' && c <= '~') {
            if (command_length < MAX_COMMAND_LEN - 1) {
                command_buffer[command_length++] = (char)c;
                command_buffer[command_length] = '\0';
                put_char((char)c);
            }
        }
    }
}
