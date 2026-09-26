/* ==============================================================================
 * Greenhouse OS - Interrupt Controller Interface
 *
 * The IDT/PIC implementation itself still lives in kernel.c (it is the first
 * subsystem brought up).  This header publishes the small API that hardware
 * drivers (keyboard, PS/2 mouse) need so they do not have to know anything
 * about the kernel monolith internals.
 * ==============================================================================
 */

#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>
#include <stddef.h>

/* Register image pushed by the ISR stubs in kernel/interrupts.asm.
 * The layout is identical to interrupt_frame_t in process.h (RIP is the
 * instruction pointer saved by the CPU, the register order is fixed by
 * isr_common_stub). */
typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t int_no;
    uint64_t err_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) irq_registers_t;

typedef void (*irq_handler_t)(irq_registers_t* regs);

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

void pic_send_eoi(uint8_t irq);
void irq_install_handler(uint8_t irq, irq_handler_t handler);
void pic_set_irq_mask(uint8_t irq, int masked);

uint64_t timer_get_ticks(void);
uint32_t timer_get_frequency(void);

/* Block for a number of PIT ticks. Unlike a busy-wait over a calibrated loop
 * count this is real time: the same pause lasts the same wall time on a fast
 * host, under emulation, or on whatever machine the ISO lands on. */
void timer_delay_ticks(uint64_t ticks);

#endif /* IRQ_H */
