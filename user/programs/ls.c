#include "stdio.h"
#include "unistd.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    char cwd[64];
    if (getcwd(cwd, sizeof(cwd)) == 0) {
        printf("[LS.ELF] Current Working Directory: %s\n", cwd);
    } else {
        printf("[LS.ELF] Unable to get current working directory.\n");
    }
    return 0;
}
