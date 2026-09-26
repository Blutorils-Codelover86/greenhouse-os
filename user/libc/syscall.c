#include "syscall.h"

int64_t syscall0(uint64_t number) {
    int64_t ret;
    __asm__ volatile (
        "movq %1, %%rax\n\t"
        "int $0x80\n\t"
        "movq %%rax, %0"
        : "=r"(ret)
        : "r"(number)
        : "rax", "rcx", "r11", "memory"
    );
    return ret;
}

int64_t syscall1(uint64_t number, uint64_t a1) {
    int64_t ret;
    __asm__ volatile (
        "movq %1, %%rax\n\t"
        "movq %2, %%rdi\n\t"
        "int $0x80\n\t"
        "movq %%rax, %0"
        : "=r"(ret)
        : "r"(number), "r"(a1)
        : "rax", "rdi", "rcx", "r11", "memory"
    );
    return ret;
}

int64_t syscall2(uint64_t number, uint64_t a1, uint64_t a2) {
    int64_t ret;
    __asm__ volatile (
        "movq %1, %%rax\n\t"
        "movq %2, %%rdi\n\t"
        "movq %3, %%rsi\n\t"
        "int $0x80\n\t"
        "movq %%rax, %0"
        : "=r"(ret)
        : "r"(number), "r"(a1), "r"(a2)
        : "rax", "rdi", "rsi", "rcx", "r11", "memory"
    );
    return ret;
}

int64_t syscall3(uint64_t number, uint64_t a1, uint64_t a2, uint64_t a3) {
    int64_t ret;
    __asm__ volatile (
        "movq %1, %%rax\n\t"
        "movq %2, %%rdi\n\t"
        "movq %3, %%rsi\n\t"
        "movq %4, %%rdx\n\t"
        "int $0x80\n\t"
        "movq %%rax, %0"
        : "=r"(ret)
        : "r"(number), "r"(a1), "r"(a2), "r"(a3)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
    return ret;
}

int64_t syscall4(uint64_t number, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    int64_t ret;
    __asm__ volatile (
        "movq %1, %%rax\n\t"
        "movq %2, %%rdi\n\t"
        "movq %3, %%rsi\n\t"
        "movq %4, %%rdx\n\t"
        "movq %5, %%r10\n\t"
        "int $0x80\n\t"
        "movq %%rax, %0"
        : "=r"(ret)
        : "r"(number), "r"(a1), "r"(a2), "r"(a3), "r"(a4)
        : "rax", "rdi", "rsi", "rdx", "r10", "rcx", "r11", "memory"
    );
    return ret;
}

int64_t syscall5(uint64_t number, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    int64_t ret;
    __asm__ volatile (
        "movq %1, %%rax\n\t"
        "movq %2, %%rdi\n\t"
        "movq %3, %%rsi\n\t"
        "movq %4, %%rdx\n\t"
        "movq %5, %%r10\n\t"
        "movq %6, %%r8\n\t"
        "int $0x80\n\t"
        "movq %%rax, %0"
        : "=r"(ret)
        : "r"(number), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5)
        : "rax", "rdi", "rsi", "rdx", "r10", "r8", "rcx", "r11", "memory"
    );
    return ret;
}
