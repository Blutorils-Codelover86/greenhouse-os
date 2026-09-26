# ==============================================================================
# Greenhouse OS 0.7 - Build System
# ==============================================================================

export PATH := /home/blu/.local/bin:/home/blu/.local/usr/bin:$(PATH)
export LD_LIBRARY_PATH := /home/blu/.local/usr/lib:/home/blu/.local/usr/lib64:$(LD_LIBRARY_PATH)

CC := gcc
NASM := nasm
LD := ld
GRUB_MKRESCUE := grub-mkrescue
QEMU := qemu-system-x86_64

CFLAGS := -ffreestanding -m64 -O2 -Wall -Wextra -fno-stack-protector -fno-builtin -mno-red-zone -fno-pie -fno-pic -mcmodel=small -mgeneral-regs-only
NASMFLAGS := -f elf64
LDFLAGS := -n -T kernel/linker.ld

KERNEL_OBJS := kernel/boot.o kernel/interrupts.o kernel/gdt.o kernel/pmm.o kernel/vmm.o kernel/heap.o kernel/process.o kernel/syscall.o kernel/elf.o kernel/block.o kernel/ata.o kernel/vfs.o kernel/ramfs.o kernel/fat32.o kernel/kernel.o
BIN := iso/boot/greenhouse.bin
ISO := greenhouse.iso

USER_CFLAGS := -ffreestanding -m64 -O2 -Wall -Wextra -fno-stack-protector -fno-builtin -mno-red-zone -fno-pie -fno-pic -mcmodel=small -Iuser/libc
USER_LIBC_OBJS := user/libc/crt0.o user/libc/syscall.o user/libc/string.o user/libc/unistd.o user/libc/stdio.o
USER_PROGRAMS := user/bin/HELLO.ELF user/bin/ECHO.ELF user/bin/CAT.ELF user/bin/LS.ELF user/bin/SLEEP.ELF user/bin/PS.ELF user/bin/TEST.ELF

.PHONY: all clean iso run run-serial test userland

all: userland $(ISO)

userland: $(USER_PROGRAMS)

user/libc/crt0.o: user/libc/crt0.asm
	@mkdir -p user/libc user/bin
	@echo "[NASM] Assembling $<"
	$(NASM) $(NASMFLAGS) $< -o $@

user/libc/%.o: user/libc/%.c
	@mkdir -p user/libc user/bin
	@echo "[GCC] Compiling $<"
	$(CC) $(USER_CFLAGS) -c $< -o $@

user/programs/%.o: user/programs/%.c
	@mkdir -p user/programs user/bin
	@echo "[GCC] Compiling $<"
	$(CC) $(USER_CFLAGS) -c $< -o $@

user/bin/HELLO.ELF: user/programs/hello.o $(USER_LIBC_OBJS) user/linker.ld
	@mkdir -p user/bin
	@echo "[LD] Linking $@"
	$(LD) -n -T user/linker.ld -o $@ user/programs/hello.o $(USER_LIBC_OBJS)

user/bin/ECHO.ELF: user/programs/echo.o $(USER_LIBC_OBJS) user/linker.ld
	@mkdir -p user/bin
	@echo "[LD] Linking $@"
	$(LD) -n -T user/linker.ld -o $@ user/programs/echo.o $(USER_LIBC_OBJS)

user/bin/CAT.ELF: user/programs/cat.o $(USER_LIBC_OBJS) user/linker.ld
	@mkdir -p user/bin
	@echo "[LD] Linking $@"
	$(LD) -n -T user/linker.ld -o $@ user/programs/cat.o $(USER_LIBC_OBJS)

user/bin/LS.ELF: user/programs/ls.o $(USER_LIBC_OBJS) user/linker.ld
	@mkdir -p user/bin
	@echo "[LD] Linking $@"
	$(LD) -n -T user/linker.ld -o $@ user/programs/ls.o $(USER_LIBC_OBJS)

user/bin/SLEEP.ELF: user/programs/sleep.o $(USER_LIBC_OBJS) user/linker.ld
	@mkdir -p user/bin
	@echo "[LD] Linking $@"
	$(LD) -n -T user/linker.ld -o $@ user/programs/sleep.o $(USER_LIBC_OBJS)

user/bin/PS.ELF: user/programs/ps.o $(USER_LIBC_OBJS) user/linker.ld
	@mkdir -p user/bin
	@echo "[LD] Linking $@"
	$(LD) -n -T user/linker.ld -o $@ user/programs/ps.o $(USER_LIBC_OBJS)

user/bin/TEST.ELF: user/programs/process_test.o $(USER_LIBC_OBJS) user/linker.ld
	@mkdir -p user/bin
	@echo "[LD] Linking $@"
	$(LD) -n -T user/linker.ld -o $@ user/programs/process_test.o $(USER_LIBC_OBJS)

$(ISO): $(BIN) iso/boot/grub/grub.cfg
	@echo "[GRUB] Generating Bootable ISO: $(ISO)"
	$(GRUB_MKRESCUE) -d /home/blu/.local/usr/lib/grub/i386-pc -o $(ISO) iso 2>/dev/null

$(BIN): $(KERNEL_OBJS) kernel/linker.ld
	@echo "[LD] Linking 64-bit Kernel Binary: $(BIN)"
	$(LD) $(LDFLAGS) -o $(BIN) $(KERNEL_OBJS)

kernel/boot.o: kernel/boot.asm
	@echo "[NASM] Assembling $<"
	$(NASM) $(NASMFLAGS) $< -o $@

kernel/interrupts.o: kernel/interrupts.asm
	@echo "[NASM] Assembling $<"
	$(NASM) $(NASMFLAGS) $< -o $@

kernel/%.o: kernel/%.c
	@echo "[GCC] Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

run: $(ISO)
	$(QEMU) -boot d -cdrom $(ISO) -hda disk.img -m 256M -display curses

run-serial: $(ISO)
	$(QEMU) -boot d -cdrom $(ISO) -hda disk.img -m 256M -serial stdio -display none

test: $(ISO)
	python3 test_kernel.py

clean:
	@echo "[CLEAN] Removing object files, binaries and ISO..."
	rm -f kernel/*.o user/libc/*.o user/programs/*.o user/bin/* $(BIN) $(ISO)
