# Greenhouse OS — VERDANT Graphical Shell

Verdant is Greenhouse OS's native graphical environment and modern window compositor. It is engineered around the core principles of **spatial fluidity, botanical minimalism, liquid-glass aesthetic separation, and deep system awareness**.

Greenhouse OS avoids legacy retro/cyberpunk tropes (no CRT scanlines, neon purple/cyan glowing borders, or ASCII windows) and instead delivers a modern, calm, spacious, and translucent operating environment.

---

## 1. System Architecture

Verdant enforces a clean architectural separation between privileged kernel hardware operations, surface composition, and shell interaction:

```
                  ┌─────────────────────────────────────┐
                  │          Greenhouse Kernel          │
                  └──────────────────┬──────────────────┘
                 ┌───────────────────┼───────────────────┐
                 │                   │                   │
         ┌───────┴──────┐    ┌───────┴──────┐    ┌───────┴──────┐
         │  Processes   │    │ Framebuffer  │    │ Input System │
         │ (Ring 0 / 3) │    │ (VBE/Bochs)  │    │  (Kbd/Mouse) │
         └───────┬──────┘    └───────┬──────┘    └───────┬──────┘
                 └───────────────────┼───────────────────┘
                                     │
                             IPC / Syscalls
                                     │
                    ┌────────────────┴────────────────┐
                    │      Verdant Surface System     │
                    │   (Surfaces, Z-Order, Damage)   │
                    ├─────────────────────────────────┤
                    │    Verdant Compositor & GFX     │
                    │ (Liquid Glass, Alpha, Shadows)  │
                    └────────────────┬────────────────┘
                                     │
                    ┌────────────────┴────────────────┐
                    │          VERDANT SHELL          │
                    └─┬──────────────┼──────────────┬─┘
                      │              │              │
           ┌──────────┴──┐    ┌──────┴──────┐    ┌──┴───────────┐
           │   Bottom    │    │  Berry Hub  │    │ Floating     │
           │  App Dock   │    │  Assistant  │    │ Top Status   │
           └─────────────┘    └─────────────┘    └──────────────┘
                      │              │              │
           ┌──────────┴──┐    ┌──────┴──────┐    ┌──┴───────────┐
           │ Graphical   │    │    File     │    │   System     │
           │  Terminal   │    │   Browser   │    │   Monitor    │
           └─────────────┘    └─────────────┘    └──────────────┘
```

### Subsystem Breakdown
- **Hardware & Memory Layer**: VBE Linear Framebuffer (`1024x768x32bpp`), Bochs DISPI, Double-buffered present pipeline (`graphics_present`), PMM/VMM dynamic allocation.
- **Modern Renderer (`kernel/gui/renderer.c`)**: Fast integer alpha compositing (`renderer_alpha_blend`), rounded translucent rectangles (`renderer_fill_alpha_rounded_rect`), soft ambient drop shadows (`renderer_draw_drop_shadow`), specular highlights, pill badges, and telemetry meters.
- **Compositor (`kernel/gui/compositor.c`)**: Atmospheric gradient wallpaper, surface z-ordering, clipping, window frame decoration with rounded header and control buttons (`[-]`, `[+]`, `[X]`), client area calculation, and hit testing.
- **Desktop Shell (`kernel/gui/shell.c`)**:
  - **Top Floating Status Bar**: Live Physical Memory (PMM) gauge, uptime clock, CPU tick counter, surface switcher tabs, and Console exit button.
  - **Bottom Floating Dock**: Translucent liquid-glass dock containing 6 quick-launch app pills (Terminal, Files, Monitor, Berry, Canvas, Settings) with hover highlights and active dot indicators.
- **Native Applications**:
  - **Berry Assistant (`kernel/gui/berry_surface.c`)**: System AI companion with interactive action chips and real-time kernel query feed.
  - **Graphical Terminal (`kernel/gui/guiterm.c`)**: Interactive terminal console with integrated command processor.
  - **File Browser (`kernel/gui/filebrowser.c`)**: Visual drive and directory navigator supporting `C:` (FAT32) and `R:` (RAMFS) with live file preview.
  - **System Monitor (`kernel/gui/sysmon.c`)**: Live memory progress meters, process table, and storage usage telemetry.
  - **Canvas Demo (`kernel/gui/canvas_surface.c`)**: Real-time rendering demo with smooth geometry and graphics tests.
  - **Settings & Shortcuts (`kernel/gui/settings_surface.c`)**: Display mode inspector and global keybinding guide.

---

## 2. Visual Design & Liquid-Glass Theme

Verdant employs a modern, calm botanical palette:
- **Atmospheric Wallpaper**: Deep slate to emerald-slate smooth vertical gradient (`#0A131C` to `#112226`) with subtle ambient coordinate nexus dots (`#10B981` at 18% intensity).
- **Liquid Glass Panels**: Translucent dark surfaces (`#0C161D` / `#101D24` at ~85-90% opacity) accented with a 1px top specular highlight line (`#38BDF8` at 30% alpha) and soft ambient drop shadows.
- **Color Accents**:
  - **Emerald Primary**: `#10B981` / `#34D399` (Focus highlights, brand accents, RAM gauge)
  - **Berry Secondary**: `#F43F5E` / `#FB7185` (Berry AI hub, alerts, close button)
  - **Sky Blue**: `#38BDF8` (Terminal, active surface indicators, telemetry badges)
  - **Amber**: `#F59E0B` (Canvas, warnings)
  - **Indigo**: `#818CF8` (System Monitor)
  - **High-Contrast Text**: `#F1F5F9` (Headings/active text) and `#94A3B8` (Muted/secondary text)

---

## 3. Spatial Workspace & Window Mechanics

- **Spatial Freeform Layout**: Surfaces exist as free-floating objects that can be dragged by their titlebar, resized via corner handles, focused by clicking, or auto-arranged in a balanced spatial constellation (`F5`).
- **Window Controls**:
  - `[-]` **Minimize**: Hides the surface content into the top status bar tab.
  - `[+]` **Maximize / Expand**: Expands the surface bounds across the workspace.
  - `[X]` **Close**: Destroys the surface and releases allocated GUI buffers.
- **Crash Safety & Bounds Checking**: Every pixel write in the renderer, compositor, and shell is guarded by strict framebuffer boundary checks (`0 <= x < width`, `0 <= y < height`) preventing kernel panics or out-of-bounds corruption.

---

## 4. Shell Navigation & Dock

### Bottom Floating Dock
The bottom dock floats at `Y: 712` (dimensions: `436x44` px):
1. **Terminal** (`F3` / Scancode `1`)
2. **Files** (`F4` / Scancode `2`)
3. **Monitor** (Scancode `3`)
4. **Berry** (`F2` / Scancode `4`)
5. **Canvas** (Scancode `5`)
6. **Settings** (Scancode `6`)

Clicking any dock icon launches or toggles the application surface. Active open surfaces display a green status dot beneath their dock icon.

---

## 5. Input Flow & Keybindings

Input events flow cleanly from hardware drivers through the kernel input queue to the Verdant Window Manager:

```
[PS/2 Hardware] ──> [kbd.c / mouse.c] ──> [input.c Queue] ──> [verdant_handle_input()]
                                                                      │
                                                ┌─────────────────────┴─────────────────────┐
                                                │                                           │
                                       [System Hotkeys / Shell]                    [Focused Surface]
```

### Global Keyboard Shortcuts
| Keybinding | Action |
|---|---|
| `F1` / `Space` (Desktop) | Open / Focus Radial Orbital Launcher |
| `F2` | Open / Focus Berry Assistant |
| `F3` | Open / Focus Graphical Terminal |
| `F4` | Open / Focus File Browser |
| `F5` | Auto-arrange Workspace Surfaces |
| `1` - `6` (In Launcher/Dock) | Launch corresponding application |
| `Tab` | Cycle focus to next surface |
| `ESC` | Exit Verdant to 720x400 VGA Text Console |

---

## 6. Building, Booting & Testing

### Build
```bash
make clean
make
```

### Launch in QEMU
```bash
make qemu
# or: make run
```
At the `C:\>` prompt, enter:
```text
verdant
```
*(or `gui` / `startgui`)*

### Automated Verification Suites
- **Full Verdant Verification**:
  ```bash
  python3 test_verdant.py
  ```
- **Standard Graphics & Kernel Tests**:
  ```bash
  python3 test_gfx.py gui
  make test
  ```
