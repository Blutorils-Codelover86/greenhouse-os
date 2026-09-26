#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

#define VMM_FLAG_PRESENT       (1ULL << 0)
#define VMM_FLAG_WRITABLE      (1ULL << 1)
#define VMM_FLAG_USER          (1ULL << 2)
#define VMM_FLAG_WRITE_THROUGH (1ULL << 3)
#define VMM_FLAG_NO_CACHE      (1ULL << 4)
#define VMM_FLAG_ACCESSED      (1ULL << 5)
#define VMM_FLAG_DIRTY         (1ULL << 6)
#define VMM_FLAG_HUGE          (1ULL << 7)
#define VMM_FLAG_GLOBAL        (1ULL << 8)
#define VMM_FLAG_NX            (1ULL << 63)

void vmm_init(void);
int vmm_map_page(uintptr_t virt, uintptr_t phys, uint64_t flags);
int vmm_unmap_page(uintptr_t virt);
uintptr_t vmm_virt_to_phys(uintptr_t virt);
uintptr_t vmm_get_cr3(void);
void vmm_invlpg(uintptr_t virt);
uintptr_t vmm_create_address_space(void);
void vmm_destroy_address_space(uintptr_t pml4_phys);
int vmm_map_page_in(uintptr_t pml4_phys, uintptr_t virt, uintptr_t phys, uint64_t flags);
void vmm_switch_pml4(uintptr_t pml4_phys);

#endif /* VMM_H */
