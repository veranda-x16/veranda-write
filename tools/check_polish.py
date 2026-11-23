#!/usr/bin/env python3
"""Veranda Write's gate, Polish (DECISIONS W38): a screenshot holds a
line of text exactly as the font draws it on the host.

usage: check_polish.py <shot.png> <font.cvt> <size> <text (UTF-8)>...

Each text is drawn by tools/hifont.py (the font's glyphs side by side, as
render.s puts plain glyphs) and looked for on the screen, every pixel of
its box ink where the drawing's is (ink: a dark pixel); prints ok or
FAIL lines, exits 1 when one is not found.

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hifont  # noqa: E402


def main():
    a = sys.argv[1:]
    if len(a) < 4:
        sys.exit(__doc__)
    from PIL import Image
    img = Image.open(a[0]).convert('L')
    w, h = img.size
    px = img.load()
    scr = [''.join('#' if px[x, y] < 128 else '.' for x in range(w)) for y in range(h)]
    bad = 0
    for text in a[3:]:
        rows = hifont.render(a[1], int(a[2]), text)
        # (trailing columns of the last glyph's gap and empty rows cut)
        k = next(i for i, r in enumerate(rows) if '#' in r)
        found = None
        for y in range(h - len(rows) + 1):
            x = scr[y + k].find(rows[k])
            while x >= 0 and not found:
                if all(scr[y + i][x:x + len(rows[i])] == rows[i] for i in range(len(rows))):
                    found = (x, y)
                x = scr[y + k].find(rows[k], x + 1)
            if found:
                break
        if found:
            print('ok    "%s" in %s %s drawn as on the host, at %d, %d (%s)' % (text, os.path.basename(a[1]), a[2],
                                                                            found[0], found[1], os.path.basename(a[0])))
        else:
            print('FAIL  "%s" (%s %s) not found in %s' % (text, os.path.basename(a[1]), a[2], a[0]))
            bad = 1
    sys.exit(bad)


if __name__ == '__main__':
    main()
