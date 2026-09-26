#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "vfs.h"

#define MAX_PROCESSES   32
#define MAX_FD          16
#define KSTACK_SIZE     16384 /* 16 KiB Kernel Stack */
#define USTACK_SIZE     16384 /* 16 KiB User Stack */
#define USER_STACK_TOP  0x0000000040004000ULL

typedef enum {
    PROCESS_READY = 0,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_SLEEPING,
    PROCESS_TERMINATED
} process_state_t;

/* Standard x86_64 Interrupt Frame saved by ISR stub */
typedef struct {
    /* Pushed by isr_common_stub */
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

    /* Pushed by ISR macro */
    uint64_t int_no;
    uint64_t err_code;

    /* Pushed automatically by x86_64 CPU on interrupt */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) interrupt_frame_t;

typedef struct {
    vfs_node_t* node;
    uint32_t offset;
    int flags;
    int is_used;
} file_descriptor_t;

typedef struct process {
    uint32_t pid;
    uint32_t ppid;
    char name[32];
    process_state_t state;
    int is_user;

    uintptr_t kstack_base;
    uintptr_t kstack_top;
    uintptr_t ustack_top;
    uintptr_t cr3;            /* PML4 Physical Address */
    interrupt_frame_t* saved_frame;

    int exit_code;
    uint64_t sleep_until_tick;
    char cwd[64];

    file_descriptor_t fds[MAX_FD];
} process_t;

void process_init(void);
process_t* process_create(const char* name, uintptr_t entry_point, int is_user, uintptr_t pml4_phys, uintptr_t ustack_top);
process_t* process_get_current(void);
process_t* process_get_by_pid(uint32_t pid);
int process_count(void);
process_t* process_get_by_index(size_t index);

uint64_t process_schedule(interrupt_frame_t* frame);
void process_wake_sleepers(uint64_t current_tick);
void process_yield(void);
void process_sleep(uint64_t ticks);
void process_exit(int code);
int process_kill(uint32_t pid);

int process_alloc_fd(process_t* proc, vfs_node_t* node, int flags);
int process_free_fd(process_t* proc, int fd);
file_descriptor_t* process_get_fd(process_t* proc, int fd);

#endif /* PROCESS_H */
