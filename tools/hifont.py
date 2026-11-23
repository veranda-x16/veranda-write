#!/usr/bin/env python3
"""Test fonts with the Veranda set's Polish letters, for Veranda Write's
gate (DECISIONS W38): a GEOS font file's records made the extended
records of Veranda's V136 (an index of 193 words: characters $20-$7F,
$A0-$FF of ISO-8859-16, the end; the bitmap right after it), the 18
Polish letters composed from their base letters (an acute, a dot, an
ogonek, a stroke: placeholders, not Veranda's drawn glyphs, which
Veranda's tools/fontcvt.py makes), the other characters of $A0-$FF left
out (a width of 0: drawn as '?'). A record that has them already (Veranda's own
fonts since V136) is kept as it is.

usage: hifont.py <in.cvt> <out.cvt>
       hifont.py render <font.cvt> <size> <text (UTF-8)>   rows as # and .

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import sys

# the Polish letters: their ISO-8859-16 byte, base letter, mark
POLISH = {
    'ą': ('a', 'ogonek'), 'ć': ('c', 'acute'), 'ę': ('e', 'ogonek'), 'ł': ('l', 'stroke'),
    'ń': ('n', 'acute'), 'ó': ('o', 'acute'), 'ś': ('s', 'acute'), 'ź': ('z', 'acute'), 'ż': ('z', 'dot'),
    'Ą': ('A', 'ogonek'), 'Ć': ('C', 'acute'), 'Ę': ('E', 'ogonek'), 'Ł': ('L', 'stroke'),
    'Ń': ('N', 'acute'), 'Ó': ('O', 'acute'), 'Ś': ('S', 'acute'), 'Ź': ('Z', 'acute'), 'Ż': ('Z', 'dot'),
}


def parse(rec):
    base, setw, h = rec[0], rec[1] | rec[2] << 8, rec[3]
    io, bo = rec[4] | rec[5] << 8, rec[6] | rec[7] << 8
    n = 193 if bo - io == 386 else 97
    idx = [rec[io + 2 * i] | rec[io + 2 * i + 1] << 8 for i in range(n)]
    bm = rec[bo:bo + setw * h]
    gl = []
    for k in range(n - 1):
        a, b = idx[k], idx[k + 1]
        gl.append([[(bm[r * setw + (x >> 3)] >> (7 - (x & 7))) & 1 for x in range(a, b)] for r in range(h)])
    return base, h, gl


def glyph_of(gl, c):
    if 0x20 <= c < 0x80:
        return gl[c - 0x20]
    if c >= 0xA0 and len(gl) > 96:
        return gl[c - 0x40]
    return None


def compose(g, mark, base, h):
    g = [list(r) for r in g]
    w = len(g[0])
    ink = [x for r in g for x in range(w) if r[x]]
    if not ink:
        return g
    lo, hi = min(ink), max(ink)
    top = next(r for r in range(h) if any(g[r]))
    mid = (lo + hi + 1) // 2
    if mark in ('acute', 'dot'):
        r = max(0, top - 2)
        if top >= 2:
            pass
        else:                           # (no room above: the top row made room)
            g[0] = [0] * w
            r = 0
        if mark == 'acute':
            g[r][min(hi, mid + 1)] = 1
            if r + 1 < top or top < 2:
                g[r + 1 if top >= 2 else r][mid] = 1
        else:
            g[r][mid] = 1
    elif mark == 'ogonek':
        r = min(h - 1, base + 1)
        g[r][max(lo, hi - 1)] = 1
        if r + 1 < h:
            g[r + 1][hi] = 1
    elif mark == 'stroke':
        rm = (top + base) // 2
        stem = next(x for x in range(w) if any(g[rr][x] for rr in range(h)))
        if stem > 0:
            g[rm + 1][stem - 1] = 1
        if stem + 1 < w:
            g[rm][stem + 1] = 1
    return g


def build(base, h, gl):
    """96 ASCII glyphs + 96 of $A0-$FF (None: left out) -> an extended record"""
    pos, x = [], 0
    allg = gl[:96] + gl[96:192]
    for g in allg:
        pos.append(x)
        x += len(g[0]) if g else 0
    pos.append(x)
    setw = (x + 7) // 8
    bm = bytearray(setw * h)
    for g, p in zip(allg, pos):
        if not g:
            continue
        for r, row in enumerate(g):
            for i, v in enumerate(row):
                if v:
                    bm[r * setw + (p + i) // 8] |= 0x80 >> ((p + i) % 8)
    io, bo = 8, 8 + 2 * 193
    out = bytearray([base, setw & 255, setw >> 8, h, io, 0, bo & 255, bo >> 8])
    for p in pos:
        out += bytes([p & 255, p >> 8])
    return bytes(out + bm)


def extend(rec):
    base, h, gl = parse(rec)
    if len(gl) > 96:
        return rec                      # (it has them: Veranda's own glyphs kept)
    gl = gl[:96] + [None] * 96
    for ch, (b, mark) in POLISH.items():
        c = ch.encode('iso8859_16')[0]
        gl[c - 0x40] = compose(gl[ord(b) - 0x20], mark, base, h)
    return build(base, h, gl)


def records(data):
    tab = data[508:762]
    recs, off = {}, 762
    for n in range(127):
        b, l = tab[2 * n], tab[2 * n + 1]
        if b == 0 and l == 0:
            break
        if b == 0:
            continue
        ln = (b - 1) * 254 + l - 1
        recs[n] = data[off:off + ln]
        off += b * 254
    return recs


def write(data, recs):
    out = bytearray(data[:508])
    tab = bytearray(254)
    body = bytearray()
    last = max(recs)
    total = 1
    for n in range(last + 1):
        if n in recs:
            r = recs[n]
            nb = (len(r) + 253) // 254
            tab[2 * n], tab[2 * n + 1] = nb, (len(r) - 1) % 254 + 2
            body += r + bytes(nb * 254 - len(r))
            total += nb
        else:
            tab[2 * n], tab[2 * n + 1] = 0, 0xFF
    out[28], out[29] = (total + 1) & 255, (total + 1) >> 8
    return bytes(out + tab + body)


def render(path, size, text):
    base, h, gl = parse(records(open(path, 'rb').read())[size])
    rows = [''] * h
    for c in text.encode('iso8859_16'):
        g = glyph_of(gl, c)
        if not g or not g[0]:
            g = gl['?'.encode()[0] - 0x20]
        for r in range(h):
            rows[r] += ''.join('#' if v else '.' for v in g[r])
    return rows


def main():
    a = sys.argv[1:]
    if len(a) == 4 and a[0] == 'render':
        print('\n'.join(render(a[1], int(a[2]), a[3])))
    elif len(a) == 2:
        data = open(a[0], 'rb').read()
        recs = {n: extend(r) for n, r in records(data).items()}
        open(a[1], 'wb').write(write(data, recs))
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
