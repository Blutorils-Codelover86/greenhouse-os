#!/usr/bin/env python3
"""End-to-end check of the userland graphics demo: RUN GFX.ELF from the shell,
verify it takes the screen over, animates, exits on ESC and gives the text
console back."""
import os
import re
import subprocess
import sys
import time

# 80x25 cells of 9x16 pixels: the text console the OS boots into.
TEXT_MODE = (720, 400)

env = os.environ.copy()
env['PATH'] = '/home/blu/.local/bin:/home/blu/.local/usr/bin:' + env.get('PATH', '')
env['LD_LIBRARY_PATH'] = '/home/blu/.local/lib:/home/blu/.local/lib64:' + env.get('LD_LIBRARY_PATH', '')

log = '/tmp/gfxu_serial.log'
paths = ('/tmp/gfxu_boot.ppm', '/tmp/gfxu_mode1.ppm', '/tmp/gfxu_mode2.ppm',
         '/tmp/gfxu_mode3.ppm', '/tmp/gfxu_text.ppm')
for p in (log,) + paths:
    if os.path.exists(p):
        os.remove(p)

proc = subprocess.Popen(
    ['qemu-system-x86_64', '-boot', 'd', '-cdrom', 'greenhouse.iso', '-hda', 'disk.img',
     '-m', '1G', '-display', 'none', '-monitor', 'stdio',
     '-serial', f'file:{log}', '-vga', 'std'],
    stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    text=True, env=env)


def mon(line, wait=0.4):
    proc.stdin.write(line + '\n')
    proc.stdin.flush()
    time.sleep(wait)


KEYS = {' ': 'spc', '.': 'dot', ',': 'comma', '-': 'minus', '\\': 'backslash'}


def send(text, delay=0.05):
    for ch in text:
        mon(f'sendkey {KEYS.get(ch, ch)}', delay)


time.sleep(3.0)
mon('sendkey ret', 4.0)
mon('screendump "/tmp/gfxu_boot.ppm"', 1.5)
send('run gfx.elf')
mon('sendkey ret', 5.0)
# Every capture needs its own path: rewriting one file inside a single QEMU
# session can leave a stale image behind.
mon('screendump "/tmp/gfxu_mode1.ppm"', 2.0)
mon('mouse_move 300 200', 1.0)
mon('mouse_move 640 420', 1.0)
mon('screendump "/tmp/gfxu_mode2.ppm"', 1.5)
mon('screendump "/tmp/gfxu_mode3.ppm"', 1.5)
mon('sendkey esc', 4.0)
mon('screendump "/tmp/gfxu_text.ppm"', 1.5)
mon('quit', 1.0)
try:
    proc.wait(timeout=5)
except subprocess.TimeoutExpired:
    proc.kill()


def load_ppm(path):
    if not os.path.exists(path):
        return None
    data = open(path, 'rb').read()
    if not data.startswith(b'P6'):
        return None
    fields, idx = [], 2
    while len(fields) < 3:
        while data[idx:idx + 1].isspace():
            idx += 1
        start = idx
        while not data[idx:idx + 1].isspace():
            idx += 1
        fields.append(int(data[start:idx]))
    idx += 1
    return fields[0], fields[1], data, idx


def ppm_info(path):
    img = load_ppm(path)
    if img is None:
        return None
    w, h, data, idx = img
    colours = {data[o:o + 3] for o in range(idx, len(data) - 2, 3 * 97)}
    return w, h, len(colours)


def count_colour(img, colour, box=None):
    """Counts exact pixels of one colour, optionally inside a box."""
    w, h, data, idx = img
    x0, y0, x1, y1 = box or (0, 0, w, h)
    total = 0
    for y in range(y0, min(y1, h)):
        base = idx + y * w * 3
        for x in range(x0, min(x1, w)):
            off = base + x * 3
            if data[off:off + 3] == colour:
                total += 1
    return total


serial = open(log, errors='replace').read()

failures = 0

# Colours the demo asks for through the graphics syscalls.
PANEL_BG = bytes((0x18, 0x1C, 0x28))
PANEL_EDGE = bytes((0x39, 0x44, 0x5C))
ACCENT = bytes((0x5A, 0xC8, 0xFA))
TEXT_FG = bytes((0xE6, 0xEA, 0xF2))

boot = ppm_info('/tmp/gfxu_boot.ppm')
if boot is None:
    print('[FAIL] no boot text screendump')
    failures += 1
elif (boot[0], boot[1]) != TEXT_MODE:
    print(f'[FAIL] the OS did not boot into the {TEXT_MODE[0]}x{TEXT_MODE[1]} text console,'
          f' got {boot[0]}x{boot[1]}')
    failures += 1
text = ppm_info('/tmp/gfxu_text.ppm')
modes = [(n, load_ppm(f'/tmp/gfxu_mode{n}.ppm')) for n in (1, 2, 3)]
modes = [(n, img) for n, img in modes if img is not None]

if not modes:
    print('[FAIL] no screendump of the userland graphics mode')
    failures += 1
for n, img in modes:
    w, h = img[0], img[1]
    print(f'[INFO] userland graphics dump {n}: {w}x{h}')
    if (w, h) != (1024, 768):
        print(f'[FAIL] dump {n}: expected a 1024x768 userland graphics mode, got {w}x{h}')
        failures += 1

if modes:
    # The demo's own drawing has to show up: the background it cleared to, the
    # panel, the accent bar and the two text colours.
    seen = set()
    for _, img in modes:
        for c in (PANEL_BG, PANEL_EDGE, ACCENT, TEXT_FG):
            if count_colour(img, c):
                seen.add(c)
    for c, name in ((PANEL_BG, 'gfx_clear background'), (PANEL_EDGE, 'gfx_rect panel'),
                    (ACCENT, 'accent bar / crosshair'), (TEXT_FG, 'gfx_text glyphs')):
        if c in seen:
            print(f'[PASS] {name} present in the userland frame')
        else:
            print(f'[FAIL] {name} never reached the framebuffer')
            failures += 1

    # The pointer crosshair must sit where the program last saw the mouse: the
    # monitor drives the emulated PS/2 mouse, the kernel turns packets into
    # events, userland tracks them and draws the crosshair at that spot.
    pointer = re.findall(r'final pointer (-?\d+),(-?\d+)', serial)
    if not pointer:
        print('[FAIL] the demo never reported the pointer position it tracked')
        failures += 1
    else:
        px, py = (int(v) for v in pointer[-1])
        print(f'[INFO] userland pointer ended at {px},{py}')
        if not (0 <= px < 1024 and 0 <= py < 768):
            print(f'[FAIL] pointer {px},{py} is outside the 1024x768 surface')
            failures += 1
        window = (max(px - 24, 0), max(py - 24, 0), min(px + 24, 1024), min(py + 24, 768))
        crosshair = max(count_colour(img, ACCENT, window) for _, img in modes)
        if crosshair >= 8:
            print(f'[PASS] crosshair drawn at the tracked pointer position '
                  f'({crosshair} accent pixels around {px},{py})')
        else:
            print(f'[FAIL] no crosshair around the tracked pointer {px},{py} '
                  f'({crosshair} accent pixels)')
            failures += 1

if text is None:
    print('[FAIL] no screendump for the text console after the demo exited')
    failures += 1
else:
    w, h, colours = text
    print(f'[INFO] text after exit: {w}x{h}, {colours} sampled colours')
    if (w, h) != TEXT_MODE:
        print(f'[FAIL] expected the {TEXT_MODE[0]}x{TEXT_MODE[1]} text console, got {w}x{h}')
        failures += 1
    if boot is None:
        print('[FAIL] no boot text screendump to compare against')
        failures += 1
    elif (w, h) != (boot[0], boot[1]):
        print(f'[FAIL] expected the boot text mode {boot[0]}x{boot[1]} after the demo, got {w}x{h}')
        failures += 1
    if colours < 2:
        print(f'[FAIL] restored text console looks blank ({colours} colours)')
        failures += 1

if 'GFX: userland demo finished' not in serial:
    print('[FAIL] demo never reported completion')
    failures += 1
if 'KERNEL PANIC' in serial:
    print('[FAIL] kernel panic')
    failures += 1
if 'GFX: surface 1024x768 @ 32 bpp' not in serial:
    print('[FAIL] the demo never reported the 1024x768x32 surface from gfx_get_info')
    failures += 1

counts = re.findall(r'demo finished after \d+ frame\(s\), (\d+) event\(s\)', serial)
if not counts:
    print('[FAIL] could not read the event count from the demo')
    failures += 1
elif int(counts[-1]) < 1:
    print(f'[FAIL] the demo saw no input events ({counts[-1]}), gfx_poll_event is not delivering')
    failures += 1
else:
    print(f'[INFO] userland input: demo handled {counts[-1]} event(s)')

print('=' * 70)
print(serial[-2500:])
print('=' * 70)
print('GFX USERLAND TEST: ' + ('PASS' if failures == 0 else f'FAIL ({failures})'))
sys.exit(1 if failures else 0)
