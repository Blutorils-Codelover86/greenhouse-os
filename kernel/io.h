/* ==============================================================================
 * Greenhouse OS - Low Level Port I/O Primitives
 * Shared by the graphics, input and window manager drivers so that hardware
 * access is written in exactly one place.
 * ==============================================================================
 */

#ifndef IO_H
#define IO_H

#include <stdint.h>
#include <stddef.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile ("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t result;
    __asm__ volatile ("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void cpu_hlt(void) {
    __asm__ volatile ("hlt");
}

static inline void cpu_cli(void) {
    __asm__ volatile ("cli");
}

static inline void cpu_sti(void) {
    __asm__ volatile ("sti");
}

static inline void pause_cpu(void) {
    __asm__ volatile ("pause");
}

/* Bounded spin helper: returns non-zero when the predicate became true. */
static inline int spin_until(int (*pred)(void), uint32_t max_spins) {
    for (uint32_t i = 0; i < max_spins; i++) {
        if (pred()) return 1;
        pause_cpu();
    }
    return 0;
}

#endif /* IO_H */
