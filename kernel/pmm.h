#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

typedef struct {
    uint64_t total_memory_bytes;
    uint64_t usable_memory_bytes;
    uint64_t total_frames;
    uint64_t usable_frames;
    uint64_t used_frames;
    uint64_t free_frames;
    uint64_t reserved_frames;
    uintptr_t max_phys_addr;
} pmm_stats_t;

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
} pmm_mem_region_t;

void pmm_init(uint32_t mb2_magic, uint64_t mb2_info_addr, uintptr_t kernel_start, uintptr_t kernel_end);
uintptr_t pmm_alloc_frame(void);
void pmm_free_frame(uintptr_t phys_addr);
uintptr_t pmm_alloc_frames(size_t count);
void pmm_free_frames(uintptr_t phys_addr, size_t count);
pmm_stats_t pmm_get_stats(void);
size_t pmm_get_region_count(void);
int pmm_get_region(size_t index, pmm_mem_region_t* out_region);

#endif /* PMM_H */
