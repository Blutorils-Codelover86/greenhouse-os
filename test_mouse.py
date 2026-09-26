#!/usr/bin/env python3
"""Ad-hoc mouse test: inject motion/clicks through the QEMU monitor and check
that the PS/2 driver posts events."""
import subprocess
import time
import os

env = os.environ.copy()
env['PATH'] = '/home/blu/.local/bin:/home/blu/.local/usr/bin:' + env.get('PATH', '')
env['LD_LIBRARY_PATH'] = '/home/blu/.local/lib:/home/blu/.local/lib64:' + env.get('LD_LIBRARY_PATH', '')

log = '/tmp/mouse_serial.log'
if os.path.exists(log):
    os.remove(log)

proc = subprocess.Popen(
    ['qemu-system-x86_64', '-boot', 'd', '-cdrom', 'greenhouse.iso',
     '-m', '256M', '-display', 'none', '-monitor', 'stdio',
     '-serial', f'file:{log}', '-vga', 'std'],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    text=True, env=env)

def mon(cmd, delay=0.3):
    proc.stdin.write(cmd + '\n')
    proc.stdin.flush()
    time.sleep(delay)

def key(name, delay=0.05):
    mon(f'sendkey {name}', delay)

def send_cmd(text, delay=1.0):
    for ch in text:
        if ch == ' ':
            key('spc')
        elif ch.isupper():
            key(f'shift-{ch.lower()}')
        else:
            key(ch)
        time.sleep(0.03)
    key('ret', delay)

time.sleep(3.0)
key('ret', 4.0)

print("[TEST] moving mouse")
mon('mouse_move 40 30', 0.4)
mon('mouse_move -20 15', 0.4)
mon('mouse_move 100 60', 0.4)
print("[TEST] clicking")
mon('mouse_button 1', 0.4)
mon('mouse_button 0', 0.4)
print("[TEST] wheel")
mon('mouse_button 8', 0.4)

send_cmd('input', 1.2)
mon('screendump /tmp/mouse_screen.ppm', 0.5)
mon('quit', 0.2)
try:
    proc.wait(timeout=5)
except subprocess.TimeoutExpired:
    proc.kill()

print(open(log, errors='replace').read()[-2500:])
