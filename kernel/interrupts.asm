; ==============================================================================
; Greenhouse OS 1.1.0 - x86_64 Interrupt Service Routine (ISR) Stubs
; ==============================================================================
default rel
bits 64

extern interrupt_dispatch

section .text

; Macro for exceptions without error code (push dummy 0 error code)
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0        ; Dummy error code
    push qword %1       ; Interrupt number
    jmp isr_common_stub
%endmacro

; Macro for exceptions with CPU error code (error code pushed by CPU)
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push qword %1       ; Interrupt number
    jmp isr_common_stub
%endmacro

; CPU Exceptions 0..31
ISR_NOERRCODE 0   ; 0x00: Division By Zero
ISR_NOERRCODE 1   ; 0x01: Debug Exception
ISR_NOERRCODE 2   ; 0x02: Non-Maskable Interrupt (NMI)
ISR_NOERRCODE 3   ; 0x03: Breakpoint (INT 3)
ISR_NOERRCODE 4   ; 0x04: Overflow (INTO)
ISR_NOERRCODE 5   ; 0x05: Bound Range Exceeded (BOUND)
ISR_NOERRCODE 6   ; 0x06: Invalid Opcode (UD2)
ISR_NOERRCODE 7   ; 0x07: Device Not Available (No Math Coprocessor)
ISR_ERRCODE   8   ; 0x08: Double Fault
ISR_NOERRCODE 9   ; 0x09: Coprocessor Segment Overrun
ISR_ERRCODE   10  ; 0x0A: Invalid TSS
ISR_ERRCODE   11  ; 0x0B: Segment Not Present
ISR_ERRCODE   12  ; 0x0C: Stack-Segment Fault
ISR_ERRCODE   13  ; 0x0D: General Protection Fault (#GP)
ISR_ERRCODE   14  ; 0x0E: Page Fault (#PF)
ISR_NOERRCODE 15  ; 0x0F: Reserved Exception
ISR_NOERRCODE 16  ; 0x10: x87 Floating-Point Exception (#MF)
ISR_ERRCODE   17  ; 0x11: Alignment Check (#AC)
ISR_NOERRCODE 18  ; 0x12: Machine Check (#MC)
ISR_NOERRCODE 19  ; 0x13: SIMD Floating-Point Exception (#XM/#XF)
ISR_NOERRCODE 20  ; 0x14: Virtualization Exception (#VE)
ISR_ERRCODE   21  ; 0x15: Control Protection Exception (#CP)
ISR_NOERRCODE 22  ; 0x16: Reserved
ISR_NOERRCODE 23  ; 0x17: Reserved
ISR_NOERRCODE 24  ; 0x18: Reserved
ISR_NOERRCODE 25  ; 0x19: Reserved
ISR_NOERRCODE 26  ; 0x1A: Reserved
ISR_NOERRCODE 27  ; 0x1B: Reserved
ISR_NOERRCODE 28  ; 0x1C: Hypervisor Injection Exception (#HV)
ISR_ERRCODE   29  ; 0x1D: VMM Communication Exception (#VC)
ISR_ERRCODE   30  ; 0x1E: Security Exception (#SX)
ISR_NOERRCODE 31  ; 0x1F: Reserved

; IRQs and General Interrupts 32..255
%assign i 32
%rep 224
ISR_NOERRCODE i
%assign i i+1
%endrep

; ------------------------------------------------------------------------------
; Common ISR Handler Stub
; ------------------------------------------------------------------------------
isr_common_stub:
    ; Save all 64-bit general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to interrupt frame in RDI (1st arg in SysV ABI)
    mov rdi, rsp

    ; Ensure Direction Flag is clear for C calling convention
    cld

    ; Call C interrupt dispatcher (returns active rsp in RAX)
    call interrupt_dispatch
    mov rsp, rax

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Discard interrupt number and error code
    add rsp, 16

    ; Return from interrupt (Ring 0 or Ring 3)
    iretq

; ------------------------------------------------------------------------------
; GDT & TSS Loading Functions
; ------------------------------------------------------------------------------
global gdt_load_flush
gdt_load_flush:
    lgdt [rdi]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    push qword 0x08
    lea rax, [rel .reload_cs]
    push rax
    retfq
.reload_cs:
    ret

global tss_load
tss_load:
    ltr di
    ret

; ------------------------------------------------------------------------------
; IDT Pointer Loading Function: void idt_load(void* idt_ptr)
; ------------------------------------------------------------------------------
global idt_load
idt_load:
    lidt [rdi]
    ret

; ------------------------------------------------------------------------------
; 256-Entry ISR Pointer Table
; ------------------------------------------------------------------------------
section .rodata
align 16
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dq isr%+i
%assign i i+1
%endrep
