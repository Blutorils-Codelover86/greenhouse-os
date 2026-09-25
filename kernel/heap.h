#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t heap_start;
    uint64_t heap_end;
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    size_t total_blocks;
    size_t active_allocs;
} heap_stats_t;

void heap_init(uintptr_t start_addr, size_t initial_bytes);
void* kmalloc(size_t size);
void kfree(void* ptr);
void* kcalloc(size_t num, size_t size);
void* krealloc(void* ptr, size_t new_size);
heap_stats_t heap_get_stats(void);

#endif /* HEAP_H */
