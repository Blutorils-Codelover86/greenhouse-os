; ==============================================================================
; Greenhouse OS 1.1.0 - 64-bit Bootloader & Multiboot2 Header
; ==============================================================================
default rel

; Multiboot 2 header definitions
MB2_MAGIC       equ 0xe85250d6
MB2_ARCH_I386   equ 0
MB2_HDR_LEN     equ (multiboot_header_end - multiboot_header_start)
MB2_CHECKSUM    equ (0x100000000 - (MB2_MAGIC + MB2_ARCH_I386 + MB2_HDR_LEN))

section .multiboot2
align 8
multiboot_header_start:
    dd MB2_MAGIC
    dd MB2_ARCH_I386
    dd MB2_HDR_LEN
    dd MB2_CHECKSUM

    ; No framebuffer tag on purpose. Requesting one makes the loader program a
    ; graphics mode before the kernel runs, which hides the VGA text console
    ; this OS boots into. The VBE backend programs and describes the mode
    ; itself, and it re-reads the adapter registers instead of trusting the
    ; loader, so a boot-time framebuffer tag would only add a way to get the
    ; wrong geometry.

    ; End tag
    align 8
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
multiboot_header_end:

section .text
bits 32
global _start
extern kernel_main

_start:
    cli

    ; Initialize 32-bit temporary stack
    mov esp, stack_top

    ; Save multiboot2 arguments
    mov [mb2_magic], eax
    mov [mb2_info_ptr], ebx

    ; Check CPUID and Long Mode support
    call check_cpuid
    call check_long_mode

    ; Set up 64-bit paging (Identity map first 1GB using 2MB huge pages)
    call setup_page_tables
    call enable_paging

    ; Load 64-bit Global Descriptor Table
    lgdt [gdt64_ptr]

    ; Far jump to 64-bit Long Mode code segment
    jmp 0x08:long_mode_entry

; ------------------------------------------------------------------------------
; 32-bit CPU Check Routines
; ------------------------------------------------------------------------------
check_cpuid:
    ; Check if CPUID is supported by attempting to flip bit 21 in EFLAGS
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    mov al, '1' ; Error 1: No CPUID
    jmp error_halt

check_long_mode:
    ; Check extended CPUID functions
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    ; Check Long Mode bit in extended processor info
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret
.no_long_mode:
    mov al, '2' ; Error 2: No Long Mode support
    jmp error_halt

error_halt:
    ; Print 'ERR:' and error code to VGA buffer at 0xB8000
    mov dword [0xB8000], 0x4F524F45 ; 'E', 'R' with red bg / white fg
    mov dword [0xB8004], 0x4F204F52 ; 'R', ':'
    mov byte [0xB8008], al
    mov byte [0xB8009], 0x4F
    cli
    hlt
    jmp $

; ------------------------------------------------------------------------------
; Paging Setup (PML4 -> PDPT -> PD with 512 x 2MB Huge Pages = 1GB identity map)
; ------------------------------------------------------------------------------
setup_page_tables:
    ; Point PML4[0] to PDPT (Present | Writable)
    mov eax, pdpt_table
    or eax, 0x03
    mov [pml4_table], eax

    ; Point PDPT[0] to Page Directory (Present | Writable)
    mov eax, pd_table
    or eax, 0x03
    mov [pdpt_table], eax

    ; Map 512 x 2MB entries in PD table (Total 1GB: 0x00000000 to 0x3FFFFFFF)
    mov ecx, 0
.map_pd:
    mov eax, 0x200000       ; 2MB
    mul ecx                 ; EAX = ecx * 2MB
    or eax, 0x83            ; Present | Writable | Huge Page (2MB)
    mov [pd_table + ecx * 8], eax
    mov dword [pd_table + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 512
    jne .map_pd
    ret

enable_paging:
    ; Load PML4 address into CR3
    mov eax, pml4_table
    mov cr3, eax

    ; Enable PAE (Physical Address Extension) in CR4 (bit 5)
    ; Enable OSFXSR (bit 9) and OSXMMEXCPT (bit 10) for SSE support
    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10)
    mov cr4, eax

    ; Set Long Mode Enable (LME) in EFER MSR (0xC0000080, bit 8)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable Paging (PG bit 31) and Protected Mode (PE bit 0) in CR0
    ; Clear EM (bit 2) and set MP (bit 1) for FPU/SSE
    mov eax, cr0
    and eax, ~(1 << 2)
    or eax, (1 << 31) | (1 << 1) | (1 << 0)
    mov cr0, eax
    ret

; ------------------------------------------------------------------------------
; 64-bit Long Mode Entry Point
; ------------------------------------------------------------------------------
bits 64
long_mode_entry:
    ; Reload all 64-bit data segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up 64-bit stack
    mov rsp, stack_top

    ; Pass Multiboot2 info to kernel_main(magic, info_addr)
    mov edi, [rel mb2_magic]
    mov esi, [rel mb2_info_ptr]

    ; Call 64-bit kernel main
    call kernel_main

    ; Halt if kernel_main ever returns
    cli
.halt_loop:
    hlt
    jmp .halt_loop

; ------------------------------------------------------------------------------
; 64-bit Global Descriptor Table (GDT)
; ------------------------------------------------------------------------------
section .rodata
align 16
gdt64:
    dq 0 ; 0x00: Null Descriptor
    ; 0x08: 64-bit Code Segment (Executable, Readable, Long Mode 64-bit, Ring 0)
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
    ; 0x10: 64-bit Data Segment (Writable, Ring 0)
    dq (1 << 41) | (1 << 44) | (1 << 47)
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dd gdt64

; ------------------------------------------------------------------------------
; Page Tables and Stack in BSS Section
; ------------------------------------------------------------------------------
section .bss
align 4096
pml4_table:
    resb 4096
pdpt_table:
    resb 4096
pd_table:
    resb 4096

align 16
stack_bottom:
    resb 65536 ; 64 KB Stack
stack_top:

mb2_magic:
    resd 1
mb2_info_ptr:
    resd 1
