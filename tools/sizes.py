#!/usr/bin/env python3
"""Veranda Write's sizes after a link: main RAM used (from the map: the
end of BSS against the C stack at $8400) and each overlay's bytes (at
most 8191).

    sizes.py <cw.map> <cw.bin> <overlay numbers...>

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import os
import re
import sys


def main():
    mp, binp, ovl = sys.argv[1], sys.argv[2], sys.argv[3:]
    text = open(mp).read()
    m = re.search(r'^BSS\s+([0-9A-F]{6})\s+([0-9A-F]{6})', text, re.M)
    end = int(m.group(2), 16) + 1 if m else 0
    top = 0x8800 - 0x400
    print('main RAM: $0801-$%04X (%d bytes), %d bytes free below the C stack ($%04X)'
          % (end - 1, end - 0x801, top - end, top))
    bad = end > top
    for n in ovl:
        s = os.path.getsize('%s.%s' % (binp, n))
        print('overlay %s: %d bytes (%d free)' % (n, s, 8191 - s))
        bad |= s > 8191
        if n == '17' and s > 3072:
            print('overlay 17: over 3 KB: the session goes at $AC00 in its bank (src/v/test.c)')
            bad = True
    if bad:
        sys.exit('sizes: too big')


main()
