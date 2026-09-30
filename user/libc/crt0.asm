; ==============================================================================
; Greenhouse OS 1.1.0 - Userland C Runtime Startup (crt0.asm)
; ==============================================================================
default rel
bits 64

global _start
extern main
extern exit

section .text
_start:
    ; Align stack to 16 bytes while preserving rdi (argc) and rsi (argv)
    push rbp
    mov rbp, rsp
    and rsp, -16

    call main

    ; Call exit(main_result)
    mov rdi, rax
    call exit

    ; Infinite loop if exit somehow returns
.halt:
    jmp .halt
