#!/usr/bin/env python3
"""(W69) A dialog box drawn opaque: its rectangle (and shadow) in a
screenshot taken with the page's text under it must be as in one taken
over blank paper, and its frame drawn.

usage: check_box.py <sdk> <dump over text> <dump over paper> left top right bottom

The rectangle is the box's (src/v/aiui.c's wbox: 96, 176, 543, 271); the
shadow, 8 pixels right and down, is compared too. The dumps are x16emu's
(-dump V), composed as VERA shows them (Veranda's vram2png.compose).

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import os
import sys


def main():
    a = sys.argv[1:]
    if len(a) != 7:
        sys.exit(__doc__)
    sys.path.insert(0, os.path.join(a[0], 'tools'))
    import vram2png
    l, t, r, b = (int(x) for x in a[3:])
    over = vram2png.compose(vram2png.section(a[1]))
    blank = vram2png.compose(vram2png.section(a[2]))
    bad = []
    diff = [(x, y) for y in range(t, b + 9) for x in range(l, r + 9)
            if not (x > r and y < t + 8) and not (y > b and x < l + 8)     # (the box or its shadow)
            and over[y][x] != blank[y][x]]
    if diff:
        ys = sorted({y for _, y in diff})
        bad.append('%d pixels of the box not as over blank paper (y %d..%d): the page shows through'
                   % (len(diff), ys[0], ys[-1]))
    paper = over[t + 1][l + 1]
    for name, pts in (('left', [(l, y) for y in range(t, b + 1)]), ('right', [(r, y) for y in range(t, b + 1)]),
                      ('top', [(x, t) for x in range(l, r + 1)]), ('bottom', [(x, b) for x in range(l, r + 1)])):
        if any(over[y][x] == paper for x, y in pts):
            bad.append('the box\'s %s edge not drawn' % name)
    for m in bad:
        print('check_box: ' + m)
    if not bad:
        print('check_box: the box opaque, its frame drawn (%d x %d)' % (r - l + 1, b - t + 1))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
