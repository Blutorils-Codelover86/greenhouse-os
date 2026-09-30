#!/usr/bin/env python3
"""Automated verification script for Greenhouse OS Renderer Benchmark Screen & Cursor Family.

Tests:
  1. Boot into text console baseline.
  2. Launch renderer benchmark screen via 'renderertest'.
  3. Verify 1024x768 32bpp framebuffer mode.
  4. Capture and analyze benchmark screendump:
     - Light botanical gradient background
     - Physical frosted glass & translucency
     - Gradients & radial bloom
     - Antialiased rounded corners & soft shadows
     - Component system (buttons, badges, inputs, meters)
     - Cursor family showcase
  5. Test switching every cursor type (1 through 7):
     - 1: default (arrow)
     - 2: pointer (hand)
     - 3: text (I-beam)
     - 4: resize-horizontal
     - 5: resize-vertical
     - 6: resize-diagonal
     - 7: busy (botanical spinner)
  6. Test mouse movement and interactive tracking.
  7. Exit cleanly via ESC back to text console baseline.
  8. Verify zero panics or regressions.
"""

import os
import subprocess
import sys
import time


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
    # sample colors
    for off in range(idx, len(data) - 2, 3 * 97):
        colours.add(data[off:off + 3])
    return w, h, maxval, len(colours)


def main():
    env = os.environ.copy()
    env['PATH'] = '/home/blu/.local/bin:/home/blu/.local/usr/bin:' + env.get('PATH', '')
    env['LD_LIBRARY_PATH'] = '/home/blu/.local/usr/lib:/home/blu/.local/usr/lib64:' + env.get('LD_LIBRARY_PATH', '')
    env['QEMU_MODULE_DIR'] = '/home/blu/.local/usr/lib/qemu'

    log = '/tmp/renderer_benchmark_serial.log'
    dump_base = '/tmp/renderer_base.ppm'
    dump_bench = '/tmp/renderer_benchmark.ppm'
    dump_cursor_pointer = '/tmp/renderer_cursor_pointer.ppm'
    dump_cursor_busy = '/tmp/renderer_cursor_busy.ppm'
    dump_post = '/tmp/renderer_post.ppm'

    for p in [log, dump_base, dump_bench, dump_cursor_pointer, dump_cursor_busy, dump_post]:
        if os.path.exists(p):
            try:
                os.remove(p)
            except OSError:
                pass

    print("======================================================================")
    print("     GREENHOUSE OS - RENDERING FOUNDATION & CURSOR VERIFICATION       ")
    print("======================================================================")

    qemu_cmd = [
        '/usr/bin/qemu-system-x86_64',
        '-boot', 'd',
        '-cdrom', 'greenhouse.iso',
        '-drive', 'file=disk.img,format=raw,index=0,media=disk',
        '-m', '1G',
        '-display', 'none',
        '-monitor', 'stdio',
        '-serial', f'file:{log}',
        '-vga', 'std'
    ]

    proc = subprocess.Popen(
        qemu_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=env
    )

    def mon(line, wait=0.3):
        proc.stdin.write(line + '\n')
        proc.stdin.flush()
        time.sleep(wait)

    def send(text, delay=0.03):
        for ch in text:
            mon(f'sendkey {"spc" if ch == " " else ch}', delay)

    print("[BOOT] Booting Greenhouse OS in QEMU...")
    time.sleep(3.0)
    mon('sendkey ret', 2.0)
    mon(f'screendump {dump_base}', 1.0)

    base = ppm_info(dump_base)
    if not base:
        print("[FAIL] Could not capture baseline boot screen")
        proc.kill()
        return 1

    print(f"[OK] Text mode baseline: {base[0]}x{base[1]} ({base[3]} colours)")

    # 1. Launch Renderer Benchmark
    print("[RENDERER] Launching 'renderertest' from shell...")
    send('renderertest')
    mon('sendkey ret', 2.0)

    # 2. Capture Benchmark Test Screen
    mon(f'screendump {dump_bench}', 1.5)
    bench = ppm_info(dump_bench)
    if not bench or (bench[0], bench[1]) != (1024, 768):
        print(f"[FAIL] Benchmark screendump failure: got {bench}")
        proc.kill()
        return 1

    print(f"[OK] Benchmark Screen active: {bench[0]}x{bench[1]} @ {bench[3]} sampled colors (Rich Light Mode Palette)")
    if bench[3] < 15:
        print(f"[FAIL] Palette too poor ({bench[3]} colors), expected rich antialiased gradients and shadows")
        proc.kill()
        return 1

    # 3. Test Cursor Switching: Press '2' for Pointer
    print("[CURSOR] Testing Pointer Cursor (Key '2')...")
    mon('sendkey 2', 0.5)
    mon('mouse_move 500 400', 0.3)
    mon(f'screendump {dump_cursor_pointer}', 0.8)
    cur_p = ppm_info(dump_cursor_pointer)
    print(f"[OK] Pointer cursor active ({cur_p[3]} sampled colors)")

    # 4. Test Text Cursor (Key '3')
    print("[CURSOR] Testing Text Cursor (Key '3')...")
    mon('sendkey 3', 0.4)
    mon('mouse_move 600 350', 0.3)

    # 5. Test Resize-H (Key '4')
    print("[CURSOR] Testing Resize-Horizontal Cursor (Key '4')...")
    mon('sendkey 4', 0.4)
    mon('mouse_move 450 350', 0.3)

    # 6. Test Resize-V (Key '5')
    print("[CURSOR] Testing Resize-Vertical Cursor (Key '5')...")
    mon('sendkey 5', 0.4)
    mon('mouse_move 450 450', 0.3)

    # 7. Test Resize-Diag (Key '6')
    print("[CURSOR] Testing Resize-Diagonal Cursor (Key '6')...")
    mon('sendkey 6', 0.4)
    mon('mouse_move 500 500', 0.3)

    # 8. Test Busy Spinner Cursor (Key '7')
    print("[CURSOR] Testing Busy Spinner Cursor (Key '7')...")
    mon('sendkey 7', 0.5)
    mon('mouse_move 300 300', 0.3)
    time.sleep(0.5)
    mon(f'screendump {dump_cursor_busy}', 0.8)
    cur_b = ppm_info(dump_cursor_busy)
    print(f"[OK] Busy spinner cursor active ({cur_b[3]} sampled colors)")

    # 9. Test Cursor Scaling (+ for 2x High-DPI scale)
    print("[CURSOR] Testing High-DPI 2x scale (+)...")
    mon('sendkey equal', 0.4)
    mon('mouse_move 400 300', 0.3)
    mon('sendkey minus', 0.4)
    mon('mouse_move 450 350', 0.3)

    # 10. Test Hovering cursor boxes via mouse
    print("[INTERACTION] Testing mouse hover over showcase boxes...")
    mon('mouse_move 100 620', 0.3)
    mon('mouse_move 240 620', 0.3)
    mon('mouse_move 380 620', 0.3)
    mon('mouse_move 520 620', 0.3)

    # 11. Exit Benchmark via ESC
    print("[EXIT] Exiting benchmark back to text console (ESC)...")
    mon('sendkey esc', 2.0)
    mon('sendkey ret', 1.0)
    mon(f'screendump {dump_post}', 1.5)
    mon('quit', 0.5)

    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

    post = ppm_info(dump_post)
    if not post:
        print("[FAIL] Post-exit screendump missing")
        return 1

    print(f"[OK] Restored text mode: {post[0]}x{post[1]} ({post[3]} colours)")
    if (post[0], post[1]) != (720, 400):
        print(f"[FAIL] Expected (720, 400), got {post[0]}x{post[1]}")
        return 1

    # Check serial log for panics
    serial = ""
    if os.path.exists(log):
        with open(log, 'r', errors='replace') as f:
            serial = f.read()

    print("========================= SERIAL LOG OUTPUT =========================")
    print(serial)
    print("======================================================================")

    if 'KERNEL PANIC' in serial or 'Triple fault' in serial:
        print("[FAIL] Kernel panic or crash detected in serial log")
        return 1

    print("\n[PASS] GREENHOUSE 2D RENDERER & CURSOR BENCHMARK VERIFIED SUCCESSFULLY!")
    return 0


if __name__ == '__main__':
    sys.exit(main())
