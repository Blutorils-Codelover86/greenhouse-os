# Greenhouse OS

A 64-bit operating system engineered with preemptive multitasking, Ring 3 userland, multi-drive virtual filesystem (FAT32 & RAMFS), linear framebuffer graphics, and the **VERDANT** spatial graphical environment.

---

## Key Features

- **64-bit Long Mode Kernel**: Pure 64-bit x86_64 architecture with GDT/TSS, 256-entry IDT, 4-level paging (VMM), and physical page allocator (PMM).
- **Preemptive Multitasking & Userland**: Ring 3 user mode execution, interrupt-driven scheduler, and software interrupt syscall interface (`int 0x80`).
- **Filesystem Support**: Multi-drive VFS manager supporting `C:` (Persistent ATA FAT32) and `R:` (In-memory RAMFS).
- **Graphics Engine**: Linear Framebuffer with VBE / Bochs DISPI support, double buffering (`1024x768x32bpp`), custom alpha-blended glass rendering, and transparent font rasterizer.
- **Verdant Spatial Graphical Environment**:
  - Spatial non-rigid window manager with animated morph transitions.
  - Botanical glass design language with emerald (`#10B981`) and berry (`#F43F5E`) accents.
  - Radial Orbital Launcher with 6 application nodes.
  - Floating System Rail with live kernel telemetry (CPU ticks, memory, uptime).
  - Native applications: Berry AI Assistant, Graphical Terminal, FAT32/RAMFS File Browser, Live System Monitor, Canvas Demo, and Settings.

---

## Building & Running

### Build the ISO & Storage Image
```bash
make clean
make
```

### Run in QEMU (Graphical Window)
To launch Greenhouse OS in a standard graphical desktop window (GTK interface):
```bash
make qemu
# or: make run
```

Alternatively, invoke QEMU directly:
```bash
qemu-system-x86_64 -boot d -cdrom greenhouse.iso -drive file=disk.img,format=raw,index=0,media=disk -m 256M -vga std -display gtk
```

#### Alternative Display Backends
- **SDL Window**:
  ```bash
  make run-sdl
  # or: qemu-system-x86_64 -boot d -cdrom greenhouse.iso -drive file=disk.img,format=raw,index=0,media=disk -m 256M -vga std -display sdl
  ```
- **Terminal Curses Mode**:
  ```bash
  make run-curses
  ```
- **Headless Serial Console**:
  ```bash
  make run-serial
  ```

---

## Using the Verdant Desktop

At the Greenhouse OS command prompt (`C:\>`), enter:
```text
verdant
```
*(or `gui` / `startgui`)*

### Keyboard Shortcuts & Navigation
| Key / Control | Function |
|---|---|
| `F1` / `[🌿 Apps]` | Open / Close the Radial Orbital Launcher |
| `1` – `6` (in launcher) | Launch orbital app (`1`: Terminal, `2`: Files, `3`: SysMon, `4`: Berry, `5`: Canvas, `6`: Settings) |
| `F2` / `[🍓 Berry]` | Open / Focus the Berry AI Assistant |
| `F3` | Open / Focus Graphical Terminal |
| `F4` | Open / Focus File Browser |
| `F5` | Auto-arrange all open surfaces across the spatial workspace |
| `Tab` | Cycle focus between open surfaces |
| `ESC` / `[Text X]` | Exit Verdant and return to the 720x400 VGA text console |

---

## Automated Verification Tests

Run the automated test suites in QEMU:
```bash
python3 test_verdant.py   # Test Verdant graphical shell (launcher, surfaces, berry, text exit)
make test                 # Full kernel, smoke, mouse, depth, and userland test suite
```

---

## Documentation
- [Verdant Graphical Shell Architecture & Guide](docs/verdant.md)
- [Graphics Subsystem & Input Architecture](docs/graphics-and-input.md)
