#include "process.h"
#include "gdt.h"
#include "vmm.h"
#include "pmm.h"
#include "heap.h"

static process_t process_table[MAX_PROCESSES];
static process_t* current_proc = NULL;
static uint32_t next_pid = 1;
static int process_count_active = 0;

static char* p_strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

static void* p_memset(void* dest, int val, size_t count) {
    uint8_t* ptr = (uint8_t*)dest;
    for (size_t i = 0; i < count; i++) ptr[i] = (uint8_t)val;
    return dest;
}

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        p_memset(&process_table[i], 0, sizeof(process_t));
        process_table[i].state = PROCESS_TERMINATED;
    }

    /* Create initial Kernel Shell process (PID 1) */
    process_t* root = &process_table[0];
    root->pid = next_pid++;
    root->ppid = 0;
    p_strncpy(root->name, "KERNEL/SHELL", sizeof(root->name) - 1);
    root->state = PROCESS_RUNNING;
    root->is_user = 0;
    root->cr3 = vmm_get_cr3();
    p_strncpy(root->cwd, "C:\\", sizeof(root->cwd) - 1);

    /* Allocate kernel stack for root */
    root->kstack_base = (uintptr_t)kmalloc(KSTACK_SIZE);
    root->kstack_top = root->kstack_base + KSTACK_SIZE;

    /* Initialize root file descriptors */
    for (int fd = 0; fd < MAX_FD; fd++) {
        root->fds[fd].is_used = 0;
    }
    root->fds[0].is_used = 1; /* stdin */
    root->fds[1].is_used = 1; /* stdout */
    root->fds[2].is_used = 1; /* stderr */

    current_proc = root;
    process_count_active = 1;
    gdt_set_kernel_stack(root->kstack_top);
}

process_t* process_create(const char* name, uintptr_t entry_point, int is_user, uintptr_t pml4_phys, uintptr_t ustack_top) {
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_TERMINATED) {
            slot = i;
            break;
        }
    }

    if (slot == -1) return NULL;

    process_t* proc = &process_table[slot];
    p_memset(proc, 0, sizeof(process_t));

    proc->pid = next_pid++;
    proc->ppid = (current_proc) ? current_proc->pid : 0;
    p_strncpy(proc->name, (name && *name) ? name : "process", sizeof(proc->name) - 1);
    proc->is_user = is_user;
    proc->cr3 = (pml4_phys != 0) ? pml4_phys : vmm_get_cr3();
    proc->ustack_top = ustack_top;

    if (current_proc) {
        p_strncpy(proc->cwd, current_proc->cwd, sizeof(proc->cwd) - 1);
    } else {
        p_strncpy(proc->cwd, "C:\\", sizeof(proc->cwd) - 1);
    }

    /* Allocate kernel stack */
    proc->kstack_base = (uintptr_t)kmalloc(KSTACK_SIZE);
    if (!proc->kstack_base) {
        proc->state = PROCESS_TERMINATED;
        return NULL;
    }
    proc->kstack_top = proc->kstack_base + KSTACK_SIZE;

    /* Setup initial interrupt frame on top of kernel stack */
    uintptr_t frame_addr = (proc->kstack_top - sizeof(interrupt_frame_t)) & ~0xFULL;
    interrupt_frame_t* frame = (interrupt_frame_t*)frame_addr;
    p_memset(frame, 0, sizeof(interrupt_frame_t));

    frame->rip = entry_point;
    frame->cs = is_user ? USER_CS : KERNEL_CS;
    frame->ss = is_user ? USER_DS : KERNEL_DS;
    frame->rflags = 0x202; /* IF enabled, bit 1 reserved */
    frame->rsp = is_user ? ustack_top : (proc->kstack_top - 16);

    proc->saved_frame = frame;

    /* File descriptors: 0=stdin, 1=stdout, 2=stderr */
    for (int fd = 0; fd < MAX_FD; fd++) {
        proc->fds[fd].is_used = 0;
    }
    proc->fds[0].is_used = 1;
    proc->fds[1].is_used = 1;
    proc->fds[2].is_used = 1;

    proc->state = PROCESS_READY;
    process_count_active++;

    return proc;
}

process_t* process_get_current(void) {
    return current_proc;
}

process_t* process_get_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROCESS_TERMINATED && process_table[i].pid == pid) {
            return &process_table[i];
        }
    }
    return NULL;
}

int process_count(void) {
    return process_count_active;
}

process_t* process_get_by_index(size_t index) {
    if (index >= MAX_PROCESSES) return NULL;
    if (process_table[index].state == PROCESS_TERMINATED) return NULL;
    return &process_table[index];
}

void process_wake_sleepers(uint64_t current_tick) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_SLEEPING) {
            if (current_tick >= process_table[i].sleep_until_tick) {
                process_table[i].state = PROCESS_READY;
            }
        }
    }
}

uint64_t process_schedule(interrupt_frame_t* frame) {
    if (!current_proc) {
        return (uint64_t)frame;
    }

    /* Save current frame */
    current_proc->saved_frame = frame;
    if (current_proc->state == PROCESS_RUNNING) {
        current_proc->state = PROCESS_READY;
    }

    int cur_idx = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (&process_table[i] == current_proc) {
            cur_idx = i;
            break;
        }
    }

    /* Find next READY process */
    int next_idx = -1;
    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int idx = (cur_idx + i) % MAX_PROCESSES;
        if (process_table[idx].state == PROCESS_READY) {
            next_idx = idx;
            break;
        }
    }

    if (next_idx != -1) {
        current_proc = &process_table[next_idx];
    }

    current_proc->state = PROCESS_RUNNING;

    /* Update TSS kernel stack for Ring 3 -> Ring 0 transitions */
    gdt_set_kernel_stack(current_proc->kstack_top);

    /* Switch address space if necessary */
    if (current_proc->cr3 != vmm_get_cr3()) {
        vmm_switch_pml4(current_proc->cr3);
    }

    return (uint64_t)current_proc->saved_frame;
}

void process_yield(void) {
    /* Vector 0xFE is the software scheduling request. It must not be a timer
     * vector: routing yields through IRQ 0 made the PIT handler advance
     * kernel_ticks, so every yield fabricated a tick and shortened sleeps. */
    __asm__ volatile ("int $0xFE");
}

void process_sleep(uint64_t ticks) {
    extern uint64_t timer_get_ticks(void);
    if (!current_proc) return;

    uint64_t until = timer_get_ticks() + ticks;

    for (;;) {
        current_proc->sleep_until_tick = until;
        current_proc->state = PROCESS_SLEEPING;
        process_yield();

        /* Only a real PIT tick can move the deadline. If we are resumed before
         * it, nothing else was runnable, so idle until the next tick instead
         * of spinning on the scheduler. */
        if (timer_get_ticks() >= until) return;
        __asm__ volatile ("hlt");
    }
}

void process_exit(int code) {
    if (!current_proc) return;

    current_proc->exit_code = code;
    current_proc->state = PROCESS_TERMINATED;

    /* Close open file descriptors */
    for (int fd = 3; fd < MAX_FD; fd++) {
        if (current_proc->fds[fd].is_used && current_proc->fds[fd].node) {
            vfs_close(current_proc->fds[fd].node);
            current_proc->fds[fd].is_used = 0;
            current_proc->fds[fd].node = NULL;
        }
    }

    if (process_count_active > 0) {
        process_count_active--;
    }

    process_yield();
}

int process_kill(uint32_t pid) {
    if (pid <= 1) return -1; /* Protect PID 1 (Kernel/Berry shell) */

    process_t* proc = process_get_by_pid(pid);
    if (!proc) return -1;

    proc->state = PROCESS_TERMINATED;
    proc->exit_code = -9;

    for (int fd = 3; fd < MAX_FD; fd++) {
        if (proc->fds[fd].is_used && proc->fds[fd].node) {
            vfs_close(proc->fds[fd].node);
            proc->fds[fd].is_used = 0;
            proc->fds[fd].node = NULL;
        }
    }

    if (process_count_active > 0) {
        process_count_active--;
    }

    if (proc == current_proc) {
        process_yield();
    }
    return 0;
}

int process_alloc_fd(process_t* proc, vfs_node_t* node, int flags) {
    if (!proc || !node) return -1;
    for (int fd = 3; fd < MAX_FD; fd++) {
        if (!proc->fds[fd].is_used) {
            proc->fds[fd].node = node;
            proc->fds[fd].offset = 0;
            proc->fds[fd].flags = flags;
            proc->fds[fd].is_used = 1;
            return fd;
        }
    }
    return -1; /* Out of file descriptors */
}

int process_free_fd(process_t* proc, int fd) {
    if (!proc || fd < 3 || fd >= MAX_FD) return -1;
    if (!proc->fds[fd].is_used) return -1;

    if (proc->fds[fd].node) {
        vfs_close(proc->fds[fd].node);
    }
    proc->fds[fd].node = NULL;
    proc->fds[fd].offset = 0;
    proc->fds[fd].is_used = 0;
    return 0;
}

file_descriptor_t* process_get_fd(process_t* proc, int fd) {
    if (!proc || fd < 0 || fd >= MAX_FD) return NULL;
    if (!proc->fds[fd].is_used) return NULL;
    return &proc->fds[fd];
}
