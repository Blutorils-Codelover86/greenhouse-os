#include "stdio.h"
#include "unistd.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    const char* target_file = "README.TXT";
    printf("[CAT.ELF] Opening '%s' via sys_open (Ring 3)...\n", target_file);

    int fd = open(target_file, 0); // Read only
    if (fd < 0) {
        printf("[CAT.ELF] Error: Could not open file '%s'\n", target_file);
        return 1;
    }

    char buf[256];
    int n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("[CAT.ELF] Read %d bytes successfully:\n", n);
        printf("----------------------------------------\n");
        print(buf);
        printf("\n----------------------------------------\n");
    } else {
        printf("[CAT.ELF] File is empty or read error.\n");
    }

    close(fd);
    printf("[CAT.ELF] File closed. Done.\n");
    return 0;
}
