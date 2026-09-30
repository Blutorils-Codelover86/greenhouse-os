/* ==============================================================================
 * Greenhouse OS - Core Shell & Command Execution Subsystem (Implementation)
 * ==============================================================================
 */

#include "os_shell.h"
#include "vfs.h"
#include "pmm.h"
#include "heap.h"
#include "elf.h"
#include "cpu.h"
#include "syscall.h"
#include "version.h"

extern void serial_put_char(char c);
extern void kernel_print(const char* text);
extern void kernel_put_char(char c);
extern uint64_t timer_get_ticks(void);
extern uint64_t timer_get_uptime_seconds(void);
extern void verdant_request_exit(int reason);
extern int  verdant_is_running(void);

static os_shell_output_fn g_shell_out_fn = NULL;

static void*              g_shell_out_ctx = NULL;

static char g_cwd[128] = "C:\\";

static char g_stdout_line_buf[256];
static int  g_stdout_line_len = 0;

static void os_shell_write_hook(const char* buf, size_t count) {
    if (!buf || count == 0) return;
    for (size_t i = 0; i < count; i++) {
        char c = buf[i];
        if (c == '\n') {
            g_stdout_line_buf[g_stdout_line_len] = '\0';
            os_shell_println(g_stdout_line_buf);
            g_stdout_line_len = 0;
            g_stdout_line_buf[0] = '\0';
        } else if (c == '\r') {
            /* ignore CR */
        } else if ((uint8_t)c >= 32 && (uint8_t)c < 127) {
            if (g_stdout_line_len < (int)sizeof(g_stdout_line_buf) - 1) {
                g_stdout_line_buf[g_stdout_line_len++] = c;
            } else {
                g_stdout_line_buf[g_stdout_line_len] = '\0';
                os_shell_println(g_stdout_line_buf);
                g_stdout_line_len = 0;
                g_stdout_line_buf[g_stdout_line_len++] = c;
            }
        }
    }
}

/* ------------------------------------------------------------------------------
 * String utilities
 * -------------------------------------------------------------------------- */

static int sh_strlen(const char* s) {
    int l = 0;
    while (s && s[l]) l++;
    return l;
}

static void sh_strcpy(char* dst, int cap, const char* src) {
    int i = 0;
    if (src) {
        for (; src[i] && i < cap - 1; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int sh_streq_nocase(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

static int sh_endswith_nocase(const char* s, const char* suffix) {
    if (!s || !suffix) return 0;
    int sl = sh_strlen(s);
    int sufl = sh_strlen(suffix);
    if (sl < sufl) return 0;
    const char* p = s + (sl - sufl);
    return sh_streq_nocase(p, suffix);
}

static void sh_put_uint(char* buf, int* n, int cap, uint64_t v) {
    char tmp[24];
    int tl = 0;
    do { tmp[tl++] = (char)('0' + (v % 10)); v /= 10; } while (v);
    while (tl && *n < cap - 1) buf[(*n)++] = tmp[--tl];
    buf[*n] = '\0';
}

/* ------------------------------------------------------------------------------
 * Output primitives
 * -------------------------------------------------------------------------- */

void os_shell_init(void) {
    g_shell_out_fn = NULL;
    g_shell_out_ctx = NULL;
    sh_strcpy(g_cwd, sizeof(g_cwd), "C:\\");
}

void os_shell_set_output_hook(os_shell_output_fn fn, void* ctx) {
    g_shell_out_fn = fn;
    g_shell_out_ctx = ctx;
}

os_shell_output_fn os_shell_get_output_hook(void) {
    return g_shell_out_fn;
}

const char* os_shell_get_cwd(void) {
    process_t* cur = process_get_current();
    if (cur && cur->cwd[0]) {
        return cur->cwd;
    }
    return g_cwd;
}

int os_shell_set_cwd(const char* path) {
    if (!path || !*path) return -1;
    vfs_node_t* node = vfs_resolve_path(os_shell_get_cwd(), path);
    if (!node || !(node->flags & VFS_DIRECTORY)) {
        return -1;
    }

    if (path[1] == ':') {
        sh_strcpy(g_cwd, sizeof(g_cwd), path);
    } else {
        char full[128];
        sh_strcpy(full, sizeof(full), os_shell_get_cwd());
        int len = sh_strlen(full);
        if (len > 0 && full[len - 1] != '\\' && full[len - 1] != '/') {
            full[len++] = '\\';
            full[len] = '\0';
        }
        sh_strcpy(full + len, sizeof(full) - len, path);
        sh_strcpy(g_cwd, sizeof(g_cwd), full);
    }

    process_t* cur = process_get_current();
    if (cur) {
        sh_strcpy(cur->cwd, sizeof(cur->cwd), g_cwd);
    }
    return 0;
}

void os_shell_println(const char* str) {
    if (g_shell_out_fn) {
        g_shell_out_fn(str ? str : "", g_shell_out_ctx);
    } else {
        kernel_print(str ? str : "");
        kernel_print("\n");
    }

    /* Always mirror to serial console */
    if (str) {
        for (int i = 0; str[i]; i++) serial_put_char(str[i]);
    }
    serial_put_char('\r');
    serial_put_char('\n');
}

void os_shell_print(const char* str) {
    if (!str) return;

    /* Split string into lines */
    char line[256];
    int li = 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] == '\n') {
            line[li] = '\0';
            os_shell_println(line);
            li = 0;
        } else if (str[i] == '\r') {
            /* ignore CR */
        } else {
            if (li < (int)sizeof(line) - 1) {
                line[li++] = str[i];
            } else {
                line[li] = '\0';
                os_shell_println(line);
                li = 0;
                line[li++] = str[i];
            }
        }
    }
    if (li > 0) {
        line[li] = '\0';
        os_shell_println(line);
    }
}

/* ------------------------------------------------------------------------------
 * ELF Executable Runner
 * -------------------------------------------------------------------------- */

int os_shell_run_elf(const char* filepath, int argc, char** argv) {
    if (!filepath || !*filepath) return -1;

    char msg[128];
    int mn = 0;
    sh_strcpy(msg, sizeof(msg), "Loading ELF executable: ");
    mn = sh_strlen(msg);
    sh_strcpy(msg + mn, sizeof(msg) - mn, filepath);
    mn = sh_strlen(msg);
    sh_strcpy(msg + mn, sizeof(msg) - mn, "...");
    os_shell_println(msg);

    process_t* new_proc = NULL;
    int err = elf_load_executable_args(filepath, argc, argv, &new_proc);
    if (err != 0 || !new_proc) {
        msg[0] = '\0';
        mn = 0;
        sh_strcpy(msg, sizeof(msg), "Failed to load ELF executable (error ");
        mn = sh_strlen(msg);
        sh_put_uint(msg, &mn, sizeof(msg), (uint64_t)(err < 0 ? -err : err));
        sh_strcpy(msg + mn, sizeof(msg) - mn, ").");
        os_shell_println(msg);
        return -1;
    }

    msg[0] = '\0';
    mn = 0;
    sh_strcpy(msg, sizeof(msg), "Spawned process '");
    mn = sh_strlen(msg);
    sh_strcpy(msg + mn, sizeof(msg) - mn, new_proc->name);
    mn = sh_strlen(msg);
    sh_strcpy(msg + mn, sizeof(msg) - mn, "' (PID: ");
    mn = sh_strlen(msg);
    sh_put_uint(msg, &mn, sizeof(msg), (uint64_t)new_proc->pid);
    sh_strcpy(msg + mn, sizeof(msg) - mn, ") in Ring 3 User Mode.");
    os_shell_println(msg);

    /* Hook stdout write calls from the user process */
    g_stdout_line_len = 0;
    g_stdout_line_buf[0] = '\0';
    syscall_set_write_hook(os_shell_write_hook);

    /* Yield CPU to let process run to completion */
    while (new_proc->state != PROCESS_TERMINATED) {
        process_yield();
    }

    /* Flush any pending buffer */
    if (g_stdout_line_len > 0) {
        g_stdout_line_buf[g_stdout_line_len] = '\0';
        os_shell_println(g_stdout_line_buf);
        g_stdout_line_len = 0;
        g_stdout_line_buf[0] = '\0';
    }

    syscall_set_write_hook(NULL);
    return new_proc->exit_code;
}

/* ------------------------------------------------------------------------------
 * Built-in Commands
 * -------------------------------------------------------------------------- */

static void cmd_builtin_help(void) {
    os_shell_println("Greenhouse OS Commands:");
    os_shell_println("  help            Display this command list");
    os_shell_println("  clear / cls     Clear the terminal screen");
    os_shell_println("  ver             Display OS version & build information");
    os_shell_println("  cpu             Display processor model & vendor (CPUID)");
    os_shell_println("  mem             Display physical memory allocation");
    os_shell_println("  heap            Display kernel heap allocator statistics");
    os_shell_println("  pwd             Display current working directory");
    os_shell_println("  cd <path>       Change working directory");
    os_shell_println("  dir / ls        List files and directories in current drive");
    os_shell_println("  cat <file>      Display file contents");
    os_shell_println("  type <file>     Display file contents");
    os_shell_println("  echo <text>     Print text or redirect to file (echo msg > file)");
    os_shell_println("  mkdir <name>    Create a new directory");
    os_shell_println("  rm / del <file> Delete a file");
    os_shell_println("  ps              Display process table status");
    os_shell_println("  sleep <sec>     Sleep for specified duration");
    os_shell_println("  vol / disks     Display mounted filesystems (C:, R:)");
    os_shell_println("  uptime / ticks  Display system uptime and PIT ticks");
    os_shell_println("  run <file.elf>  Execute userland ELF binary directly");
    os_shell_println("Userland Programs on C:\\:");
    os_shell_println("  hello           Run HELLO.ELF (Ring 3 greeting & PID)");
    os_shell_println("  echo <args...>  Run ECHO.ELF with arguments");
    os_shell_println("  cat <file>      Run CAT.ELF (Ring 3 file reader)");
    os_shell_println("  ls              Run LS.ELF (Ring 3 directory utility)");
    os_shell_println("  ps              Run PS.ELF (Ring 3 process status)");
    os_shell_println("  sleep <sec>     Run SLEEP.ELF (Ring 3 sleep test)");
    os_shell_println("  test            Run TEST.ELF (Ring 3 syscall validation)");
    os_shell_println("  gfx             Run GFX.ELF (Ring 3 2D graphics demo)");
}

static void cmd_builtin_cpu(void) {
    const CPUInfo* cpu = get_cpu_info();
    if (cpu && cpu->brand[0]) {
        char buf[128];
        sh_strcpy(buf, sizeof(buf), "Processor: ");
        int n = sh_strlen(buf);
        sh_strcpy(buf + n, sizeof(buf) - n, cpu->brand);
        os_shell_println(buf);

        buf[0] = '\0';
        n = 0;
        sh_strcpy(buf, sizeof(buf), "Vendor: ");
        n = sh_strlen(buf);
        sh_strcpy(buf + n, sizeof(buf) - n, cpu->vendor);
        n = sh_strlen(buf);
        sh_strcpy(buf + n, sizeof(buf) - n, "  Family: ");
        n = sh_strlen(buf);
        sh_put_uint(buf, &n, sizeof(buf), cpu->family);
        sh_strcpy(buf + n, sizeof(buf) - n, "  Model: ");
        n = sh_strlen(buf);
        sh_put_uint(buf, &n, sizeof(buf), cpu->model);
        os_shell_println(buf);
    } else {
        os_shell_println("Processor: x86_64 Compatible (CPUID N/A)");
    }
}

static void cmd_builtin_mem(void) {
    pmm_stats_t st = pmm_get_stats();
    char buf[128];
    int n = 0;
    sh_strcpy(buf, sizeof(buf), "Physical RAM: ");
    n = sh_strlen(buf);
    sh_put_uint(buf, &n, sizeof(buf), (st.used_frames * 4096) / (1024 * 1024));
    sh_strcpy(buf + n, sizeof(buf) - n, " MiB used / ");
    n = sh_strlen(buf);
    sh_put_uint(buf, &n, sizeof(buf), (st.total_frames * 4096) / (1024 * 1024));
    sh_strcpy(buf + n, sizeof(buf) - n, " MiB total");
    os_shell_println(buf);

    heap_stats_t hp = heap_get_stats();
    buf[0] = '\0';
    n = 0;
    sh_strcpy(buf, sizeof(buf), "Kernel Heap:  ");
    n = sh_strlen(buf);
    sh_put_uint(buf, &n, sizeof(buf), hp.used_bytes / 1024);
    sh_strcpy(buf + n, sizeof(buf) - n, " KiB used / ");
    n = sh_strlen(buf);
    sh_put_uint(buf, &n, sizeof(buf), hp.total_bytes / 1024);
    sh_strcpy(buf + n, sizeof(buf) - n, " KiB total");
    os_shell_println(buf);
}

static void cmd_builtin_dir(const char* arg) {
    const char* path = (arg && *arg) ? arg : os_shell_get_cwd();
    vfs_node_t* node = vfs_resolve_path(os_shell_get_cwd(), path);
    if (!node || !(node->flags & VFS_DIRECTORY)) {
        os_shell_println("Cannot open directory.");
        return;
    }

    char hdr[128];
    sh_strcpy(hdr, sizeof(hdr), "Directory of ");
    int hn = sh_strlen(hdr);
    sh_strcpy(hdr + hn, sizeof(hdr) - hn, path);
    os_shell_println(hdr);

    vfs_dirent_t ent;
    for (uint32_t i = 0; i < 64; i++) {
        if (vfs_readdir(node, i, &ent) != 0) break;

        char line[128];
        int ln = 0;
        if (ent.is_dir) {
            sh_strcpy(line, sizeof(line), "  [DIR]  ");
        } else {
            sh_strcpy(line, sizeof(line), "  [FILE] ");
        }
        ln = sh_strlen(line);
        sh_strcpy(line + ln, sizeof(line) - ln, ent.name);
        ln = sh_strlen(line);

        if (!ent.is_dir) {
            while (ln < 30) line[ln++] = ' ';
            line[ln] = '\0';
            sh_put_uint(line, &ln, sizeof(line), (uint64_t)ent.size);
            sh_strcpy(line + ln, sizeof(line) - ln, " bytes");
        }
        os_shell_println(line);
    }
}

static void cmd_builtin_type(const char* filename) {
    if (!filename || !*filename) {
        os_shell_println("Usage: type <filename>");
        return;
    }

    vfs_node_t* file = vfs_resolve_path(os_shell_get_cwd(), filename);
    if (!file || (file->flags & VFS_DIRECTORY)) {
        /* Try with .TXT fallback */
        char alt[64];
        sh_strcpy(alt, sizeof(alt), filename);
        int al = sh_strlen(alt);
        sh_strcpy(alt + al, sizeof(alt) - al, ".TXT");
        file = vfs_resolve_path(os_shell_get_cwd(), alt);
    }

    if (!file || (file->flags & VFS_DIRECTORY)) {
        os_shell_println("The system cannot find the file specified.");
        return;
    }

    char buf[512];
    uint32_t offset = 0;
    while (offset < file->size) {
        uint32_t to_read = sizeof(buf) - 1;
        if (offset + to_read > file->size) to_read = file->size - offset;
        int n = vfs_read(file, offset, to_read, (uint8_t*)buf);
        if (n <= 0) break;
        buf[n] = '\0';
        os_shell_print(buf);
        offset += n;
    }
}

static void cmd_builtin_echo(const char* text) {
    if (!text || !*text) {
        os_shell_println("");
        return;
    }

    /* Check for redirection '>' */
    const char* redir = 0;
    for (int i = 0; text[i]; i++) {
        if (text[i] == '>') { redir = text + i; break; }
    }

    if (redir) {
        int append = (redir[1] == '>');
        const char* fname = append ? redir + 2 : redir + 1;
        while (*fname == ' ') fname++;

        char clean_file[64];
        int fi = 0;
        while (*fname && *fname != ' ' && fi < 63) clean_file[fi++] = *fname++;
        clean_file[fi] = '\0';

        char content[256];
        int clen = (int)(redir - text);
        while (clen > 0 && text[clen - 1] == ' ') clen--;
        for (int i = 0; i < clen && i < (int)sizeof(content) - 2; i++) content[i] = text[i];
        content[clen++] = '\n';
        content[clen] = '\0';

        vfs_node_t* target = vfs_resolve_path(os_shell_get_cwd(), clean_file);
        if (!target) {
            vfs_create_file(os_shell_get_cwd(), clean_file);
            target = vfs_resolve_path(os_shell_get_cwd(), clean_file);
        }

        if (!target || (target->flags & VFS_DIRECTORY)) {
            os_shell_println("Error: Cannot create redirection file.");
            return;
        }

        uint32_t off = append ? target->size : 0;
        vfs_write(target, off, clen, (const uint8_t*)content);
        return;
    }

    os_shell_println(text);
}

/* ------------------------------------------------------------------------------
 * Command Line Execution Dispatcher
 * -------------------------------------------------------------------------- */

int os_shell_execute(const char* cmdline) {
    if (!cmdline) return 0;
    while (*cmdline == ' ') cmdline++;
    if (!*cmdline) return 0;

    /* Parse verb and arguments */
    char verb[64];
    int vi = 0;
    while (*cmdline && *cmdline != ' ' && vi < (int)sizeof(verb) - 1) {
        verb[vi++] = *cmdline++;
    }
    verb[vi] = '\0';

    while (*cmdline == ' ') cmdline++;
    const char* rest = cmdline;

    /* Tokenize argv */
    #define MAX_ARGS 16
    char* argv[MAX_ARGS];
    char  arg_pool[256];
    int   argc = 0;
    int   pool_idx = 0;

    /* argv[0] is the verb */
    argv[argc++] = arg_pool + pool_idx;
    sh_strcpy(arg_pool + pool_idx, sizeof(arg_pool) - pool_idx, verb);
    pool_idx += sh_strlen(verb) + 1;

    const char* p = rest;
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ') p++;
        if (!*p) break;

        argv[argc++] = arg_pool + pool_idx;
        while (*p && *p != ' ' && pool_idx < (int)sizeof(arg_pool) - 1) {
            arg_pool[pool_idx++] = *p++;
        }
        arg_pool[pool_idx++] = '\0';
    }

    /* 1. Drive letter change e.g. "C:" or "R:" */
    if (vi == 2 && ((verb[0] >= 'A' && verb[0] <= 'Z') || (verb[0] >= 'a' && verb[0] <= 'z')) && verb[1] == ':') {
        char drv_root[4];
        drv_root[0] = verb[0];
        drv_root[1] = ':';
        drv_root[2] = '\\';
        drv_root[3] = '\0';
        if (os_shell_set_cwd(drv_root) == 0) {
            return 0;
        }
        os_shell_println("Drive not found.");
        return -1;
    }

    /* 2. Direct Built-in Keywords */
    if (sh_streq_nocase(verb, "LOGOUT") || sh_streq_nocase(verb, "EXIT") || sh_streq_nocase(verb, "QUIT")) {
        if (verdant_is_running()) {
            os_shell_println("Logging out of Verdant graphical session...");
            verdant_request_exit(3);
        } else {
            os_shell_println("Already in console text mode.");
        }
        return 0;
    }
    if (sh_streq_nocase(verb, "HELP") || sh_streq_nocase(verb, "?")) {

        cmd_builtin_help();
        return 0;
    }
    if (sh_streq_nocase(verb, "CLEAR") || sh_streq_nocase(verb, "CLS")) {
        /* If output hook exists, it can handle clearing or we output a blank banner */
        os_shell_println("");
        return 0;
    }
    if (sh_streq_nocase(verb, "VER") || sh_streq_nocase(verb, "VERSION")) {
        os_shell_println(GREENHOUSE_VERSION_LINE);
        return 0;
    }
    if (sh_streq_nocase(verb, "CPU") || sh_streq_nocase(verb, "CPUID")) {
        cmd_builtin_cpu();
        return 0;
    }
    if (sh_streq_nocase(verb, "MEM") || sh_streq_nocase(verb, "MEMORY")) {
        cmd_builtin_mem();
        return 0;
    }
    if (sh_streq_nocase(verb, "PWD")) {
        os_shell_println(os_shell_get_cwd());
        return 0;
    }
    if (sh_streq_nocase(verb, "CD")) {
        if (!*rest) {
            os_shell_println(os_shell_get_cwd());
            return 0;
        }
        if (sh_streq_nocase(rest, "..")) {
            char cur[128];
            sh_strcpy(cur, sizeof(cur), os_shell_get_cwd());
            int len = sh_strlen(cur);
            if (len > 3 && (cur[len - 1] == '\\' || cur[len - 1] == '/')) len--;
            while (len > 3 && cur[len - 1] != '\\' && cur[len - 1] != '/') len--;
            cur[len] = '\0';
            os_shell_set_cwd(cur);
            return 0;
        }
        if (os_shell_set_cwd(rest) != 0) {
            os_shell_println("Directory not found.");
            return -1;
        }
        return 0;
    }
    if (sh_streq_nocase(verb, "DIR")) {
        cmd_builtin_dir(rest);
        return 0;
    }
    if (sh_streq_nocase(verb, "TYPE")) {
        cmd_builtin_type(rest);
        return 0;
    }
    if (sh_streq_nocase(verb, "ECHO")) {
        /* If arguments contain redirection '>' or user wants shell echo */
        int has_redir = 0;
        for (int i = 0; rest[i]; i++) {
            if (rest[i] == '>') { has_redir = 1; break; }
        }
        if (has_redir) {
            cmd_builtin_echo(rest);
            return 0;
        }

        /* Check if ECHO.ELF is available on C:\ */
        vfs_node_t* echo_elf = vfs_resolve_path(NULL, "C:\\ECHO.ELF");
        if (echo_elf) {
            return os_shell_run_elf("C:\\ECHO.ELF", argc, argv);
        }
        cmd_builtin_echo(rest);
        return 0;
    }
    if (sh_streq_nocase(verb, "MKDIR") || sh_streq_nocase(verb, "MD")) {
        if (!*rest) {
            os_shell_println("Usage: mkdir <dirname>");
            return -1;
        }
        if (vfs_mkdir_path(os_shell_get_cwd(), rest) != 0) {
            os_shell_println("Failed to create directory.");
            return -1;
        }
        os_shell_println("Directory created.");
        return 0;
    }
    if (sh_streq_nocase(verb, "RM") || sh_streq_nocase(verb, "DEL") || sh_streq_nocase(verb, "UNLINK")) {
        if (!*rest) {
            os_shell_println("Usage: rm <filename>");
            return -1;
        }
        if (vfs_delete_path(os_shell_get_cwd(), rest) != 0) {
            os_shell_println("Failed to delete file.");
            return -1;
        }
        os_shell_println("File deleted.");
        return 0;
    }
    if (sh_streq_nocase(verb, "VOL") || sh_streq_nocase(verb, "DISKS")) {
        os_shell_println("Mounted Drives:");
        os_shell_println("  C:\\  [FAT32] Persistent Storage");
        os_shell_println("  R:\\  [RAMFS] Virtual RAM Disk");
        return 0;
    }
    if (sh_streq_nocase(verb, "UPTIME") || sh_streq_nocase(verb, "TICKS")) {
        char buf[64];
        int n = 0;
        sh_strcpy(buf, sizeof(buf), "Kernel Uptime: ");
        n = sh_strlen(buf);
        sh_put_uint(buf, &n, sizeof(buf), timer_get_uptime_seconds());
        sh_strcpy(buf + n, sizeof(buf) - n, " seconds (");
        n = sh_strlen(buf);
        sh_put_uint(buf, &n, sizeof(buf), timer_get_ticks());
        sh_strcpy(buf + n, sizeof(buf) - n, " ticks)");
        os_shell_println(buf);
        return 0;
    }
    if (sh_streq_nocase(verb, "RUN") || sh_streq_nocase(verb, "EXEC")) {
        if (!*rest) {
            os_shell_println("Usage: run <binary.elf> [args...]");
            return -1;
        }
        /* Pass remaining arguments (from argv[1] onwards) */
        char* sub_argv[MAX_ARGS];
        int sub_argc = 0;
        for (int i = 1; i < argc && sub_argc < MAX_ARGS; i++) {
            sub_argv[sub_argc++] = argv[i];
        }
        return os_shell_run_elf(argv[1], sub_argc, sub_argv);
    }

    /* 3. Check for specific common ELF commands or generic ELF resolution */
    /* Target check paths:
     * 1) verb as given (if ends in .elf)
     * 2) C:\verb.ELF
     * 3) cwd\verb.ELF
     */
    char test_path[128];

    /* A. If verb already ends in .elf */
    if (sh_endswith_nocase(verb, ".ELF")) {
        if (verb[1] == ':') {
            sh_strcpy(test_path, sizeof(test_path), verb);
        } else {
            test_path[0] = 'C'; test_path[1] = ':'; test_path[2] = '\\'; test_path[3] = '\0';
            sh_strcpy(test_path + 3, sizeof(test_path) - 3, verb);
        }
        vfs_node_t* n = vfs_resolve_path(NULL, test_path);
        if (n && !(n->flags & VFS_DIRECTORY)) {
            return os_shell_run_elf(test_path, argc, argv);
        }
    }

    /* B. Check C:\VERB.ELF */
    test_path[0] = 'C'; test_path[1] = ':'; test_path[2] = '\\'; test_path[3] = '\0';
    sh_strcpy(test_path + 3, sizeof(test_path) - 3, verb);
    int tlen = sh_strlen(test_path);
    sh_strcpy(test_path + tlen, sizeof(test_path) - tlen, ".ELF");

    vfs_node_t* elf_node = vfs_resolve_path(NULL, test_path);
    if (elf_node && !(elf_node->flags & VFS_DIRECTORY)) {
        return os_shell_run_elf(test_path, argc, argv);
    }

    /* C. Check in current directory */
    sh_strcpy(test_path, sizeof(test_path), os_shell_get_cwd());
    int cl = sh_strlen(test_path);
    if (cl > 0 && test_path[cl - 1] != '\\' && test_path[cl - 1] != '/') {
        test_path[cl++] = '\\';
        test_path[cl] = '\0';
    }
    sh_strcpy(test_path + cl, sizeof(test_path) - cl, verb);
    tlen = sh_strlen(test_path);
    sh_strcpy(test_path + tlen, sizeof(test_path) - tlen, ".ELF");

    elf_node = vfs_resolve_path(NULL, test_path);
    if (elf_node && !(elf_node->flags & VFS_DIRECTORY)) {
        return os_shell_run_elf(test_path, argc, argv);
    }

    /* 4. Special fallback: if verb is 'ls' and no LS.ELF, run dir */
    if (sh_streq_nocase(verb, "LS")) {
        cmd_builtin_dir(rest);
        return 0;
    }
    if (sh_streq_nocase(verb, "CAT")) {
        cmd_builtin_type(rest);
        return 0;
    }

    /* Command not found */
    char not_found[128];
    sh_strcpy(not_found, sizeof(not_found), "'");
    int nfl = sh_strlen(not_found);
    sh_strcpy(not_found + nfl, sizeof(not_found) - nfl, verb);
    nfl = sh_strlen(not_found);
    sh_strcpy(not_found + nfl, sizeof(not_found) - nfl, "' is not recognized as an internal or external command.");
    os_shell_println(not_found);
    os_shell_println("Type 'help' for a list of available commands.");
    return -1;
}
