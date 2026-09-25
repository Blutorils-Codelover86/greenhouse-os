#include "vmm.h"
#include "pmm.h"

static uintptr_t kernel_pml4_phys = 0;

static inline void memset_page(void* ptr) {
    uint64_t* p = (uint64_t*)ptr;
    for (int i = 0; i < 512; i++) {
        p[i] = 0;
    }
}

uintptr_t vmm_get_cr3(void) {
    uintptr_t cr3_val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3_val));
    return cr3_val & ~0xFFFULL;
}

void vmm_invlpg(uintptr_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_init(void) {
    kernel_pml4_phys = vmm_get_cr3();
}

int vmm_map_page(uintptr_t virt, uintptr_t phys, uint64_t flags) {
    if (kernel_pml4_phys == 0) {
        vmm_init();
    }

    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pml4 = (uint64_t*)kernel_pml4_phys;

    /* 1. PDPT Table */
    if (!(pml4[pml4_idx] & VMM_FLAG_PRESENT)) {
        uintptr_t new_pdpt = pmm_alloc_frame();
        if (!new_pdpt) return -1;
        memset_page((void*)new_pdpt);
        pml4[pml4_idx] = new_pdpt | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | (flags & VMM_FLAG_USER);
    }
    uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFFULL);

    /* 2. Page Directory Table */
    if (!(pdpt[pdpt_idx] & VMM_FLAG_PRESENT)) {
        uintptr_t new_pd = pmm_alloc_frame();
        if (!new_pd) return -1;
        memset_page((void*)new_pd);
        pdpt[pdpt_idx] = new_pd | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | (flags & VMM_FLAG_USER);
    }
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFFULL);

    /* 3. Handle 2MB Huge Page Split if already mapped */
    if ((pd[pd_idx] & VMM_FLAG_PRESENT) && (pd[pd_idx] & VMM_FLAG_HUGE)) {
        uintptr_t huge_phys_base = pd[pd_idx] & ~0x1FFFFFULL;
        uint64_t huge_flags = pd[pd_idx] & 0x1FF;
        huge_flags &= ~VMM_FLAG_HUGE;

        uintptr_t new_pt = pmm_alloc_frame();
        if (!new_pt) return -1;
        uint64_t* pt = (uint64_t*)new_pt;

        for (int i = 0; i < 512; i++) {
            pt[i] = (huge_phys_base + (i * PAGE_SIZE)) | huge_flags;
        }

        pd[pd_idx] = new_pt | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | (flags & VMM_FLAG_USER);
    }

    /* 4. Page Table */
    if (!(pd[pd_idx] & VMM_FLAG_PRESENT)) {
        uintptr_t new_pt = pmm_alloc_frame();
        if (!new_pt) return -1;
        memset_page((void*)new_pt);
        pd[pd_idx] = new_pt | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | (flags & VMM_FLAG_USER);
    }
    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFFULL);

    /* 5. Set PT Entry */
    pt[pt_idx] = (phys & ~0xFFFULL) | (flags & 0xFFF) | VMM_FLAG_PRESENT;
    vmm_invlpg(virt);

    return 0;
}

int vmm_unmap_page(uintptr_t virt) {
    if (kernel_pml4_phys == 0) vmm_init();

    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pml4 = (uint64_t*)kernel_pml4_phys;
    if (!(pml4[pml4_idx] & VMM_FLAG_PRESENT)) return -1;

    uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & VMM_FLAG_PRESENT)) return -1;

    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & VMM_FLAG_PRESENT)) return -1;

    if (pd[pd_idx] & VMM_FLAG_HUGE) {
        /* Split 2MB page to unmap 4KB page */
        uintptr_t huge_phys_base = pd[pd_idx] & ~0x1FFFFFULL;
        uint64_t huge_flags = pd[pd_idx] & 0x1FF;
        huge_flags &= ~VMM_FLAG_HUGE;

        uintptr_t new_pt = pmm_alloc_frame();
        if (!new_pt) return -1;
        uint64_t* pt = (uint64_t*)new_pt;

        for (int i = 0; i < 512; i++) {
            if (i == (int)pt_idx) {
                pt[i] = 0;
            } else {
                pt[i] = (huge_phys_base + (i * PAGE_SIZE)) | huge_flags;
            }
        }
        pd[pd_idx] = new_pt | VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE;
        vmm_invlpg(virt);
        return 0;
    }

    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFFULL);
    pt[pt_idx] = 0;
    vmm_invlpg(virt);

    return 0;
}

uintptr_t vmm_virt_to_phys(uintptr_t virt) {
    if (kernel_pml4_phys == 0) vmm_init();

    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;
    uintptr_t offset = virt & 0xFFFULL;

    uint64_t* pml4 = (uint64_t*)kernel_pml4_phys;
    if (!(pml4[pml4_idx] & VMM_FLAG_PRESENT)) return 0;

    uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & VMM_FLAG_PRESENT)) return 0;

    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & VMM_FLAG_PRESENT)) return 0;

    if (pd[pd_idx] & VMM_FLAG_HUGE) {
        return (pd[pd_idx] & ~0x1FFFFFULL) + (virt & 0x1FFFFFULL);
    }

    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFFULL);
    if (!(pt[pt_idx] & VMM_FLAG_PRESENT)) return 0;

    return (pt[pt_idx] & ~0xFFFULL) + offset;
}
