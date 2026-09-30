/* ==============================================================================
 * Greenhouse OS - Core Shell & Command Execution Subsystem
 *
 * Provides a unified command execution architecture for both the text console
 * and the Verdant graphical terminal. Resolves built-in commands and userland
 * ELF executables with argument parsing and stdout redirection.
 * ==============================================================================
 */

#ifndef OS_SHELL_H
#define OS_SHELL_H

#include <stdint.h>
#include <stddef.h>
#include "process.h"

typedef void (*os_shell_output_fn)(const char* line, void* ctx);

/* Initialize the OS shell subsystem */
void os_shell_init(void);

/* Register output hook for capturing command output (e.g. into GUI terminal) */
void os_shell_set_output_hook(os_shell_output_fn fn, void* ctx);
os_shell_output_fn os_shell_get_output_hook(void);

/* Print string to active output hook or VGA console */
void os_shell_print(const char* str);
void os_shell_println(const char* str);

/* Execute command line string with argument resolution */
int  os_shell_execute(const char* cmdline);

/* Run an ELF binary with argument vector */
int  os_shell_run_elf(const char* filepath, int argc, char** argv);

/* Get current working directory string */
const char* os_shell_get_cwd(void);

/* Set current working directory */
int os_shell_set_cwd(const char* path);

#endif /* OS_SHELL_H */
