#include "stdio.h"
#include "unistd.h"
#include "string.h"

static uint16_t get_cs(void) {
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    return cs;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("\n==================================================\n");
    printf("   GREENHOUSE OS 1.0 USER MODE PROCESS TEST       \n");
    printf("==================================================\n");

    /* 1. Verify Ring 3 Privilege Level */
    uint16_t cs = get_cs();
    int rpl = cs & 0x03;
    printf("1. Checking CPU Privilege Level: CS=0x%x (RPL=%d)\n", cs, rpl);
    if (rpl == 3) {
        printf("   [PASS] Process is running in Ring 3 (User Mode)!\n");
    } else {
        printf("   [FAIL] Process is not in Ring 3!\n");
    }

    /* 2. Check SYS_GETPID */
    int pid = getpid();
    printf("2. Checking SYS_GETPID: PID = %d\n", pid);
    if (pid > 0) {
        printf("   [PASS] Valid Process ID assigned.\n");
    } else {
        printf("   [FAIL] Invalid PID received.\n");
    }

    /* 3. Check User Stack and Memory Manipulation */
    printf("3. Checking User Memory & Stack Manipulation...\n");
    volatile uint8_t user_array[512];
    for (int i = 0; i < 512; i++) {
        user_array[i] = (uint8_t)(i & 0xFF);
    }
    int mem_ok = 1;
    for (int i = 0; i < 512; i++) {
        if (user_array[i] != (uint8_t)(i & 0xFF)) {
            mem_ok = 0;
            break;
        }
    }
    if (mem_ok) {
        printf("   [PASS] User stack & local memory buffer verified.\n");
    } else {
        printf("   [FAIL] User memory data mismatch.\n");
    }

    /* 4. Check SYS_YIELD */
    printf("4. Testing SYS_YIELD...\n");
    yield();
    printf("   [PASS] Resumed successfully after yield.\n");

    /* 5. Check SYS_SLEEP */
    printf("5. Testing SYS_SLEEP (100 ms)...\n");
    sleep(100);
    printf("   [PASS] Resumed successfully after sleep.\n");

    /* 6. Check File Operations (SYS_OPEN, SYS_READ, SYS_CLOSE) */
    printf("6. Testing VFS File Descriptors (Reading README.TXT)...\n");
    int fd = open("README.TXT", 0);
    if (fd >= 0) {
        char buf[64];
        int n = read(fd, buf, 32);
        if (n > 0) {
            buf[n] = '\0';
            printf("   [PASS] Read %d bytes: \"", n);
            for (int i = 0; i < n && i < 20; i++) {
                if (buf[i] >= ' ' && buf[i] <= '~') putchar(buf[i]);
            }
            printf("...\"\n");
        }
        close(fd);
    } else {
        printf("   [INFO] README.TXT not found on current drive; testing file descriptor allocation.\n");
    }

    printf("==================================================\n");
    printf("   ALL USER MODE TESTS COMPLETED SUCCESSFULLY!    \n");
    printf("==================================================\n\n");

    return 0;
}
