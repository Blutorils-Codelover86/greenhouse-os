#include "pmm.h"

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289
#define MB2_TAG_TYPE_END               0
#define MB2_TAG_TYPE_BASIC_MEMINFO     4
#define MB2_TAG_TYPE_MMAP              6

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

static uint8_t* pmm_bitmap = NULL;
static size_t pmm_bitmap_size = 0;
static size_t pmm_total_frames = 0;
static size_t pmm_used_frames = 0;
static size_t pmm_usable_frames = 0;
static uint64_t pmm_total_ram_bytes = 0;
static uint64_t pmm_usable_ram_bytes = 0;
static uintptr_t pmm_max_phys_addr = 0;

static inline void bitmap_set(size_t frame) {
    if (frame < pmm_total_frames) {
        pmm_bitmap[frame / 8] |= (1 << (frame % 8));
    }
}

static inline void bitmap_clear(size_t frame) {
    if (frame < pmm_total_frames) {
        pmm_bitmap[frame / 8] &= ~(1 << (frame % 8));
    }
}

static inline int bitmap_test(size_t frame) {
    if (frame >= pmm_total_frames) return 1;
    return (pmm_bitmap[frame / 8] & (1 << (frame % 8))) != 0;
}

static void pmm_mark_region(uintptr_t base, size_t length, int used) {
    size_t start_frame = base / PAGE_SIZE;
    size_t frame_count = (length + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t i = 0; i < frame_count; i++) {
        size_t f = start_frame + i;
        if (f < pmm_total_frames) {
            if (used) {
                if (!bitmap_test(f)) {
                    bitmap_set(f);
                    pmm_used_frames++;
                }
            } else {
                if (bitmap_test(f)) {
                    bitmap_clear(f);
                    if (pmm_used_frames > 0) pmm_used_frames--;
                }
            }
        }
    }
}

static pmm_mem_region_t pmm_regions[64];
static size_t pmm_region_count = 0;

void pmm_init(uint32_t mb2_magic, uint64_t mb2_info_addr, uintptr_t kernel_start, uintptr_t kernel_end) {
    pmm_total_frames = 0;
    pmm_used_frames = 0;
    pmm_usable_frames = 0;
    pmm_total_ram_bytes = 0;
    pmm_usable_ram_bytes = 0;
    pmm_max_phys_addr = 0;
    pmm_region_count = 0;

    if (mb2_magic != MULTIBOOT2_BOOTLOADER_MAGIC || mb2_info_addr == 0) {
        /* Fallback default to 128 MB */
        pmm_max_phys_addr = 128 * 1024 * 1024;
        pmm_regions[0].base = 0x100000;
        pmm_regions[0].length = 127 * 1024 * 1024;
        pmm_regions[0].type = 1;
        pmm_region_count = 1;
    } else {
        uint32_t total_size = *(volatile uint32_t*)mb2_info_addr;
        uint8_t* tag_ptr = (uint8_t*)(mb2_info_addr + 8);
        uint8_t* end_ptr = (uint8_t*)(mb2_info_addr + total_size);

        while (tag_ptr < end_ptr) {
            uint32_t tag_type = *(uint32_t*)tag_ptr;
            uint32_t tag_size = *(uint32_t*)(tag_ptr + 4);

            if (tag_type == MB2_TAG_TYPE_END || tag_size == 0) break;

            if (tag_type == MB2_TAG_TYPE_MMAP) {
                uint32_t entry_size = *(uint32_t*)(tag_ptr + 8);
                if (entry_size == 0) entry_size = sizeof(mb2_mmap_entry_t);
                uint8_t* mmap_curr = tag_ptr + 16;
                uint8_t* mmap_end = tag_ptr + tag_size;

                while (mmap_curr + entry_size <= mmap_end) {
                    mb2_mmap_entry_t* entry = (mb2_mmap_entry_t*)mmap_curr;
                    uint64_t reg_end = entry->base_addr + entry->length;

                    if (pmm_region_count < 64) {
                        pmm_regions[pmm_region_count].base = entry->base_addr;
                        pmm_regions[pmm_region_count].length = entry->length;
                        pmm_regions[pmm_region_count].type = entry->type;
                        pmm_region_count++;
                    }

                    pmm_total_ram_bytes += entry->length;
                    if (entry->type == 1) { // Available RAM
                        pmm_usable_ram_bytes += entry->length;
                        if (reg_end > pmm_max_phys_addr && reg_end <= 0x100000000ULL) {
                            pmm_max_phys_addr = (uintptr_t)reg_end;
                        }
                    }
                    mmap_curr += entry_size;
                }
            }
            tag_ptr += ((tag_size + 7) & ~7);
        }
    }

    if (pmm_max_phys_addr == 0) {
        pmm_max_phys_addr = 128 * 1024 * 1024;
    }

    pmm_total_frames = pmm_max_phys_addr / PAGE_SIZE;
    pmm_bitmap_size = (pmm_total_frames + 7) / 8;

    /* Place bitmap immediately after aligned kernel end */
    uintptr_t bitmap_addr = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    pmm_bitmap = (uint8_t*)bitmap_addr;

    /* Initially mark all frames as USED (1) */
    for (size_t i = 0; i < pmm_bitmap_size; i++) {
        pmm_bitmap[i] = 0xFF;
    }
    pmm_used_frames = pmm_total_frames;

    /* Mark usable memory frames as available */
    for (size_t r = 0; r < pmm_region_count; r++) {
        if (pmm_regions[r].type == 1 && pmm_regions[r].base < pmm_max_phys_addr) {
            uint64_t len = pmm_regions[r].length;
            if (pmm_regions[r].base + len > pmm_max_phys_addr) {
                len = pmm_max_phys_addr - pmm_regions[r].base;
            }
            pmm_mark_region((uintptr_t)pmm_regions[r].base, (size_t)len, 0);
            pmm_usable_frames += (size_t)(len / PAGE_SIZE);
        }
    }

    /* 1. Reserve Lower 1MB (BIOS, IVT, VGA buffer, EBDA) */
    pmm_mark_region(0x0, 0x100000, 1);

    /* 2. Reserve Kernel binary area */
    pmm_mark_region(kernel_start, kernel_end - kernel_start, 1);

    /* 3. Reserve Multiboot2 info buffer */
    if (mb2_info_addr != 0) {
        uint32_t mb2_size = *(volatile uint32_t*)mb2_info_addr;
        pmm_mark_region((uintptr_t)mb2_info_addr, mb2_size, 1);
    }

    /* 4. Reserve PMM Bitmap area itself */
    pmm_mark_region(bitmap_addr, pmm_bitmap_size, 1);
}

uintptr_t pmm_alloc_frame(void) {
    for (size_t i = 0; i < pmm_total_frames; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            pmm_used_frames++;
            return (uintptr_t)(i * PAGE_SIZE);
        }
    }
    return 0; /* Out of physical memory */
}

void pmm_free_frame(uintptr_t phys_addr) {
    size_t frame = phys_addr / PAGE_SIZE;
    if (frame < pmm_total_frames) {
        if (bitmap_test(frame)) {
            bitmap_clear(frame);
            if (pmm_used_frames > 0) pmm_used_frames--;
        }
    }
}

uintptr_t pmm_alloc_frames(size_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_frame();

    size_t consecutive = 0;
    size_t start_frame = 0;

    for (size_t i = 0; i < pmm_total_frames; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) start_frame = i;
            consecutive++;
            if (consecutive == count) {
                for (size_t j = 0; j < count; j++) {
                    bitmap_set(start_frame + j);
                }
                pmm_used_frames += count;
                return (uintptr_t)(start_frame * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }
    return 0; /* Out of contiguous physical memory */
}

void pmm_free_frames(uintptr_t phys_addr, size_t count) {
    size_t start_frame = phys_addr / PAGE_SIZE;
    for (size_t i = 0; i < count; i++) {
        size_t f = start_frame + i;
        if (f < pmm_total_frames && bitmap_test(f)) {
            bitmap_clear(f);
            if (pmm_used_frames > 0) pmm_used_frames--;
        }
    }
}

pmm_stats_t pmm_get_stats(void) {
    pmm_stats_t stats;
    stats.total_memory_bytes = pmm_total_ram_bytes;
    stats.usable_memory_bytes = pmm_usable_ram_bytes;
    stats.total_frames = pmm_total_frames;
    stats.usable_frames = pmm_usable_frames;
    stats.used_frames = pmm_used_frames;
    stats.free_frames = (pmm_total_frames > pmm_used_frames) ? (pmm_total_frames - pmm_used_frames) : 0;
    stats.reserved_frames = (pmm_total_frames > pmm_usable_frames) ? (pmm_total_frames - pmm_usable_frames) : 0;
    stats.max_phys_addr = pmm_max_phys_addr;
    return stats;
}

size_t pmm_get_region_count(void) {
    return pmm_region_count;
}

int pmm_get_region(size_t index, pmm_mem_region_t* out_region) {
    if (index >= pmm_region_count || !out_region) return -1;
    *out_region = pmm_regions[index];
    return 0;
}
