#!/usr/bin/env python3
"""Graphics + GUI runtime test.

Boots the OS, enters the desktop through the `gui` shell command, grabs a
screendump while the desktop is on screen, then leaves the GUI again and
verifies the display went back to 80x25 text mode.

The restore check compares the post-exit screendump against the baseline boot
capture *and* against the 80x25 VGA text geometry this OS boots into, so a boot
loader that hands over a graphics mode cannot pass by matching its own
baseline.

Usage: python3 test_gfx.py [gui|gfx] [width [height [bpp]]]
"""
import os
import re
import subprocess
import sys
import time

# 80x25 cells of 9x16 pixels: the text console the OS boots into.
TEXT_MODE = (720, 400)


def ppm_info(path):
    """Returns (width, height, distinct_colours) for a binary PPM screendump."""
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
    what = sys.argv[1] if len(sys.argv) > 1 else 'gui'
    # Optional requested mode, e.g. `gfx 640 480 16`. Missing fields keep the
    # driver default and are simply not sent.
    mode = [int(a) for a in sys.argv[2:6]] if len(sys.argv) > 1 else []
    if len(sys.argv) > 2 and (what != 'gfx' or not mode or any(v <= 0 for v in mode)):
        print(f"usage: {sys.argv[0]} [gui|gfx] [width [height [bpp]] [hold]]")
        return 1
    # Without a hold the self test leaves the mode before the first screendump,
    # so there is no graphics surface left to capture: force a hold and make it
    # long enough for the monitor command to land inside it.
    hold = mode[3] if len(mode) > 3 else 0
    if what == 'gfx' and any(mode[:3]) and hold == 0:
        hold = 8
        print(f'[INFO] no hold requested, using {hold}s so the surface can be captured')
    while len(mode) < 4:
        mode.append(0)
    mode[3] = hold

    env = os.environ.copy()
    env['PATH'] = '/home/blu/.local/bin:/home/blu/.local/usr/bin:' + env.get('PATH', '')
    env['LD_LIBRARY_PATH'] = '/home/blu/.local/lib:/home/blu/.local/lib64:' + env.get('LD_LIBRARY_PATH', '')

    log = '/tmp/gfx_serial.log'
    for path in (log, '/tmp/gfx_base.ppm', '/tmp/gfx_mode.ppm', '/tmp/gfx_text.ppm'):
        if os.path.exists(path):
            os.remove(path)

    proc = subprocess.Popen(
        ['qemu-system-x86_64', '-boot', 'd', '-cdrom', 'greenhouse.iso',
         '-m', '256M', '-display', 'none', '-monitor', 'stdio',
         '-serial', f'file:{log}', '-vga', 'std'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, env=env)

    def mon(line, wait=0.4):
        proc.stdin.write(line + '\n')
        proc.stdin.flush()
        time.sleep(wait)

    def send(text, delay=0.03):
        for ch in text:
            mon(f'sendkey {"spc" if ch == " " else ch}', delay)

    time.sleep(3.0)
    mon('sendkey ret', 4.0)
    mon('screendump /tmp/gfx_base.ppm', 1.0)

    baseline = None
    boot_failure = False
    if os.path.exists('/tmp/gfx_base.ppm'):
        baseline = ppm_info('/tmp/gfx_base.ppm')
        print(f"[INFO] baseline text mode: {baseline[0]}x{baseline[1]}")
        if (baseline[0], baseline[1]) != TEXT_MODE:
            print(f"[FAIL] the OS did not boot into the {TEXT_MODE[0]}x{TEXT_MODE[1]}"
                  f" text console, got {baseline[0]}x{baseline[1]}")
            boot_failure = True
    else:
        print("[FAIL] no boot screendump to compare against")
        boot_failure = True

    command = what
    if what == 'gfx' and any(mode):
        given = ' '.join(str(v) for v in mode if v)
        command = f'gfx {given}'
        print(f'[GFX] requesting "{command}"')
    print(f"[GFX] running '{command}'")
    send(command)
    mon('sendkey ret', 2.0)
    command_sent = time.time()

    # Let the desktop/self test paint, then capture what the display shows.  A
    # held self test is still on screen here; a self test without a hold has
    # already returned to the text console.
    mon('screendump /tmp/gfx_mode.ppm', 2.5)

    if what == 'gui':
        # Drive the GUI: move the pointer over a window, click, then leave.
        mon('mouse_move 120 100', 0.4)
        mon('mouse_button 1', 0.3)
        mon('mouse_button 0', 0.6)
        mon('screendump /tmp/gfx_mode.ppm', 1.5)
        mon('sendkey esc', 1.5)

    if hold > 0:
        # The driver holds the surface for the requested time before handing the
        # display back, so wait it out or the "text" dump catches the graphics
        # mode and its geometry.
        remaining = hold + 2.0 - (time.time() - command_sent)
        if remaining > 0:
            print(f'[INFO] waiting {remaining:.1f}s for the held surface to be released')
            time.sleep(remaining)

    mon('screendump /tmp/gfx_text.ppm', 1.5)
    mon('quit', 1.0)
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

    failures = 1 if boot_failure else 0
    for label, path in (('graphics mode', '/tmp/gfx_mode.ppm'),
                        ('text mode', '/tmp/gfx_text.ppm')):
        if not os.path.exists(path):
            print(f"[FAIL] no screendump for {label}")
            failures += 1
            continue
        info = ppm_info(path)
        if info is None:
            print(f"[FAIL] unreadable screendump for {label}")
            failures += 1
            continue
        w, h, maxval, colours = info
        print(f"[INFO] {label}: {w}x{h} maxval {maxval}, {colours} sampled colours")
        if label == 'graphics mode' and what == 'gui':
            if (w, h) != (1024, 768):
                print(f"[FAIL] expected a 1024x768 graphics mode, got {w}x{h}")
                failures += 1
            if colours < 8:
                print(f"[FAIL] graphics screendump looks blank ({colours} colours)")
                failures += 1
        if label == 'text mode':
            # The firmware text console is 80x25 cells of 9x16 pixels. Checking
            # the geometry against a fixed value as well as against the boot
            # baseline keeps a boot loader that hands over a graphics mode from
            # hiding behind a matching baseline.
            if (w, h) != TEXT_MODE:
                print(f'[FAIL] expected the {TEXT_MODE[0]}x{TEXT_MODE[1]} text console, got {w}x{h}')
                failures += 1
            if baseline is not None and (w, h) != (baseline[0], baseline[1]):
                print(f"[FAIL] expected the boot text mode {baseline[0]}x{baseline[1]} after leaving,"
                      f" got {w}x{h}")
                failures += 1

    serial = open(log, errors='replace').read()
    print("=" * 70)
    print(serial)
    print("=" * 70)
    if 'KERNEL PANIC' in serial:
        print("[FAIL] kernel panic in serial log")
        failures += 1
    if 'Result:         FAIL' in serial:
        print("[FAIL] graphics self test reported failures")
        failures += 1
    if 'Result:         PASS' in serial and what == 'gfx':
        print("[PASS] self test reported PASS")

    if what == 'gfx' and any(mode):
        # The adapter's answer, not the request, is what the surface must be.
        want_w, want_h, want_bpp = mode[0], mode[1], mode[2]
        surface = re.search(r'Surface:\s+(\d+) x (\d+) @ (\d+) bpp', serial)
        if not surface:
            print('[FAIL] no surface line in the self test report')
            failures += 1
        else:
            got_w, got_h, got_bpp = (int(g) for g in surface.groups())
            print(f'[INFO] self test ran at {got_w}x{got_h} @ {got_bpp} bpp')
            if (got_w, got_h) != (want_w, want_h):
                print(f'[FAIL] expected the self test to run at {want_w}x{want_h},'
                      f' got {got_w}x{got_h}')
                failures += 1
            if want_bpp and got_bpp != want_bpp:
                print(f'[FAIL] expected {want_bpp} bpp, got {got_bpp}')
                failures += 1
            if hold > 0 and os.path.exists('/tmp/gfx_mode.ppm'):
                mode_ppm = ppm_info('/tmp/gfx_mode.ppm')
                if mode_ppm and (mode_ppm[0], mode_ppm[1]) != (got_w, got_h):
                    print(f"[FAIL] screendump is {mode_ppm[0]}x{mode_ppm[1]} but the adapter"
                          f" reported {got_w}x{got_h}")
                    failures += 1
                elif mode_ppm:
                    if mode_ppm[3] < 8:
                        print(f"[FAIL] {got_bpp} bpp screendump looks blank"
                              f" ({mode_ppm[3]} colours)")
                        failures += 1
                    else:
                        print(f"[PASS] screendump of the {got_bpp} bpp surface has"
                              f" {mode_ppm[3]} sampled colours")

    print("GFX TEST: " + ("PASS" if failures == 0 else f"FAIL ({failures})"))
    return 1 if failures else 0


sys.exit(main())
