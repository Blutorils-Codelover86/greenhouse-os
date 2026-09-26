# Graphics, Input and the Userland Display API

This document covers the display stack (`kernel/graphics/`), the unified input
stack (`kernel/input/`), the ring 3 graphics/input syscalls, and the shell
commands and tests that exercise them.

## 1. Design rules

* **Graphics is opt-in.** The kernel boots into the firmware text console and
  stays there until something asks for a graphics mode. `gfx`, `gui` and the
  `SYS_GFX_ENTER` syscall are the only ways in.
* **The text console comes back exactly as it was found.** Entering a graphics
  mode snapshots the whole VGA register file, and leaving restores it and then
  *verifies* it register by register. A graphics command that leaves a mangled
  console behind reports failure instead of quietly continuing.
* **User space never touches the hardware.** Ring 3 has no framebuffer mapping
  and no I/O ports. Every pixel and every event travels through `int 0x80`.

## 2. Display stack

```
vbe.c        VBE/Bochs probe, mode programming, text state save/restore
  -> framebuffer.c  mapping, pixel format classification, text verification
  -> graphics.c   back buffer, clipping, primitives, present
  -> font.c       8x16 glyphs, draw_text()
  -> gfx_test.c   self test (gfx)
  -> gui/         desktop: window manager, widgets, cursor, graphical terminal
```

The Multiboot2 header deliberately carries **no framebuffer tag**. Requesting one
makes the loader program a graphics mode before the kernel runs, which hides the
VGA text console this OS boots into, and the VBE backend re-reads the adapter
registers anyway instead of trusting loader-supplied geometry. The tests assert
the 80x25 text geometry directly, so a loader handing over a graphics mode fails
loudly rather than matching its own baseline.

### 2.1 Mode handling

`framebuffer_enter_mode()` calls `vbe_save_text_state()` *before* anything is
reprogrammed, then programs the mode through the VBE registers and maps the
linear framebuffer through the existing VMM (the loader only identity maps the
first GiB, and video adapters place their framebuffer in the PCI hole far above
that). Whatever geometry the hardware reports back wins over the geometry the
loader or the caller asked for, so the stride the driver uses is the stride the
adapter really programmed.

Pixel depth is a parameter, not a constant: `framebuffer_enter_mode_ex()` takes
a width, height and bits-per-pixel, and 0 in any field means "keep the driver
default" (1024x768x32). The channel positions move with the depth, so the driver
describes the format it was actually given -- 8/8/8 at bit 16/8/0 for 24 and 32
bpp, and 5/6/5 at bit 11/5/0 for 16 bpp -- and `fb_classify_format()` then
picks RGB565 or RGB888 rather than assuming everything is 32 bpp. Every pixel
write and read switches on that classification, so one code path serves the
whole set. The self test's exact-match pixel check becomes a per-channel
tolerance at shallower depths (a step of 8 in 8-bit terms at 16 bpp), because
5-6-5 quantisation legitimately moves a channel; at 24 and 32 bpp the check
stays exact.

`framebuffer_leave_mode()` restores the snapshot and then calls
`framebuffer_verify_text_state_ex()`, which diffs the live VGA registers against
the snapshot and reports how many registers differ. `cmd_gfx()` only prints
`PASS` when that count is zero, so a restore that silently fails cannot be
mistaken for success.

### 2.2 The QEMU attribute controller trap

The VGA attribute controller at `0x3C0`/`0x3C1` is index/data multiplexed and a
flip-flop decides which one the next access means. Two details are easy to get
wrong and both corrupt the palette in ways that look like a graphics bug:

* Only reading the input status port `0x3DA` is *guaranteed* to reset the
  flip-flop. QEMU does **not** reset it on a read from `0x3C1`, so index writes
  alternate between index and data. `vbe_save_text_state()` and
  `vbe_restore_text_state()` therefore poll `0x3DA` before every attribute index
  write.
* Data writes have to go to `0x3C0`; QEMU ignores writes to `0x3C1`.

With the flip-flop clear, reading `0x3C0` returns the index latch itself, whose
bit 5 is what keeps the display live. The latch value is saved in
`vga_text_state_t.attr_index` and put back at the end of the save.

QEMU also locks CRTC registers 0-7 while CR11 bit 7 is set, so mode exit clears
that lock before restoring the CRTC.

### 2.3 Back buffer and present

Drawing goes into a back buffer (`GFX_BACKBUF_VIRT_BASE`, 0x400000 in the low
4 GiB) and `graphics_present()` copies it to the framebuffer row by row. The
copy is a guest loop, so a QEMU `screendump` can land in the middle of it and
capture a half-updated screen. Tests that need a stable image either capture
several dumps and accept the best one, or check content that survives tearing.

## 3. Input stack

```
kbd.c   -> scancode + ASCII, modifiers, extended keys
mouse.c -> 3-byte PS/2 packets, buttons, wheel (4th byte)
input.c -> one 256-slot queue, pointer state, statistics
```

`input_poll_event()` is the single consumer interface; the kernel shell, the GUI
and `SYS_INPUT_POLL` all read from it. Statistics are available through the
`input` command (packets, overruns, posted/delivered/dropped events, pointer,
button state).

Two PS/2 details the driver has to get right, both found by driving the emulated
mouse from the QEMU monitor:

* **The Y axis points up.** A raw Y byte of `0xE2` means "30 pixels up", so the
  driver negates it. Without the negation every downward move arrives as a
  negative delta, the pointer clamps to the top of the screen and stays there.
* **The pointer position is owned by the posting side.** One monitor
  `mouse_move` expands into a burst of packets, and if the absolute position is
  only recomputed when an event is polled, every event in the burst reports the
  same stale base and the pointer never reaches the target. `input_post_mouse_move()`
  and `input_post_mouse_button()` advance `pointer_x`/`pointer_y` as soon as a
  packet arrives.

Pointer bounds follow the display mode: `framebuffer_enter_mode()` and
`framebuffer_leave_mode()` call `input_set_pointer_bounds()` with the mode's
geometry (the text geometry is derived from the saved CRTC registers), so a
userland program can address the whole surface without knowing the mode size.

## 4. Scheduler and timer

`process_yield()` raises **vector 0xFE**, a software scheduling request, and
`interrupt_dispatch()` reschedules on it without touching the PIT. This matters:
routing yields through IRQ 0 made the timer handler increment `kernel_ticks` on
every yield, so a `sleep()` was satisfied by fabricated ticks instead of elapsed
time - `sleep(300)` returned immediately and `ticks`/`uptime` reported a
multiple of real time.

`process_sleep()` marks the process `PROCESS_SLEEPING` with an absolute
`sleep_until_tick` and yields; `process_wake_sleepers()` makes it `READY` again
on a real tick. The loop after the yield re-checks the deadline, because the
scheduler can hand control straight back when nothing else is runnable; in that
case the process idles on `hlt` until the next tick instead of spinning.

## 5. Userland API

Syscalls `18`-`26` (`user/libc/syscall.h`, dispatched in `kernel/syscall.c`):

| Number | Name | Purpose |
| --- | --- | --- |
| 18 | `SYS_GFX_ENTER` | snapshot text state, program a graphics mode |
| 19 | `SYS_GFX_LEAVE` | restore the text console |
| 20 | `SYS_GFX_INFO` | width, height, pitch, bpp, channel layout |
| 21 | `SYS_GFX_CLEAR` | fill the back buffer |
| 22 | `SYS_GFX_RECT` | filled, clipped rectangle |
| 23 | `SYS_GFX_LINE` | clipped line |
| 24 | `SYS_GFX_TEXT` | 8x16 text with foreground and background |
| 25 | `SYS_GFX_PRESENT` | copy the back buffer to the framebuffer |
| 26 | `SYS_INPUT_POLL` | dequeue one input event |

Rules the dispatcher enforces:

* Drawing calls require a live surface; they return `-1` instead of scribbling
  into a memory map that is not there.
* User pointers are range checked with `s_validate_user_ptr()`. `SYS_GFX_TEXT`
  additionally walks the string with a 256 byte bound, validating each byte, so
  an unterminated string cannot walk the whole address space.
* `SYS_GFX_INFO` reports `back_buffer = 0`: the surface itself is never mapped
  into ring 3.

The userland wrappers live in `user/libc/gui.c`, and `user/programs/gfx.c` is a
worked example: it enters graphics mode, animates a panel, follows the pointer
and returns the text console on ESC.

## 6. Shell commands

| Command | What it does |
| --- | --- |
| `gfx [width [height [bpp]] [hold]]` | programs the requested mode (0 or a missing field keeps the default), runs the graphics self test, optionally holds the finished frame on screen for `hold` seconds, then restores the text console and reports the register diff |
| `gui` | programs the mode and starts the desktop |
| `gfxinfo` | adapter, BAR0, mode source, pixel format, backend |
| `input` | keyboard/mouse/queue statistics and pointer state |

## 7. Tests

| Test | Covers |
| --- | --- |
| `test_kernel.py` | PMM, VMM, heap, syscalls, scheduler, filesystem |
| `test_smoke.py` | boot, `gfxinfo`, `input`, basic shell operation |
| `test_mouse.py` | PS/2 packets, buttons, wheel, pointer tracking |
| `test_gfx.py gfx` | self test plus text restore, verified against the 720x400 boot text mode |
| `test_gfx.py gui` | desktop rendering and text restore |
| `test_gfx.py gfx 640 480 16 10` | the shallow 5-6-5 path: pixel checks, present checks and a screendump of a held 640x480 surface |
| `test_gfx.py gfx 800 600 24 10` | the same for 24 bpp packed RGB |
| `test_gfx_userland.py` | `GFX.ELF` end to end: mode, drawing, text, input, restore |

`test_gfx_userland.py` is the strongest end-to-end check: it launches the demo
from the shell, captures the surface, verifies the exact colours the demo asked
for through the syscalls (clear background, panel, accent bar, text glyphs),
confirms the pointer crosshair is drawn where the program last saw the mouse,
sends ESC, and checks the text console matches the boot resolution.

Build and run everything with:

```sh
make                       # kernel, userland, ISO (and disk.img on demand)
make test                  # the whole suite below
make test-gfx-depths       # just the 16 and 24 bpp runs
make test-gfx-userland     # just the ring 3 end-to-end check
```

## 8. Known limitations

* 16 bpp is exercised as RGB565 and 24 bpp as packed RGB on the emulated adapter
  only. 5-5-5 and the indexed formats (`FB_PIXEL_INDEXED8`/`INDEXED4`, which
  resolve colours through the VGA DAC palette) are implemented but still
  untested, and the userland API has no way to request a mode: `gfx_enter()`
  always takes the driver default.
* PS/2 wheel events are decoded, but QEMU has not been observed to emit them
  (`input` reports `wheel 0`), so the path is untested end to end.
* `graphics_present()` copies row by row, so a screendump taken during a present
  can capture a partially updated screen.
* The GUI is a single full-screen desktop; there is no window movement, z-order
  or focus management yet.
