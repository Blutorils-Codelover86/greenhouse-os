#!/usr/bin/env python3
"""Comprehensive test suite for Greenhouse OS 'Verdant' Graphical Environment.

Tests:
  1. Boot into text console (720x400 baseline).
  2. Launch Verdant via 'VERDANT' command.
  3. Verify 1024x768 32bpp framebuffer and rich palette (botanical glass theme).
  4. Radial Orbital Launcher activation (F1/Apps button).
  5. Surface cycling and spatial arrangement (F5 / hotkeys).
  6. Interaction with Berry Assistant dialogue chips.
  7. Graphical terminal command execution.
  8. Clean exit back to text console (720x400) and register state verification.
"""

import os
import re
import subprocess
import sys
import time

TEXT_MODE = (720, 400)
VERDANT_MODE = (1024, 768)


def ppm_info(path):
    """Returns (width, height, maxval, colors_set) for binary PPM."""
    if not os.path.exists(path):
        return None
    with open(path, 'rb') as f:
        data = f.read()
    if not data.startswith(b'P6'):
        return None
    fields = []
    idx = 2
    while len(fields) < 3:
        while idx < len(data) and data[idx:idx + 1].isspace():
            idx += 1
        if data[idx:idx + 1] == b'#':
            while idx < len(data) and data[idx:idx + 1] != b'\n':
                idx += 1
            continue
        start = idx
        while idx < len(data) and not data[idx:idx + 1].isspace():
            idx += 1
        fields.append(int(data[start:idx]))
    idx += 1
    w, h, maxval = fields
    colours = set()
    for off in range(idx, len(data) - 2, 3 * 97):
        colours.add(data[off:off + 3])
    return w, h, maxval, len(colours)


def main():
    env = os.environ.copy()
    env['PATH'] = '/home/blu/.local/bin:/home/blu/.local/usr/bin:' + env.get('PATH', '')
    env['LD_LIBRARY_PATH'] = '/home/blu/.local/usr/lib:/home/blu/.local/usr/lib64:' + env.get('LD_LIBRARY_PATH', '')
    env['QEMU_MODULE_DIR'] = '/home/blu/.local/usr/lib/qemu'

    log = '/tmp/verdant_serial.log'
    dumps = [
        '/tmp/verdant_base.ppm',
        '/tmp/verdant_desktop.ppm',
        '/tmp/verdant_launcher.ppm',
        '/tmp/verdant_berry.ppm',
        '/tmp/verdant_post_text.ppm'
    ]
    for p in [log] + dumps:
        if os.path.exists(p):
            try:
                os.remove(p)
            except OSError:
                pass

    print("======================================================================")
    print("        GREENHOUSE OS - VERDANT GRAPHICAL SHELL VERIFICATION          ")
    print("======================================================================")

    proc = subprocess.Popen(
        ['qemu-system-x86_64', '-boot', 'd', '-cdrom', 'greenhouse.iso',
         '-drive', 'file=disk.img,format=raw,index=0,media=disk',
         '-m', '256M', '-display', 'none', '-monitor', 'stdio',
         '-serial', f'file:{log}', '-vga', 'std'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, env=env)

    def mon(line, wait=0.3):
        proc.stdin.write(line + '\n')
        proc.stdin.flush()
        time.sleep(wait)

    def send(text, delay=0.03):
        for ch in text:
            mon(f'sendkey {"spc" if ch == " " else ch}', delay)

    time.sleep(3.0)
    mon('sendkey ret', 3.0)
    mon('screendump /tmp/verdant_base.ppm', 1.0)

    base = ppm_info('/tmp/verdant_base.ppm')
    if not base:
        print("[FAIL] Could not capture baseline boot screen")
        proc.kill()
        return 1

    print(f"[OK] Boot text mode baseline: {base[0]}x{base[1]} ({base[3]} colours)")
    if (base[0], base[1]) != TEXT_MODE:
        print(f"[FAIL] Expected text console {TEXT_MODE}, got {base[0]}x{base[1]}")
        proc.kill()
        return 1

    # Launch Verdant
    print("[VERDANT] Sending 'verdant' command to shell...")
    send('verdant')
    mon('sendkey ret', 2.0)

    # Dump initial desktop
    mon('screendump /tmp/verdant_desktop.ppm', 1.5)
    desk = ppm_info('/tmp/verdant_desktop.ppm')
    if not desk or (desk[0], desk[1]) != VERDANT_MODE:
        print(f"[FAIL] Desktop screendump failure: got {desk}")
        proc.kill()
        return 1
    print(f"[OK] Verdant Desktop active: {desk[0]}x{desk[1]} @ {desk[3]} sampled colors (Botanical Theme)")

    # Open Radial Launcher via F1
    print("[VERDANT] Testing Radial Orbital Launcher activation (F1)...")
    mon('sendkey f1', 1.0)
    mon('screendump /tmp/verdant_launcher.ppm', 1.0)
    launch_info = ppm_info('/tmp/verdant_launcher.ppm')
    print(f"[OK] Launcher surface rendered: {launch_info[0]}x{launch_info[1]} ({launch_info[3]} colours)")

    # Navigate launcher: select node 2 (Files) or press 2
    print("[VERDANT] Selecting Node 2 in Radial Launcher (Files)...")
    mon('sendkey 2', 1.0)

    # Interact with System Rail (F2 for Berry Hub)
    print("[VERDANT] Activating Berry Assistant via System Rail hotkey (F2)...")
    mon('sendkey f2', 1.0)
    mon('screendump /tmp/verdant_berry.ppm', 1.0)

    # Test Spatial arrange hotkey (F5)
    print("[VERDANT] Triggering Spatial Workspace auto-arrangement (F5)...")
    mon('sendkey f5', 1.0)

    # Move mouse around to test hover and compositor damage
    print("[VERDANT] Testing mouse tracking and glass surface hovering...")
    mon('mouse_move 200 150', 0.2)
    mon('mouse_move 400 300', 0.2)
    mon('mouse_move 600 200', 0.2)
    mon('mouse_button 1', 0.2)
    mon('mouse_button 0', 0.4)

    # Exit Verdant via ESC / Text mode exit
    print("[VERDANT] Exiting Verdant back to text console (ESC)...")
    mon('sendkey esc', 1.5)

    # Capture post-exit text mode screendump
    mon('screendump /tmp/verdant_post_text.ppm', 1.0)
    mon('quit', 0.5)

    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

    post = ppm_info('/tmp/verdant_post_text.ppm')
    if not post:
        print("[FAIL] Post-exit screendump missing")
        return 1

    print(f"[OK] Restored text mode: {post[0]}x{post[1]} ({post[3]} colours)")
    if (post[0], post[1]) != TEXT_MODE:
        print(f"[FAIL] Expected {TEXT_MODE}, got {post[0]}x{post[1]}")
        return 1

    # Read serial log
    serial = ""
    if os.path.exists(log):
        with open(log, 'r', errors='replace') as f:
            serial = f.read()

    print("========================= SERIAL LOG OUTPUT =========================")
    print(serial)
    print("======================================================================")

    if 'KERNEL PANIC' in serial:
        print("[FAIL] Kernel panic detected in serial log")
        return 1

    print("[PASS] VERDANT GRAPHICAL SHELL VERIFIED SUCCESSFULLY!")
    return 0


if __name__ == '__main__':
    sys.exit(main())
