#!/usr/bin/env python3
"""Verification of Greenhouse OS GUI Terminal real command execution:
Tests typing:
1. 'hello' -> executes real C:\\HELLO.ELF in Ring 3
2. 'echo hello world' -> executes real C:\\ECHO.ELF with argv
3. 'ls' -> executes real C:\\LS.ELF
4. 'ps' -> executes real C:\\PS.ELF
5. 'cat README.TXT' -> executes real C:\\CAT.ELF
6. 'pwd' -> executes real built-in pwd
7. 'dir' -> executes real directory listing
"""

import os
import subprocess
import sys
import time

TEXT_MODE = (720, 400)
VERDANT_MODE = (1024, 768)

def main():
    env = os.environ.copy()
    env['PATH'] = '/home/blu/.local/bin:/home/blu/.local/usr/bin:' + env.get('PATH', '')
    env['LD_LIBRARY_PATH'] = '/home/blu/.local/usr/lib:/home/blu/.local/usr/lib64:' + env.get('LD_LIBRARY_PATH', '')
    env['QEMU_MODULE_DIR'] = '/home/blu/.local/usr/lib/qemu'

    log = '/tmp/verdant_commands_serial.log'
    if os.path.exists(log):
        os.remove(log)

    print("[START] Launching QEMU for Greenhouse Terminal Commands Verification...")
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

    # Boot into text mode
    time.sleep(3.0)
    mon('sendkey ret', 2.0)

    # Launch Verdant
    print("[TEST] Launching Verdant GUI...")
    send('verdant')
    mon('sendkey ret', 2.0)

    # Terminal is opened and focused by default. Send 'hello'
    print("[TEST] Sending 'hello' command to GUI terminal...")
    send('hello')
    mon('sendkey ret', 1.0)

    # Send 'echo hello world'
    print("[TEST] Sending 'echo hello world' command to GUI terminal...")
    send('echo hello world')
    mon('sendkey ret', 1.0)

    # Send 'ls'
    print("[TEST] Sending 'ls' command to GUI terminal...")
    send('ls')
    mon('sendkey ret', 1.0)

    # Send 'ps'
    print("[TEST] Sending 'ps' command to GUI terminal...")
    send('ps')
    mon('sendkey ret', 1.0)

    # Send 'cat README.TXT'
    print("[TEST] Sending 'cat README.TXT' command to GUI terminal...")
    send('cat README.TXT')
    mon('sendkey ret', 1.0)

    # Send 'pwd'
    print("[TEST] Sending 'pwd' command to GUI terminal...")
    send('pwd')
    mon('sendkey ret', 0.5)

    # Send 'dir'
    print("[TEST] Sending 'dir' command to GUI terminal...")
    send('dir')
    mon('sendkey ret', 0.5)

    # Screendump for visual verification
    mon('screendump /tmp/guiterm_commands.ppm', 1.0)

    # Exit Verdant
    print("[TEST] Exiting Verdant (typing 'logout' in GUI terminal)...")
    send('logout')
    mon('sendkey ret', 2.0)
    mon('quit', 0.5)


    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

    # Read serial log
    serial = ""
    if os.path.exists(log):
        with open(log, 'r', errors='replace') as f:
            serial = f.read()

    print("=================== COMMANDS TEST SERIAL OUTPUT ===================")
    print(serial)
    print("===================================================================")

    checks = [
        ("HELLO.ELF execution", "[HELLO.ELF] Hello from Greenhouse OS 1.0 User Mode (Ring 3)!"),
        ("ECHO.ELF with argv", "hello world"),
        ("LS.ELF execution", "[LS.ELF] Current Working Directory: C:\\"),
        ("PS.ELF execution", "Greenhouse OS Userland Process Status"),
        ("CAT.ELF execution", "[CAT.ELF] Opening 'README.TXT' via sys_open (Ring 3)..."),
        ("PWD command", "C:\\"),
        ("DIR command", "Directory of C:\\")
    ]

    all_passed = True
    for name, expected in checks:
        if expected in serial:
            print(f"[PASS] {name}: '{expected}' verified in output")
        else:
            print(f"[FAIL] {name}: '{expected}' NOT found in output!")
            all_passed = False

    if 'KERNEL PANIC' in serial:
        print("[FAIL] Kernel panic detected in serial log!")
        return 1

    if all_passed:
        print("\n[ALL TESTS PASSED] Real command execution in GUI terminal 100% verified!")
        return 0
    else:
        print("\n[SOME TESTS FAILED] Check serial log output above.")
        return 1

if __name__ == '__main__':
    sys.exit(main())
