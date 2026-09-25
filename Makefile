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

OBJS := kernel/boot.o kernel/interrupts.o kernel/pmm.o kernel/vmm.o kernel/heap.o kernel/block.o kernel/ata.o kernel/vfs.o kernel/ramfs.o kernel/fat32.o kernel/kernel.o
BIN := iso/boot/greenhouse.bin
ISO := greenhouse.iso

.PHONY: all clean iso run run-serial test

all: $(ISO)

$(ISO): $(BIN) iso/boot/grub/grub.cfg
	@echo "[GRUB] Generating Bootable ISO: $(ISO)"
	$(GRUB_MKRESCUE) -d /home/blu/.local/usr/lib/grub/i386-pc -o $(ISO) iso 2>/dev/null

$(BIN): $(OBJS) kernel/linker.ld
	@echo "[LD] Linking 64-bit Kernel Binary: $(BIN)"
	$(LD) $(LDFLAGS) -o $(BIN) $(OBJS)

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
	$(QEMU) -cdrom $(ISO) -m 256M -display curses

run-serial: $(ISO)
	$(QEMU) -cdrom $(ISO) -m 256M -serial stdio -display none

test: $(ISO)
	python3 test_kernel.py

clean:
	@echo "[CLEAN] Removing object files and ISO..."
	rm -f kernel/*.o $(BIN) $(ISO)
