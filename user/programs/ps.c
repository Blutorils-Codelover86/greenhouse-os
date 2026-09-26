#include "stdio.h"
#include "unistd.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("Greenhouse OS Userland Process Status\n");
    printf("Current PID: %d (Running in Ring 3 User Space)\n", getpid());
    return 0;
}
