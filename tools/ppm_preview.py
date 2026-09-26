#!/usr/bin/env python3
"""Coarse ASCII preview of a screendump, so the layout can be eyeballed in a
terminal: python3 tools/ppm_preview.py /tmp/gfx_mode.ppm [cols] [rows]"""
import sys

RAMP = " .:-=+*#%@"


def main():
    path = sys.argv[1]
    cols = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    rows = int(sys.argv[3]) if len(sys.argv) > 3 else 30

    data = open(path, 'rb').read()
    assert data.startswith(b'P6'), "not a binary PPM"
    fields, idx = [], 2
    while len(fields) < 3:
        while data[idx:idx + 1].isspace():
            idx += 1
        if data[idx:idx + 1] == b'#':
            while data[idx:idx + 1] != b'\n':
                idx += 1
            continue
        start = idx
        while not data[idx:idx + 1].isspace():
            idx += 1
        fields.append(int(data[start:idx]))
    w, h, _ = fields
    idx += 1
    pixels = data[idx:]

    print(f"{path}: {w}x{h} -> {cols}x{rows}")
    for r in range(rows):
        line = []
        for c in range(cols):
            # average a block so text becomes visible structure
            x0, x1 = c * w // cols, max(c * w // cols + 1, (c + 1) * w // cols)
            y0, y1 = r * h // rows, max(r * h // rows + 1, (r + 1) * h // rows)
            total = count = 0
            for y in range(y0, y1, max(1, (y1 - y0) // 3)):
                base = y * w * 3
                for x in range(x0, x1, max(1, (x1 - x0) // 3)):
                    o = base + x * 3
                    if o + 2 < len(pixels):
                        total += (pixels[o] * 299 + pixels[o + 1] * 587 + pixels[o + 2] * 114) // 1000
                        count += 1
            lum = (total // count) if count else 0
            line.append(RAMP[min(len(RAMP) - 1, lum * len(RAMP) // 256)])
        print("".join(line))


main()
