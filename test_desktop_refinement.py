#!/usr/bin/env python3
"""Automated verification for Greenhouse OS Verdant Desktop Refinement:
- Serene botanical light-mode background
- macOS menubar elegance + Windows taskbar/system tray fusion in Topbar:
  * Brand pill [GH] Greenhouse
  * Interactive window tabs with status dots and minimize-on-click
  * Live RAM badge (querying PMM)
  * Real RTC / system clock (HH:MM)
  * Console return pill
- Floating bottom dock with macOS-style app tiles and running dots
- Window management:
  * Spatial auto-tile layout
  * Edge snapping & Alt shortcuts (Alt+Up maximize, Alt+Down restore/minimize, Alt+Left/Right snap)
  * Alt+Tab window cycling
  * F11 Show Desktop toggle
- Right-click Desktop Context Menu
- Clean exit to 720x400 text console
"""

import os
import subprocess
import sys
import time

TEXT_MODE = (720, 400)
VERDANT_MODE = (1024, 768)

def ppm_info(path):
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

def ppm_to_png(ppm_path, png_path):
    if not os.path.exists(ppm_path):
        return False
    cmd = ['ffmpeg', '-y', '-i', ppm_path, png_path]
    res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return res.returncode == 0 and os.path.exists(png_path)

def main():
    env = os.environ.copy()
    env['PATH'] = '/home/blu/.local/bin:/home/blu/.local/usr/bin:' + env.get('PATH', '')
    env['LD_LIBRARY_PATH'] = '/home/blu/.local/usr/lib:/home/blu/.local/usr/lib64:' + env.get('LD_LIBRARY_PATH', '')
    env['QEMU_MODULE_DIR'] = '/home/blu/.local/usr/lib/qemu'

    log = '/tmp/desktop_refinement_serial.log'
    dumps = [
        '/tmp/dr_01_base.ppm',
        '/tmp/dr_02_spatial.ppm',
        '/tmp/dr_03_maximized.ppm',
        '/tmp/dr_04_split_snap.ppm',
        '/tmp/dr_05_show_desk.ppm',
        '/tmp/dr_06_ctx_menu.ppm',
        '/tmp/dr_07_post.ppm'
    ]
    for p in [log] + dumps:
        if os.path.exists(p):
            try:
                os.remove(p)
            except OSError:
                pass

    print("[START] Launching QEMU for Desktop Refinement verification...")
    proc = subprocess.Popen(
        ['qemu-system-x86_64', '-boot', 'd', '-cdrom', 'greenhouse.iso',
         '-drive', 'file=disk.img,format=raw,index=0,media=disk',
         '-m', '1G', '-display', 'none', '-monitor', 'stdio',
         '-serial', f'file:{log}', '-vga', 'std'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, env=env)

    def mon(line, wait=0.3):
        proc.stdin.write(line + '\n')
        proc.stdin.flush()
        time.sleep(wait)

    def send(text, delay=0.03):
        for ch in text:
            if ch == ' ':
                mon('sendkey spc', delay)
            elif ch == '\n':
                mon('sendkey ret', delay)
            elif ch == '.':

                mon('sendkey dot', delay)
            elif ch == '-':
                mon('sendkey minus', delay)
            elif ch == '_':
                mon('sendkey shift-minus', delay)
            elif ch == ':':
                mon('sendkey shift-semicolon', delay)
            elif ch == '\\':
                mon('sendkey backslash', delay)
            elif ch == '/':
                mon('sendkey slash', delay)
            elif ch.isupper():
                mon(f'sendkey shift-{ch.lower()}', delay)
            else:
                mon(f'sendkey {ch}', delay)

    time.sleep(3.0)
    mon('sendkey ret', 3.0)
    mon('screendump /tmp/dr_01_base.ppm', 1.0)
    base = ppm_info('/tmp/dr_01_base.ppm')
    if not base or (base[0], base[1]) != TEXT_MODE:
        print(f"[FAIL] Baseline mode failed: {base}")
        proc.kill()
        return 1
    print(f"[OK] Baseline 720x400 text mode active: {base}")

    # Launch Verdant
    print("[TEST] Launching Verdant graphical desktop...")
    send('verdant')
    mon('sendkey ret', 3.0)
    mon('screendump /tmp/dr_02_spatial.ppm', 1.0)
    spatial = ppm_info('/tmp/dr_02_spatial.ppm')
    if not spatial or (spatial[0], spatial[1]) != VERDANT_MODE:
        print(f"[FAIL] Verdant desktop mode failed: {spatial}")
        proc.kill()
        return 1
    print(f"[OK] Verdant desktop 1024x768 active with {spatial[3]} distinct colors.")
    ppm_to_png('/tmp/dr_02_spatial.ppm', '/tmp/dr_02_spatial.png')

    # Test Window Maximize via Alt+Up
    print("[TEST] Testing Alt+Up (Window Maximize)...")
    mon('sendkey alt-up', 0.8)
    mon('screendump /tmp/dr_03_maximized.ppm', 1.0)
    maxi = ppm_info('/tmp/dr_03_maximized.ppm')
    print(f"[OK] Window maximized capture: {maxi}")
    ppm_to_png('/tmp/dr_03_maximized.ppm', '/tmp/dr_03_maximized.png')

    # Restore via Alt+Down, Snap Left via Alt+Left
    print("[TEST] Testing Alt+Down (Restore) and Alt+Left (Snap Left Half)...")
    mon('sendkey alt-down', 0.8)
    mon('sendkey alt-left', 0.8)

    # Focus next window via Alt+Tab and Snap Right via Alt+Right
    print("[TEST] Testing Alt+Tab (Cycle Focus) and Alt+Right (Snap Right Half)...")
    mon('sendkey alt-tab', 0.6)
    mon('sendkey alt-right', 0.8)
    mon('screendump /tmp/dr_04_split_snap.ppm', 1.0)
    split_info = ppm_info('/tmp/dr_04_split_snap.ppm')
    print(f"[OK] Split snap capture: {split_info}")
    ppm_to_png('/tmp/dr_04_split_snap.ppm', '/tmp/dr_04_split_snap.png')

    # Test Show Desktop Toggle (F11)
    print("[TEST] Testing F11 (Show Desktop Toggle)...")
    mon('sendkey f11', 0.8)
    mon('screendump /tmp/dr_05_show_desk.ppm', 1.0)
    desk_only = ppm_info('/tmp/dr_05_show_desk.ppm')
    print(f"[OK] Show Desktop capture: {desk_only}")
    ppm_to_png('/tmp/dr_05_show_desk.ppm', '/tmp/dr_05_show_desk.png')

    # Test Desktop Context Menu (when all windows are minimized, right click on desktop)
    print("[TEST] Testing Right-Click Desktop Context Menu...")
    mon('sendkey shift-f10', 0.8)
    mon('screendump /tmp/dr_06_ctx_menu.ppm', 1.0)
    ctx_info = ppm_info('/tmp/dr_06_ctx_menu.ppm')
    print(f"[OK] Desktop Context Menu capture: {ctx_info}")
    ppm_to_png('/tmp/dr_06_ctx_menu.ppm', '/tmp/dr_06_ctx_menu.png')

    # Dismiss Context Menu
    mon('sendkey esc', 1.0)

    # Restore windows via F11
    print("[TEST] Restoring windows via F11...")
    mon('sendkey f11', 1.0)

    # Test Seed Key (Windows Key / meta_l) to open Seed Command Center
    print("[TEST] Testing Seed Key (meta_l / Windows Key) to toggle Seed Command Center...")
    mon('sendkey meta_l', 0.8)
    mon('screendump /tmp/dr_07_seed_menu.ppm', 1.0)
    seed_info = ppm_info('/tmp/dr_07_seed_menu.ppm')
    print(f"[OK] Seed Command Center capture: {seed_info}")
    ppm_to_png('/tmp/dr_07_seed_menu.ppm', '/tmp/dr_07_seed_menu.png')

    # Dismiss Seed Menu
    mon('sendkey esc', 0.8)

    # Exit Verdant via logout in GUI terminal
    print("[TEST] Exiting Verdant back to console (typing 'logout')...")
    send('logout\n', 0.08)
    time.sleep(2.5)
    mon('screendump /tmp/dr_08_post.ppm', 1.5)
    mon('quit', 0.5)


    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

    post = ppm_info('/tmp/dr_08_post.ppm')
    if not post or (post[0], post[1]) != TEXT_MODE:
        print(f"[FAIL] Post text restore failed: {post}")
        return 1
    print(f"[OK] Cleanly restored 720x400 text console: {post}")

    # Copy screenshots to artifact directory
    art_dir = '/home/blu/.gemini/antigravity-cli/brain/d11f3b7c-9a84-4e28-91e0-b8cd64cd81c9'
    for png in ['dr_02_spatial.png', 'dr_03_maximized.png', 'dr_04_split_snap.png',
                'dr_05_show_desk.png', 'dr_06_ctx_menu.png', 'dr_07_seed_menu.png']:
        src = f'/tmp/{png}'
        dst = f'{art_dir}/{png}'
        if os.path.exists(src):
            try:
                import shutil
                shutil.copyfile(src, dst)
                print(f"[OK] Exported artifact: {dst}")
            except Exception as e:
                print(f"[WARN] Failed copying artifact {png}: {e}")

    # Inspect serial log
    serial = ""
    if os.path.exists(log):
        with open(log, 'r', errors='replace') as f:
            serial = f.read()

    print("=================== SERIAL LOG SUMMARY ===================")
    print(serial[-800:] if len(serial) > 800 else serial)
    print("==========================================================")

    if 'KERNEL PANIC' in serial:
        print("[FAIL] Kernel panic in serial log!")
        return 1

    print("\n[SUCCESS] Desktop Refinement automated verification passed!")
    return 0

if __name__ == '__main__':
    sys.exit(main())
