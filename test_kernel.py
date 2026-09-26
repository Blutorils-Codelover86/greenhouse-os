#!/usr/bin/env python3
import subprocess
import time
import os
import sys

def run_tests():
    env = os.environ.copy()
    env['PATH'] = '/home/blu/.local/bin:/home/blu/.local/usr/bin:' + env.get('PATH', '')
    env['LD_LIBRARY_PATH'] = '/home/blu/.local/usr/lib:/home/blu/.local/usr/lib64:' + env.get('LD_LIBRARY_PATH', '')

    serial_log = '/tmp/qemu_interactive_test.log'
    if os.path.exists(serial_log):
        os.remove(serial_log)

    print("[TEST] Ensuring FAT32 disk image exists...")
    subprocess.run(['python3', 'create_disk.py'], check=True, env=env)

    print("[TEST] Launching Greenhouse OS 0.9 in QEMU with ATA disk...")
    proc = subprocess.Popen(
        [
            'qemu-system-x86_64',
            '-boot', 'd',
            '-cdrom', 'greenhouse.iso',
            '-hda', 'disk.img',
            '-m', '256M',
            '-display', 'none',
            '-monitor', 'stdio',
            '-serial', f'file:{serial_log}'
        ],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env
    )

    def send_cmd(cmd_text, delay_after=0.8):
        print(f"[TEST] Sending Command: '{cmd_text}'")
        for char in cmd_text:
            if char == ' ':
                key = 'spc'
            elif char == '-':
                key = 'minus'
            elif char == '>':
                key = 'shift-dot'
            elif char == '\\':
                key = 'backslash'
            elif char == '/':
                key = 'slash'
            elif char == '.':
                key = 'dot'
            elif char == ':':
                key = 'shift-semicolon'
            elif char == '_':
                key = 'shift-minus'
            elif char.isupper():
                key = f'shift-{char.lower()}'
            else:
                key = char
            proc.stdin.write(f'sendkey {key}\n')
            proc.stdin.flush()
            time.sleep(0.03)
        proc.stdin.write('sendkey ret\n')
        proc.stdin.flush()
        time.sleep(delay_after)

    # Wait for GRUB menu and press Enter
    time.sleep(3.0)
    proc.stdin.write('sendkey ret\n')
    proc.stdin.flush()
    time.sleep(3.5)

    commands = [
        'help',
        'time',
        'ticks',
        'cpu',
        'mem',
        'memmap',
        'heap',
        'disks',
        'vol',
        'ps',
        'tasks',
        'test',
        'c:',
        'vol',
        'dir',
        'run HELLO.ELF',
        'run ECHO.ELF Hello from Ring 3 Greenhouse OS Userland!',
        'run CAT.ELF README.TXT',
        'run LS.ELF',
        'run PS.ELF',
        'run TEST.ELF',
        'run SLEEP.ELF 1',
        'ps',
        'type README.TXT',
        'echo Hello FAT32 Persistent File > NOTE.TXT',
        'type NOTE.TXT',
        'mkdir TESTDIR',
        'dir',
        'cd DOCS',
        'dir',
        'type INFO.TXT',
        'cd ..',
        'berry mem',
        'berry disk',
        'berry files',
        'berry status',
        'r:',
        'dir'
    ]

    for c in commands:
        send_cmd(c, delay_after=0.7)

    # Capture interactive operations screenshot
    proc.stdin.write('screendump /tmp/screen_interactive.ppm\n')
    proc.stdin.flush()
    time.sleep(0.5)

    # Intentionally trigger controlled #PF page fault panic
    print("[TEST] Intentionally triggering Page Fault Exception: PANIC PAGEFAULT")
    send_cmd('panic pagefault', delay_after=1.0)

    # Capture panic screenshot
    proc.stdin.write('screendump /tmp/screen_panic.ppm\n')
    proc.stdin.flush()
    time.sleep(0.5)

    proc.stdin.write('quit\n')
    proc.stdin.flush()
    try:
        proc.communicate(timeout=5)
    except Exception:
        proc.kill()

    print("\n" + "="*80)
    print("            GREENHOUSE OS 0.9 TEST SESSION SERIAL LOG")
    print("="*80)
    if os.path.exists(serial_log):
        with open(serial_log, 'r', errors='ignore') as f:
            content = f.read()
            print(content)
    print("="*80)

    # Convert screenshots to brain artifacts directory
    artifact_dir = '/home/blu/.gemini/antigravity-cli/brain/6339e8fb-5aed-401a-80aa-a419fa1e5b44'
    subprocess.run(['magick', '/tmp/screen_interactive.ppm', f'{artifact_dir}/screen_interactive.png'], check=False)
    subprocess.run(['magick', '/tmp/screen_panic.ppm', f'{artifact_dir}/screen_panic.png'], check=False)

if __name__ == '__main__':
    run_tests()
