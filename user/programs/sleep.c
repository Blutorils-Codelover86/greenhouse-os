#include "stdio.h"
#include "unistd.h"

static int parse_int(const char* s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return v;
}

int main(int argc, char** argv) {
    int ms = 300;
    if (argc > 1 && argv[1]) {
        int v = parse_int(argv[1]);
        if (v > 0) {
            ms = (v <= 10) ? v * 1000 : v;
        }
    }

    printf("[SLEEP.ELF] PID %d going to sleep for %d ms...\n", getpid(), ms);
    sleep(ms);
    printf("[SLEEP.ELF] PID %d is awake!\n", getpid());
    return 0;
}
