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
    ; Align stack to 16 bytes
    and rsp, -16

    ; Call main(argc=0, argv=NULL)
    xor rdi, rdi
    xor rsi, rsi
    call main

    ; Call exit(main_result)
    mov rdi, rax
    call exit

    ; Infinite loop if exit somehow returns
.halt:
    jmp .halt
