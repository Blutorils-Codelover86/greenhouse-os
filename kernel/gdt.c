#include "gdt.h"

static struct {
    gdt_entry_t null_desc;
    gdt_entry_t kernel_code;
    gdt_entry_t kernel_data;
    gdt_entry_t user_data;
    gdt_entry_t user_code;
    tss_entry_t tss_desc;
} __attribute__((packed, aligned(16))) gdt_table;

static tss_t kernel_tss;
static gdt_ptr_t gdt_pointer;

extern void gdt_load_flush(gdt_ptr_t* ptr);
extern void tss_load(uint16_t sel);

void gdt_init(void) {
    /* 1. Clear GDT and TSS structures */
    for (size_t i = 0; i < sizeof(gdt_table); i++) {
        ((uint8_t*)&gdt_table)[i] = 0;
    }
    for (size_t i = 0; i < sizeof(kernel_tss); i++) {
        ((uint8_t*)&kernel_tss)[i] = 0;
    }

    /* 2. Null Descriptor (0x00) */
    /* All zeroes */

    /* 3. Kernel 64-bit Code Segment (0x08) */
    /* Access: Present(1), DPL(00), Code/Data(1), Executable(1), Readable(1) = 0x9A */
    /* Granularity: LongMode(1), 32-bit(0), Gran(0) = 0x20 */
    gdt_table.kernel_code.access = 0x9A;
    gdt_table.kernel_code.granularity = 0x20;

    /* 4. Kernel 64-bit Data Segment (0x10) */
    /* Access: Present(1), DPL(00), Code/Data(1), Writable(1) = 0x92 */
    gdt_table.kernel_data.access = 0x92;
    gdt_table.kernel_data.granularity = 0x00;

    /* 5. User 64-bit Data Segment (0x18 -> 0x1B with RPL=3) */
    /* Access: Present(1), DPL(11), Code/Data(1), Writable(1) = 0xF2 */
    gdt_table.user_data.access = 0xF2;
    gdt_table.user_data.granularity = 0x00;

    /* 6. User 64-bit Code Segment (0x20 -> 0x23 with RPL=3) */
    /* Access: Present(1), DPL(11), Code/Data(1), Executable(1), Readable(1) = 0xFA */
    /* Granularity: LongMode(1), 32-bit(0), Gran(0) = 0x20 */
    gdt_table.user_code.access = 0xFA;
    gdt_table.user_code.granularity = 0x20;

    /* 7. Setup TSS (0x28) */
    uintptr_t tss_base = (uintptr_t)&kernel_tss;
    uint32_t tss_limit = sizeof(tss_t) - 1;

    kernel_tss.iomap_base = sizeof(tss_t);

    gdt_table.tss_desc.limit_low = (uint16_t)(tss_limit & 0xFFFF);
    gdt_table.tss_desc.base_low = (uint16_t)(tss_base & 0xFFFF);
    gdt_table.tss_desc.base_mid = (uint8_t)((tss_base >> 16) & 0xFF);
    gdt_table.tss_desc.access = 0x89; /* Present, 64-bit TSS (Available), DPL=0 */
    gdt_table.tss_desc.granularity = (uint8_t)((tss_limit >> 16) & 0x0F);
    gdt_table.tss_desc.base_high = (uint8_t)((tss_base >> 24) & 0xFF);
    gdt_table.tss_desc.base_upper = (uint32_t)((tss_base >> 32) & 0xFFFFFFFF);
    gdt_table.tss_desc.reserved = 0;

    /* 8. Load GDT */
    gdt_pointer.limit = sizeof(gdt_table) - 1;
    gdt_pointer.base = (uint64_t)&gdt_table;

    gdt_load_flush(&gdt_pointer);
    tss_load(TSS_SEL);
}

void gdt_set_kernel_stack(uintptr_t kstack) {
    kernel_tss.rsp0 = kstack;
}

tss_t* gdt_get_tss(void) {
    return &kernel_tss;
}
