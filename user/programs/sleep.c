#include "stdio.h"
#include "unistd.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("[SLEEP.ELF] PID %d going to sleep for 300 ms...\n", getpid());
    sleep(300);
    printf("[SLEEP.ELF] PID %d is awake!\n", getpid());
    return 0;
}
