#include "elf.h"
#include "vmm.h"
#include "pmm.h"
#include "heap.h"
#include "vfs.h"

static void* e_memset(void* dest, int val, size_t count) {
    uint8_t* ptr = (uint8_t*)dest;
    for (size_t i = 0; i < count; i++) ptr[i] = (uint8_t)val;
    return dest;
}

static size_t e_strlen(const char* s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

int elf_load_executable(const char* filepath, process_t** out_proc) {
    return elf_load_executable_args(filepath, 0, NULL, out_proc);
}

int elf_load_executable_args(const char* filepath, int argc, char** argv, process_t** out_proc) {
    if (!filepath || !*filepath) return -1;

    process_t* cur = process_get_current();
    const char* cwd = (cur) ? cur->cwd : "C:\\";

    vfs_node_t* file = vfs_resolve_path(cwd, filepath);
    if (!file || (file->flags & VFS_DIRECTORY)) {
        return -1;
    }

    Elf64_Ehdr ehdr;
    if (vfs_read(file, 0, sizeof(Elf64_Ehdr), (uint8_t*)&ehdr) != sizeof(Elf64_Ehdr)) {
        return -1;
    }

    /* Validate ELF Header */
    if (*(uint32_t*)ehdr.e_ident != ELF_MAGIC) return -2;
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) return -3;
    if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB) return -4;
    if (ehdr.e_type != ET_EXEC && ehdr.e_type != 3 /* DYN/PIE */) return -5;
    if (ehdr.e_machine != EM_X86_64) return -6;

    /* Create address space for new process */
    uintptr_t proc_pml4 = vmm_create_address_space();
    if (!proc_pml4) return -7;

    /* Read and load PT_LOAD program headers */
    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr phdr;
        uint32_t phdr_offset = (uint32_t)ehdr.e_phoff + (i * ehdr.e_phentsize);
        if (vfs_read(file, phdr_offset, sizeof(Elf64_Phdr), (uint8_t*)&phdr) != sizeof(Elf64_Phdr)) {
            vmm_destroy_address_space(proc_pml4);
            return -8;
        }

        if (phdr.p_type == PT_LOAD) {
            uintptr_t seg_vstart = phdr.p_vaddr & ~0xFFFULL;
            uintptr_t seg_vend   = (phdr.p_vaddr + phdr.p_memsz + 4095) & ~0xFFFULL;

            for (uintptr_t vpage = seg_vstart; vpage < seg_vend; vpage += 4096) {
                uintptr_t phys_frame = pmm_alloc_frame();
                if (!phys_frame) {
                    vmm_destroy_address_space(proc_pml4);
                    return -9;
                }
                e_memset((void*)phys_frame, 0, 4096);

                /* Calculate file overlap with this page */
                uintptr_t page_end = vpage + 4096;
                uintptr_t file_seg_start = phdr.p_vaddr;
                uintptr_t file_seg_end = phdr.p_vaddr + phdr.p_filesz;

                if (vpage < file_seg_end && page_end > file_seg_start) {
                    uintptr_t copy_vstart = (vpage > file_seg_start) ? vpage : file_seg_start;
                    uintptr_t copy_vend   = (page_end < file_seg_end) ? page_end : file_seg_end;
                    size_t copy_len = copy_vend - copy_vstart;

                    size_t offset_in_page = copy_vstart - vpage;
                    uint32_t file_read_offset = (uint32_t)(phdr.p_offset + (copy_vstart - file_seg_start));

                    vfs_read(file, file_read_offset, copy_len, (void*)(phys_frame + offset_in_page));
                }

                uint64_t flags = VMM_FLAG_PRESENT | VMM_FLAG_USER | VMM_FLAG_WRITABLE;
                vmm_map_page_in(proc_pml4, vpage, phys_frame, flags);
            }
        }
    }

    /* Allocate and Map User Stack (16 KiB = 4 pages at USER_STACK_TOP - 16KB) */
    uintptr_t ustack_base = USER_STACK_TOP - USTACK_SIZE;
    uintptr_t top_frame = 0;
    for (uintptr_t spage = ustack_base; spage < USER_STACK_TOP; spage += 4096) {
        uintptr_t s_frame = pmm_alloc_frame();
        if (!s_frame) {
            vmm_destroy_address_space(proc_pml4);
            return -10;
        }
        e_memset((void*)s_frame, 0, 4096);
        vmm_map_page_in(proc_pml4, spage, s_frame, VMM_FLAG_PRESENT | VMM_FLAG_USER | VMM_FLAG_WRITABLE);
        if (spage == USER_STACK_TOP - 4096) {
            top_frame = s_frame;
        }
    }

    /* Extract binary name */
    size_t plen = e_strlen(filepath);
    size_t name_start = plen;
    while (name_start > 0 && filepath[name_start - 1] != '\\' && filepath[name_start - 1] != '/') {
        name_start--;
    }
    const char* proc_name = filepath + name_start;

    uintptr_t ustack_top = USER_STACK_TOP - 16;
    uintptr_t user_argv_addr = 0;

    /* Populate argv on the top stack page if arguments are provided */
    if (argc > 0 && argv && top_frame) {
        size_t total_str_bytes = 0;
        for (int i = 0; i < argc; i++) {
            total_str_bytes += e_strlen(argv[i]) + 1;
        }

        size_t aligned_str_bytes = (total_str_bytes + 15) & ~15ULL;
        uintptr_t str_vaddr_start = USER_STACK_TOP - 16 - aligned_str_bytes;

        size_t ptr_bytes = (argc + 1) * sizeof(uint64_t);
        size_t aligned_ptr_bytes = (ptr_bytes + 15) & ~15ULL;
        user_argv_addr = str_vaddr_start - aligned_ptr_bytes;
        ustack_top = (user_argv_addr - 16) & ~15ULL;

        uintptr_t cur_str_vaddr = str_vaddr_start;
        uint64_t* user_argv_table = (uint64_t*)(top_frame + (user_argv_addr - (USER_STACK_TOP - 4096)));

        for (int i = 0; i < argc; i++) {
            user_argv_table[i] = cur_str_vaddr;
            char* dest = (char*)(top_frame + (cur_str_vaddr - (USER_STACK_TOP - 4096)));
            const char* src = argv[i];
            size_t slen = e_strlen(src);
            for (size_t k = 0; k <= slen; k++) {
                dest[k] = src[k];
            }
            cur_str_vaddr += slen + 1;
        }
        user_argv_table[argc] = 0; /* NULL terminator */
    }

    process_t* proc = process_create(proc_name, ehdr.e_entry, 1 /* is_user */, proc_pml4, ustack_top);
    if (!proc) {
        vmm_destroy_address_space(proc_pml4);
        return -11;
    }

    if (argc > 0 && user_argv_addr) {
        proc->saved_frame->rdi = (uint64_t)argc;
        proc->saved_frame->rsi = (uint64_t)user_argv_addr;
        proc->saved_frame->rsp = (uint64_t)ustack_top;
    }

    if (out_proc) {
        *out_proc = proc;
    }
    return 0;
}
