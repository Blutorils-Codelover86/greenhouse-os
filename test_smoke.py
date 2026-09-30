#!/usr/bin/env python3
"""Ad-hoc boot smoke test: text mode + new graphics/input shell commands."""
import subprocess
import time
import os
import sys

def main():
    env = os.environ.copy()
    env['PATH'] = '/home/blu/.local/bin:/home/blu/.local/usr/bin:' + env.get('PATH', '')
    env['LD_LIBRARY_PATH'] = '/home/blu/.local/lib:/home/blu/.local/lib64:' + env.get('LD_LIBRARY_PATH', '')

    log = '/tmp/smoke_serial.log'
    if os.path.exists(log):
        os.remove(log)

    proc = subprocess.Popen(
        ['qemu-system-x86_64', '-boot', 'd', '-cdrom', 'greenhouse.iso',
         '-m', '1G', '-display', 'none', '-monitor', 'stdio',
         '-serial', f'file:{log}', '-vga', 'std'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, env=env)

    def key(name):
        proc.stdin.write(f'sendkey {name}\n')
        proc.stdin.flush()

    def send_cmd(text, delay=1.0):
        for ch in text:
            if ch == ' ':
                key('spc')
            elif ch.isupper():
                key(f'shift-{ch.lower()}')
            else:
                key(ch)
            time.sleep(0.03)
        key('ret')
        time.sleep(delay)

    time.sleep(3.0)
    key('ret')
    time.sleep(4.0)

    for c in sys.argv[1:] or ['gfxinfo', 'input']:
        print(f"[SMOKE] {c}")
        send_cmd(c)

    proc.stdin.write('screendump /tmp/smoke_screen.ppm\n')
    proc.stdin.flush()
    time.sleep(0.5)
    proc.stdin.write('quit\n')
    proc.stdin.flush()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

    print(open(log, errors='replace').read())

main()
