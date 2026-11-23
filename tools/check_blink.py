#!/usr/bin/env python3
"""Veranda Write's gate: the blink probe's lines (DECISIONS W42; as
Veranda's tests/check_blink.py, V160): the text cursor blinks on the
clock, whatever the main loop's speed.

usage: check_blink.py <console.txt> <shown> <hidden> [modes]

The player's 'B' m n record (tools/play.s) logs "BLINK m s frames"
at each change of the cursor (s the new state, frames since the probe
began, hex), the main loop slowed by about m * 1,300 cycles a pass (0:
as fast as it runs; 200: about 2 frames a pass). Between two changes the
cursor was shown for `shown` frames (a change to 0) or hidden for
`hidden` (a change to 1): exactly when not slowed; slowed, as many frames
more as a pass takes, and one (the toggle comes at a pass's start).
modes: the m values that must be there (default 0, 40, 200), each with 5
intervals at least.

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).
"""
import sys


def check(con, shown, hidden, modes=(0, 40, 200)):
    runs = {}
    for line in con.split('\n'):
        i = line.find('BLINK ')
        if i < 0:
            continue
        p = line[i + 6:].split()
        if len(p) != 3:
            continue
        m, s, f = (int(x, 16) for x in p)
        runs.setdefault(m, []).append((s, f))
    bad, said = [], []
    for m in modes:
        r = runs.get(m, [])
        if len(r) < 6:
            bad.append('blink (main loop slowed %d): %d changes logged, want 6 at least' % (m, len(r)))
            continue
        ds = []
        slack = 0 if m == 0 else 1 + -(-m * 1300 // 133000)   # a pass's frames, and one
        for (s0, f0), (s1, f1) in zip(r, r[1:]):
            want = shown if s1 == 0 else hidden
            d = f1 - f0
            ds.append(d)
            if s1 == s0 or not want <= d <= want + slack:
                bad.append('blink (main loop slowed %d): %s for %d frames, want %d'
                           % (m, 'shown' if s1 == 0 else 'hidden', d, want))
        said.append('slowed %d: %s' % (m, ' '.join(map(str, ds))))
    return bad, 'the cursor blinks on the clock (%d shown, %d hidden frames; %s)' % (shown, hidden, '; '.join(said))


def main():
    con = open(sys.argv[1], 'rb').read().decode('latin-1')
    modes = tuple(int(x) for x in sys.argv[4].split(',')) if len(sys.argv) > 4 else (0, 40, 200)
    bad, ok = check(con, int(sys.argv[2]), int(sys.argv[3]), modes)
    for b in bad:
        print('check_blink:', b)
    if bad:
        sys.exit(1)
    print('ok   ', ok)


if __name__ == '__main__':
    main()
