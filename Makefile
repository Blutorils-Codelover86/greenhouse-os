# ==============================================================================
# Greenhouse OS 1.1.0 - Build System
# ==============================================================================

export PATH := /home/blu/.local/bin:/home/blu/.local/usr/bin:$(PATH)
export LD_LIBRARY_PATH := /home/blu/.local/usr/lib:/home/blu/.local/usr/lib64:$(LD_LIBRARY_PATH)
export QEMU_MODULE_DIR := /home/blu/.local/usr/lib/qemu

CC := gcc
NASM := nasm
LD := ld
GRUB_MKRESCUE := grub-mkrescue
QEMU = /usr/bin/qemu-system-x86_64

CFLAGS := -ffreestanding -m64 -O2 -Wall -Wextra -fno-stack-protector -fno-builtin -mno-red-zone -fno-pie -fno-pic -mcmodel=small -mgeneral-regs-only
NASMFLAGS := -f elf64
LDFLAGS := -n -T kernel/linker.ld

GRAPHICS_OBJS := kernel/graphics/framebuffer.o kernel/graphics/vbe.o kernel/graphics/font.o kernel/graphics/graphics.o kernel/graphics/gfx_test.o
INPUT_OBJS    := kernel/input/input.o kernel/input/kbd.o kernel/input/mouse.o
GUI_OBJS      := kernel/gui/renderer.o kernel/gui/shell.o kernel/gui/morph.o kernel/gui/surface.o kernel/gui/compositor.o kernel/gui/launcher.o kernel/gui/rail.o kernel/gui/berry_surface.o kernel/gui/filebrowser.o kernel/gui/sysmon.o kernel/gui/canvas_surface.o kernel/gui/settings_surface.o kernel/gui/guiterm.o kernel/gui/cursor.o kernel/gui/widget.o kernel/gui/window.o kernel/gui/wm.o kernel/gui/verdant.o kernel/gui/gui.o

KERNEL_OBJS := kernel/boot.o kernel/interrupts.o kernel/gdt.o kernel/pmm.o kernel/vmm.o kernel/heap.o kernel/process.o kernel/syscall.o kernel/elf.o kernel/block.o kernel/ata.o kernel/vfs.o kernel/ramfs.o kernel/fat32.o $(GRAPHICS_OBJS) $(INPUT_OBJS) $(GUI_OBJS) kernel/kernel.o
BIN := iso/boot/greenhouse.bin
ISO := greenhouse.iso

USER_CFLAGS := -ffreestanding -m64 -O2 -Wall -Wextra -fno-stack-protector -fno-builtin -mno-red-zone -fno-pie -fno-pic -mcmodel=small -Iuser/libc
USER_LIBC_OBJS := user/libc/crt0.o user/libc/syscall.o user/libc/string.o user/libc/unistd.o user/libc/stdio.o
USER_PROGRAMS := user/bin/HELLO.ELF user/bin/ECHO.ELF user/bin/CAT.ELF user/bin/LS.ELF user/bin/SLEEP.ELF user/bin/PS.ELF user/bin/TEST.ELF user/bin/GFX.ELF

.PHONY: all clean iso run run-serial run-gui test test-gui test-gfx-userland \
        test-gfx-depths userland disk.img

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

user/bin/GFX.ELF: user/programs/gfx.o user/libc/gui.o $(USER_LIBC_OBJS) user/linker.ld
	@mkdir -p user/bin
	@echo "[LD] Linking $@"
	$(LD) -n -T user/linker.ld -o $@ user/programs/gfx.o user/libc/gui.o $(USER_LIBC_OBJS)

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

# Subdirectory sources: the same flags, with the directory created on demand.
kernel/graphics/%.o: kernel/graphics/%.c
	@mkdir -p kernel/graphics
	@echo "[GCC] Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

kernel/input/%.o: kernel/input/%.c
	@mkdir -p kernel/input
	@echo "[GCC] Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

kernel/gui/%.o: kernel/gui/%.c
	@mkdir -p kernel/gui
	@echo "[GCC] Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

qemu: $(ISO) disk.img
	$(QEMU) -boot d -cdrom $(ISO) -drive file=disk.img,format=raw,index=0,media=disk -m 256M -vga std -display gtk

run: qemu

run-gui: qemu

run-sdl: $(ISO) disk.img
	$(QEMU) -boot d -cdrom $(ISO) -drive file=disk.img,format=raw,index=0,media=disk -m 256M -vga std -display sdl

run-curses: $(ISO) disk.img
	$(QEMU) -boot d -cdrom $(ISO) -drive file=disk.img,format=raw,index=0,media=disk -m 256M -vga std -display curses

run-serial: $(ISO) disk.img
	$(QEMU) -boot d -cdrom $(ISO) -drive file=disk.img,format=raw,index=0,media=disk -m 256M -vga std -serial stdio -display none

# The tests boot QEMU themselves; the image has to carry the userland binaries
# for the GFX userland test to run.
disk.img: userland
	python3 create_disk.py

test: $(ISO) disk.img
	python3 test_kernel.py
	python3 test_smoke.py
	python3 test_mouse.py
	python3 test_gfx.py gfx
	python3 test_gfx.py gui
	$(MAKE) test-gfx-depths
	python3 test_gfx_userland.py

test-gfx-userland: $(ISO) disk.img
	python3 test_gfx_userland.py

# Shallow pixel formats take a different path through every pixel write and read
# and use a different channel layout, so they get their own runs. The trailing
# argument holds the finished test frame on screen for the screendump.
test-gfx-depths: $(ISO)
	python3 test_gfx.py gfx 640 480 16 10
	python3 test_gfx.py gfx 800 600 24 10

test-gui: $(ISO)
	python3 test_gui.py

clean:
	@echo "[CLEAN] Removing object files, binaries and ISO..."
	rm -f kernel/*.o kernel/graphics/*.o kernel/input/*.o kernel/gui/*.o \
	      user/libc/*.o user/programs/*.o user/bin/* $(BIN) $(ISO)
