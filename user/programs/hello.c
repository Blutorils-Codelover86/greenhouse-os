#include "stdio.h"
#include "unistd.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("\n[HELLO.ELF] Hello from Greenhouse OS 1.0 User Mode (Ring 3)!\n");
    printf("[HELLO.ELF] Process ID (PID): %d\n", getpid());
    printf("[HELLO.ELF] Syscalls active: write, getpid, exit.\n");
    return 0;
}
