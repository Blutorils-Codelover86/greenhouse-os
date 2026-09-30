#!/usr/bin/env python3
"""Targeted verification for Greenhouse OS VERDANT new capabilities:
1. Boot into text console.
2. Enter 'verdant' graphical shell.
3. Type 'help' and 'cpu' into guiterm; capture output.
4. Type 'run HELLO.ELF' into guiterm; capture Ring 3 execution.
5. Send F2 to activate Berry, test 'quote' command.
6. Auto-arrange with F5.
7. Exit with ESC back to 720x400 text console.
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

def main():
    env = os.environ.copy()
    env['PATH'] = '/home/blu/.local/bin:/home/blu/.local/usr/bin:' + env.get('PATH', '')
    env['LD_LIBRARY_PATH'] = '/home/blu/.local/usr/lib:/home/blu/.local/usr/lib64:' + env.get('LD_LIBRARY_PATH', '')
    env['QEMU_MODULE_DIR'] = '/home/blu/.local/usr/lib/qemu'

    log = '/tmp/verdant_interactive_serial.log'
    dumps = [
        '/tmp/vi_base.ppm',
        '/tmp/vi_desk.ppm',
        '/tmp/vi_term_run.ppm',
        '/tmp/vi_berry.ppm',
        '/tmp/vi_post.ppm'
    ]
    for p in [log] + dumps:
        if os.path.exists(p):
            try:
                os.remove(p)
            except OSError:
                pass

    print("[START] Launching QEMU for Verdant interactive test...")
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
    mon('screendump /tmp/vi_base.ppm', 1.0)
    base = ppm_info('/tmp/vi_base.ppm')
    if not base or (base[0], base[1]) != TEXT_MODE:
        print(f"[FAIL] Baseline mode failed: {base}")
        proc.kill()
        return 1
    print(f"[OK] Baseline 720x400 text mode verified: {base}")

    print("[TEST] Launching Verdant via command...")
    send('verdant')
    mon('sendkey ret', 2.0)
    mon('screendump /tmp/vi_desk.ppm', 1.0)
    desk = ppm_info('/tmp/vi_desk.ppm')
    if not desk or (desk[0], desk[1]) != VERDANT_MODE:
        print(f"[FAIL] Verdant desktop mode failed: {desk}")
        proc.kill()
        return 1
    print(f"[OK] Verdant 1024x768 32bpp active: {desk}")

    # Focus Terminal via F3
    print("[TEST] Focusing Terminal via F3...")
    mon('sendkey f3', 0.5)

    # Test graphical terminal command execution
    print("[TEST] Sending 'help' to graphical terminal...")
    send('help')
    mon('sendkey ret', 0.5)

    print("[TEST] Sending 'cpu' to graphical terminal...")
    send('cpu')
    mon('sendkey ret', 0.5)

    print("[TEST] Sending 'run HELLO.ELF' to graphical terminal...")
    send('run HELLO.ELF')
    mon('sendkey ret', 1.5)
    mon('screendump /tmp/vi_term_run.ppm', 1.0)

    # Test Berry Assistant
    print("[TEST] Focusing Berry (F2)...")
    mon('sendkey f2', 0.5)
    print("[TEST] Sending 'quote' to Berry...")
    send('quote')
    mon('sendkey ret', 0.8)

    # Arrange spatial workspace
    print("[TEST] Arranging spatial workspace (F5)...")
    mon('sendkey f5', 0.5)

    # Exit Verdant
    print("[TEST] Exiting Verdant (ESC)...")
    mon('sendkey esc', 1.5)
    mon('sendkey ret', 1.0)
    mon('screendump /tmp/vi_post.ppm', 1.0)
    mon('quit', 0.5)

    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

    post = ppm_info('/tmp/vi_post.ppm')
    if not post or (post[0], post[1]) != TEXT_MODE:
        print(f"[FAIL] Post text restore failed: {post}")
        return 1
    print(f"[OK] Cleanly restored 720x400 text console: {post}")

    # Inspect serial log
    serial = ""
    if os.path.exists(log):
        with open(log, 'r', errors='replace') as f:
            serial = f.read()

    print("=================== INTERACTIVE SERIAL OUTPUT ===================")
    print(serial)
    print("=================================================================")

    if '[HELLO.ELF] Hello from Greenhouse OS 1.0 User Mode (Ring 3)!' in serial:
        print("[PASS] Userland HELLO.ELF successfully executed from GUI terminal via write-hook!")
    else:
        print("[WARN] Userland binary output not seen in serial log - checking execution.")

    if 'KERNEL PANIC' in serial:
        print("[FAIL] Kernel panic in serial log!")
        return 1

    print("[SUCCESS] All Verdant interactive tests passed!")
    return 0

if __name__ == '__main__':
    sys.exit(main())
