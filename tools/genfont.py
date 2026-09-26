#!/usr/bin/env python3
"""
Greenhouse OS - built-in bitmap font generator.

Rasterises the printable ASCII range (0x20..0x7E) of a monospaced TrueType
font into a fixed 8x16 1-bit-per-pixel console style bitmap and emits the
C source table used by kernel/graphics/font.c.

The generated glyph data is a derivative bitmap rendering of:

    Adwaita Mono Regular
    Copyright 2011-2022 The Adwaita Mono Project Authors
    Licensed under the SIL Open Font License, Version 1.1
    https://github.com/googlefonts/AdwaitaMono

Usage:
    python3 tools/genfont.py <font.ttf> > kernel/graphics/font_data.inc

The generator is intentionally dependency free (no Pillow / fontTools) so the
build environment stays minimal: it only needs a TrueType file and CPython.
"""

import struct
import sys

GLYPH_W = 8
GLYPH_H = 16
FIRST_CHAR = 0x20
LAST_CHAR = 0x7E

# Target metrics: classic 8x16 console look.
CAP_HEIGHT_PX = 10.0
BASELINE_PX = 12.0
SUPERSAMPLE = 5


class TrueType:
    def __init__(self, data):
        self.data = data
        (sfnt, num_tables, _sr, _es, _rs) = struct.unpack(">IHHHH", data[0:12])
        if sfnt not in (0x00010000, 0x74727565):
            raise ValueError("not a TrueType font (sfnt=0x%08x)" % sfnt)
        self.tables = {}
        for i in range(num_tables):
            off = 12 + i * 16
            tag, _csum, toff, tlen = struct.unpack(">4sIII", data[off:off + 16])
            self.tables[tag.decode("latin1")] = (toff, tlen)

        head_off = self.tables["head"][0]
        self.units_per_em = struct.unpack(">H", data[head_off + 18:head_off + 20])[0]
        self.index_to_loc = struct.unpack(">h", data[head_off + 50:head_off + 52])[0]

        maxp_off = self.tables["maxp"][0]
        self.num_glyphs = struct.unpack(">H", data[maxp_off + 4:maxp_off + 6])[0]

        hhea_off = self.tables["hhea"][0]
        self.ascender = struct.unpack(">h", data[hhea_off + 4:hhea_off + 6])[0]
        self.descender = struct.unpack(">h", data[hhea_off + 6:hhea_off + 8])[0]
        self.num_hmetrics = struct.unpack(">H", data[hhea_off + 34:hhea_off + 36])[0]

        self._read_loca()
        self._read_hmtx()
        self._read_cmap()

    def _read_loca(self):
        off = self.tables["loca"][0]
        n = self.num_glyphs + 1
        if self.index_to_loc == 0:
            raw = struct.unpack(">%dH" % n, self.data[off:off + n * 2])
            self.loca = [v * 2 for v in raw]
        else:
            self.loca = list(struct.unpack(">%dI" % n, self.data[off:off + n * 4]))

    def _read_hmtx(self):
        off = self.tables["hmtx"][0]
        self.advances = []
        for i in range(self.num_hmetrics):
            adv = struct.unpack(">H", self.data[off + i * 4:off + i * 4 + 2])[0]
            self.advances.append(adv)
        if not self.advances:
            self.advances = [self.units_per_em // 2]

    def advance(self, gid):
        if gid < len(self.advances):
            return self.advances[gid]
        return self.advances[-1]

    def _read_cmap(self):
        off = self.tables["cmap"][0]
        n = struct.unpack(">H", self.data[off + 2:off + 4])[0]
        best = None
        for i in range(n):
            rec = off + 4 + i * 8
            pid, eid, sub = struct.unpack(">HHI", self.data[rec:rec + 8])
            score = -1
            if (pid, eid) == (3, 1):
                score = 3
            elif (pid, eid) == (0, 3):
                score = 2
            elif (pid, eid) == (3, 10):
                score = 4
            elif pid == 3 and eid == 0:
                score = 1
            if score >= 0 and (best is None or score > best[0]):
                best = (score, off + sub)
        if best is None:
            raise ValueError("no usable cmap subtable")
        self.cmap_off = best[1]
        fmt = struct.unpack(">H", self.data[self.cmap_off:self.cmap_off + 2])[0]
        if fmt == 12:
            # Segmented coverage: startCharCode, endCharCode, startGlyphID
            self.cmap12 = []
            ngroups = struct.unpack(">I", self.data[self.cmap_off + 12:self.cmap_off + 16])[0]
            base = self.cmap_off + 16
            for i in range(ngroups):
                s, e, sg = struct.unpack(">III", self.data[base + i * 12:base + i * 12 + 12])
                self.cmap12.append((s, e, sg))
            self.cmap_tables = []
            return
        if fmt != 4:
            raise ValueError("cmap format %d unsupported (need 4 or 12)" % fmt)
        self.cmap_tables = []
        seg_x2 = struct.unpack(">H", self.data[self.cmap_off + 6:self.cmap_off + 8])[0]
        seg = seg_x2 // 2
        base = self.cmap_off + 14
        ends = struct.unpack(">%dH" % seg, self.data[base:base + seg_x2])
        base += seg_x2 + 2
        starts = struct.unpack(">%dH" % seg, self.data[base:base + seg_x2])
        base += seg_x2
        deltas = struct.unpack(">%dh" % seg, self.data[base:base + seg_x2])
        base += seg_x2
        range_off_pos = base
        range_offs = struct.unpack(">%dH" % seg, self.data[base:base + seg_x2])
        for i in range(seg):
            self.cmap_tables.append((starts[i], ends[i], deltas[i], range_offs[i], range_off_pos + i * 2))

    def glyph_id(self, codepoint):
        for (start, end, sg) in getattr(self, "cmap12", []):
            if start <= codepoint <= end:
                return sg + (codepoint - start)
        for (start, end, delta, ro, ro_pos) in self.cmap_tables:
            if start <= codepoint <= end:
                if ro == 0:
                    return (codepoint + delta) & 0xFFFF
                addr = ro_pos + ro + (codepoint - start) * 2
                gid = struct.unpack(">H", self.data[addr:addr + 2])[0]
                if gid == 0:
                    return 0
                return (gid + delta) & 0xFFFF
        return 0

    def glyph_contours(self, gid, depth=0):
        """Return list of contours, each a list of (x, y) in font units."""
        goff = self.tables["glyf"][0]
        start, end = self.loca[gid], self.loca[gid + 1]
        if start == end:
            return []
        g = self.data[goff + start:goff + end]
        ncont = struct.unpack(">h", g[0:2])[0]
        if ncont < 0:
            return self._composite(g, depth)

        pos = 10
        end_pts = struct.unpack(">%dH" % ncont, g[pos:pos + n * 2]) if False else struct.unpack(">%dH" % ncont, g[pos:pos + ncont * 2])
        pos += ncont * 2
        npoints = end_pts[-1] + 1
        ilen = struct.unpack(">H", g[pos:pos + 2])[0]
        pos += 2 + ilen

        flags = []
        while len(flags) < npoints:
            f = g[pos]
            pos += 1
            flags.append(f)
            if f & 0x08:
                rep = g[pos]
                pos += 1
                flags.extend([f] * rep)
        flags = flags[:npoints]

        xs = []
        v = 0
        for f in flags:
            if f & 0x02:
                d = g[pos]
                pos += 1
                v += d if (f & 0x10) else -d
            elif not (f & 0x10):
                d = struct.unpack(">h", g[pos:pos + 2])[0]
                pos += 2
                v += d
            xs.append(v)

        ys = []
        v = 0
        for f in flags:
            if f & 0x04:
                d = g[pos]
                pos += 1
                v += d if (f & 0x20) else -d
            elif not (f & 0x20):
                d = struct.unpack(">h", g[pos:pos + 2])[0]
                pos += 2
                v += d
            ys.append(v)

        contours = []
        s = 0
        for e in end_pts:
            pts = [(xs[i], ys[i], bool(flags[i] & 1)) for i in range(s, e + 1)]
            contours.append(flatten_contour(pts))
            s = e + 1
        return contours

    def _composite(self, g, depth):
        if depth > 4:
            return []
        pos = 10
        contours = []
        while True:
            flags, gi = struct.unpack(">HH", g[pos:pos + 4])
            pos += 4
            if flags & 0x0001:  # ARG_1_AND_2_ARE_WORDS
                a1, a2 = struct.unpack(">hh", g[pos:pos + 4])
                pos += 4
            else:
                a1, a2 = struct.unpack(">bb", g[pos:pos + 2])
                pos += 2
            a1b, a2b = a1, a2
            if flags & 0x0002:  # ARGS_ARE_XY_VALUES
                pass
            else:
                a1b = 0
                a2b = 0
            sx = sy = 1.0
            s01 = s10 = 0.0
            if flags & 0x0008:  # WE_HAVE_A_SCALE
                sx = sy = f2dot14(g, pos)
                pos += 2
            elif flags & 0x0040:
                sx = f2dot14(g, pos)
                sy = f2dot14(g, pos + 2)
                pos += 4
            elif flags & 0x0080:
                sx = f2dot14(g, pos)
                s01 = f2dot14(g, pos + 2)
                s10 = f2dot14(g, pos + 4)
                sy = f2dot14(g, pos + 6)
                pos += 8
            dx, dy = (a1b, a2b)
            if flags & 0x0002:
                if not (flags & 0x0001):
                    dx, dy = a1b, a2b
            for c in self.glyph_contours(gi, depth + 1):
                contours.append([(x * sx + y * s10 + dx, x * s01 + y * sy + dy) for (x, y) in c])
            if not (flags & 0x0020):  # MORE_COMPONENTS
                break
        return contours


def f2dot14(g, pos):
    return struct.unpack(">h", g[pos:pos + 2])[0] / 16384.0


def flatten_contour(pts, steps=8):
    """Turn a TrueType contour (on/off curve points) into a polyline."""
    if not pts:
        return []
    # Build an on-curve start point (implicit midpoints for off-curve runs).
    expanded = []
    n = len(pts)
    for i in range(n):
        x, y, on = pts[i]
        expanded.append((x, y, on))
    if not expanded[0][2]:
        if expanded[-1][2]:
            expanded.insert(0, expanded[-1])
        else:
            mx = (expanded[0][0] + expanded[-1][0]) / 2.0
            my = (expanded[0][1] + expanded[-1][1]) / 2.0
            expanded.insert(0, (mx, my, True))
    out = [(float(expanded[0][0]), float(expanded[0][1]))]
    i = 1
    total = len(expanded)
    while i <= total:
        px, py, on = expanded[i % total]
        if on:
            out.append((float(px), float(py)))
            i += 1
            continue
        # off-curve control point
        nx, ny, non = expanded[(i + 1) % total]
        if non:
            ex, ey = float(nx), float(ny)
            i += 2
        else:
            ex = (px + nx) / 2.0
            ey = (py + ny) / 2.0
            i += 1
        x0, y0 = out[-1]
        for s in range(1, steps + 1):
            t = s / float(steps)
            u = 1.0 - t
            out.append((u * u * x0 + 2 * u * t * px + t * t * ex,
                        u * u * y0 + 2 * u * t * py + t * t * ey))
    return out


def fill_polys(contours, w, h, scale, ox, oy, ss):
    """Rasterise polygons (non-zero winding) into a w*h coverage bitmap.

    Font units are converted to cell pixels with y pointing down: the baseline
    sits at oy and one font unit equals `scale` pixels.
    """
    cov = [[0.0] * w for _ in range(h)]
    edges = []
    for c in contours:
        for i in range(len(c)):
            x0, y0 = c[i]
            x1, y1 = c[(i + 1) % len(c)]
            if y0 == y1:
                continue
            edges.append((x0, y0, x1, y1))
    for py in range(h * ss):
        y_out = (py + 0.5) / ss
        y_fu = (oy - y_out) / scale  # font y grows up, cell y grows down
        xs = []
        for (x0, y0, x1, y1) in edges:
            if (y0 <= y_fu < y1) or (y1 <= y_fu < y0):
                t = (y_fu - y0) / (y1 - y0)
                xs.append((x0 + t * (x1 - x0), 1 if y1 > y0 else -1))
        if not xs:
            continue
        xs.sort()
        winding = 0
        row = py // ss
        for i in range(len(xs) - 1):
            winding += xs[i][1]
            if winding != 0:
                sx0 = xs[i][0] * scale + ox
                sx1 = xs[i + 1][0] * scale + ox
                a = int(max(0, sx0))
                b = int(min(w, sx1 + 1))
                for px in range(a, b):
                    cov[row][px] += 1.0
    total = float(ss)  # ss sub-scanlines per output row (x is sampled at full width)
    return [[min(1.0, cov[y][x] / total) for x in range(w)] for y in range(h)]


def render_char(font, ch, cap_height_px, baseline_px, cap_units):
    gid = font.glyph_id(ord(ch))
    contours = font.glyph_contours(gid) if gid else []
    scale = cap_height_px / float(cap_units)
    adv = font.advance(gid) if gid else font.units_per_em // 2
    advance_px = adv * scale
    # Monospaced faces centre their ink inside the advance box already, so we
    # only need to centre the advance box inside the cell.
    ox = (GLYPH_W - advance_px) / 2.0
    oy = baseline_px
    if not contours:
        return [[0.0] * GLYPH_W for _ in range(GLYPH_H)]
    return fill_polys(contours, GLYPH_W, GLYPH_H, scale, ox, oy, SUPERSAMPLE)


def main():
    if len(sys.argv) < 2:
        print("usage: genfont.py <font.ttf>", file=sys.stderr)
        return 1
    with open(sys.argv[1], "rb") as f:
        font = TrueType(f.read())

    threshold = 0.40
    for arg in sys.argv[2:]:
        try:
            threshold = float(arg)
        except ValueError:
            pass

    rows = []
    hgid = font.glyph_id(ord("H"))
    hcs = font.glyph_contours(hgid) if hgid else []
    cap_units = max([p[1] for c in hcs for p in c] or [font.ascender * 0.7])
    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        ch = chr(code)
        cov = render_char(font, ch, CAP_HEIGHT_PX, BASELINE_PX, cap_units)
        bytes_out = []
        for y in range(GLYPH_H):
            b = 0
            for x in range(GLYPH_W):
                if cov[y][x] >= threshold:
                    b |= (0x80 >> x)
            bytes_out.append(b)
        rows.append(bytes_out)

    if "-v" in sys.argv:
        for code in range(FIRST_CHAR, LAST_CHAR + 1):
            print("0x%02X '%s'" % (code, chr(code)))
            for y in range(GLYPH_H):
                b = rows[code - FIRST_CHAR][y]
                print("    " + "".join("#" if (b & (0x80 >> x)) else "." for x in range(GLYPH_W)))
        return 0

    out = []
    out.append("/* Auto-generated by tools/genfont.py - do not edit by hand. */")
    out.append("/* Derived from Adwaita Mono Regular (SIL Open Font License 1.1).     */")
    out.append("")
    out.append("static const uint8_t font8x16_glyphs[%d][%d] = {" % (len(rows), GLYPH_H))
    for idx, glyph in enumerate(rows):
        code = idx + FIRST_CHAR
        if idx % 8 == 0:
            out.append("    /* 0x%02X */" % code)
        out.append("    { " + " ".join("0x%02X," % b for b in glyph) + " },")
    out.append("};")
    out.append("")
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
