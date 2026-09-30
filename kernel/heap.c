#include "heap.h"
#include "pmm.h"
#include "vmm.h"

#define HEAP_BLOCK_MAGIC 0x47484550 /* "GHEP" */

typedef struct heap_block {
    uint32_t magic;
    uint32_t is_free;
    size_t size; /* Payload size in bytes */
    struct heap_block* next;
    struct heap_block* prev;
} __attribute__((aligned(16))) heap_block_t;

#define BLOCK_HEADER_SIZE (sizeof(heap_block_t))

static uintptr_t heap_base_addr = 0;
static uintptr_t heap_current_end = 0;
static heap_block_t* heap_first_block = NULL;

static void* heap_memset(void* dest, int val, size_t count) {
    uint8_t* ptr = (uint8_t*)dest;
    for (size_t i = 0; i < count; i++) {
        ptr[i] = (uint8_t)val;
    }
    return dest;
}

static void* heap_memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
    }
    return dest;
}

static int heap_expand(size_t additional_bytes) {
    size_t pages_needed = (additional_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t i = 0; i < pages_needed; i++) {
        uintptr_t frame = pmm_alloc_frame();
        if (!frame) return -1; /* Out of physical memory */

        if (vmm_map_page(heap_current_end, frame, VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE) != 0) {
            pmm_free_frame(frame);
            return -1;
        }

        heap_memset((void*)heap_current_end, 0, PAGE_SIZE);
        heap_current_end += PAGE_SIZE;
    }

    return 0;
}

void heap_init(uintptr_t start_addr, size_t initial_bytes) {
    heap_base_addr = (start_addr + 15) & ~15ULL;
    heap_current_end = heap_base_addr;

    if (initial_bytes < PAGE_SIZE) initial_bytes = PAGE_SIZE;
    heap_expand(initial_bytes);

    heap_first_block = (heap_block_t*)heap_base_addr;
    heap_first_block->magic = HEAP_BLOCK_MAGIC;
    heap_first_block->is_free = 1;
    heap_first_block->size = (heap_current_end - heap_base_addr) - BLOCK_HEADER_SIZE;
    heap_first_block->next = NULL;
    heap_first_block->prev = NULL;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    if (heap_first_block == NULL) {
        heap_init(0x20000000ULL, 16 * 1024 * 1024); /* 16 MiB default initial heap */
    }

    /* 16-byte align payload size */
    size = (size + 15) & ~15ULL;

    heap_block_t* curr = heap_first_block;
    heap_block_t* last = NULL;

    while (curr) {
        if (curr->magic != HEAP_BLOCK_MAGIC) {
            /* Corrupted heap block magic */
            return NULL;
        }

        if (curr->is_free && curr->size >= size) {
            /* Check if block can be split */
            if (curr->size >= size + BLOCK_HEADER_SIZE + 16) {
                heap_block_t* remainder = (heap_block_t*)((uintptr_t)curr + BLOCK_HEADER_SIZE + size);
                remainder->magic = HEAP_BLOCK_MAGIC;
                remainder->is_free = 1;
                remainder->size = curr->size - size - BLOCK_HEADER_SIZE;
                remainder->next = curr->next;
                remainder->prev = curr;

                if (curr->next) {
                    curr->next->prev = remainder;
                }
                curr->next = remainder;
                curr->size = size;
            }

            curr->is_free = 0;
            return (void*)((uintptr_t)curr + BLOCK_HEADER_SIZE);
        }

        last = curr;
        curr = curr->next;
    }

    /* Need to expand heap */
    size_t expand_amount = size + BLOCK_HEADER_SIZE + PAGE_SIZE;
    if (expand_amount < 2 * 1024 * 1024) {
        expand_amount = 2 * 1024 * 1024; /* Expand in 2 MiB chunks */
    }
    uintptr_t old_end = heap_current_end;

    if (heap_expand(expand_amount) != 0) {
        return NULL; /* Out of memory */
    }

    /* If last block was free, merge with newly mapped memory */
    if (last && last->is_free) {
        last->size += (heap_current_end - old_end);
        return kmalloc(size);
    } else {
        heap_block_t* new_block = (heap_block_t*)old_end;
        new_block->magic = HEAP_BLOCK_MAGIC;
        new_block->is_free = 1;
        new_block->size = (heap_current_end - old_end) - BLOCK_HEADER_SIZE;
        new_block->next = NULL;
        new_block->prev = last;

        if (last) {
            last->next = new_block;
        }
        return kmalloc(size);
    }
}

void kfree(void* ptr) {
    if (!ptr) return;

    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - BLOCK_HEADER_SIZE);
    if (block->magic != HEAP_BLOCK_MAGIC) {
        return; /* Invalid pointer or corrupted block */
    }

    block->is_free = 1;

    /* Merge with next block if free */
    if (block->next && block->next->is_free) {
        heap_block_t* next_block = block->next;
        block->size += BLOCK_HEADER_SIZE + next_block->size;
        block->next = next_block->next;
        if (next_block->next) {
            next_block->next->prev = block;
        }
    }

    /* Merge with previous block if free */
    if (block->prev && block->prev->is_free) {
        heap_block_t* prev_block = block->prev;
        prev_block->size += BLOCK_HEADER_SIZE + block->size;
        prev_block->next = block->next;
        if (block->next) {
            block->next->prev = prev_block;
        }
    }
}

void* kcalloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        heap_memset(ptr, 0, total);
    }
    return ptr;
}

void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - BLOCK_HEADER_SIZE);
    if (block->magic != HEAP_BLOCK_MAGIC) return NULL;

    if (block->size >= new_size) {
        return ptr; /* Already large enough */
    }

    void* new_ptr = kmalloc(new_size);
    if (new_ptr) {
        heap_memcpy(new_ptr, ptr, block->size);
        kfree(ptr);
    }
    return new_ptr;
}

heap_stats_t heap_get_stats(void) {
    heap_stats_t stats;
    stats.heap_start = heap_base_addr;
    stats.heap_end = heap_current_end;
    stats.total_bytes = (heap_current_end > heap_base_addr) ? (heap_current_end - heap_base_addr) : 0;
    stats.used_bytes = 0;
    stats.free_bytes = 0;
    stats.total_blocks = 0;
    stats.active_allocs = 0;

    heap_block_t* curr = heap_first_block;
    while (curr) {
        if (curr->magic != HEAP_BLOCK_MAGIC) break;

        stats.total_blocks++;
        if (curr->is_free) {
            stats.free_bytes += curr->size;
        } else {
            stats.used_bytes += curr->size + BLOCK_HEADER_SIZE;
            stats.active_allocs++;
        }
        curr = curr->next;
    }

    return stats;
}
