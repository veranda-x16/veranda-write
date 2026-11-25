#!/usr/bin/env python3
"""Veranda Write's link configuration: the SDK's cfg/app-c-ovl.cfg (24
overlays) with more overlays, as many as asked (Veranda's loader takes
126: its DECISIONS V18), made the same way: overlay n at $A000 into
"%O.n" from the segments OVERLAYn, OVLnRO, OVLnDATA, OVLnBSS.

usage: ovlcfg.py <the SDK's app-c-ovl.cfg> <overlays> <out>

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import re
import sys


def main(src, n, out):
    s = open(src).read()
    have = max(int(m) for m in re.findall(r'^\s*OVL(\d+):\s*file', s, re.M))
    mem = ''.join('\tOVL%d: file = "%%O.%d", define = yes, start = $A000, size = $2000;\n' % (k, k)
                  for k in range(have + 1, n + 1))
    seg = ''.join('\tOVERLAY%d: load = OVL%d, type = ro, define = yes, optional = yes;\n'
                  '\tOVL%dRO: load = OVL%d, type = ro, optional = yes;\n'
                  '\tOVL%dDATA: load = OVL%d, type = rw, optional = yes;\n'
                  '\tOVL%dBSS: load = OVL%d, type = bss, define = yes, optional = yes;\n'
                  % ((k,) * 8) for k in range(have + 1, n + 1))
    s = re.sub(r'(\n\tOVL%d: file[^\n]*\n)' % have, lambda m: m.group(1) + mem, s, count=1)
    s = re.sub(r'(\n\tOVL%dBSS: load[^\n]*\n)' % have, lambda m: m.group(1) + seg, s, count=1)
    open(out, 'w').write(s)


if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]), sys.argv[3])
