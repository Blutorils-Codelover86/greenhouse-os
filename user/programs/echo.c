#include "stdio.h"
#include "unistd.h"

int main(int argc, char** argv) {
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            printf("%s%s", argv[i], (i + 1 < argc) ? " " : "\n");
        }
        return 0;
    }

    printf("Echo Utility (Ring 3)\n");
    printf("Greenhouse OS Userland Echo Active.\n");
    return 0;
}
