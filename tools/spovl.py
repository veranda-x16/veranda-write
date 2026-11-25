#!/usr/bin/env python3
"""Veranda Write's spelling overlays as one file, SPELL.OVL (W59): the
overlays the link made (cw.bin.23 .. .26) are not records of the
program's file (Veranda would load them into banks at the start for
good); v/spload.c reads them from /VERANDA/DICT/SPELL.OVL when spelling
is first used.

usage: spovl.py <build folder> <overlay numbers...>

The file: "WSPO", the build's mark (4 bytes: a CRC-32 of the main
program and the overlays), then each overlay: its length (2 bytes) and
its bytes. The same mark is written into overlay 22's sp_want (its
address from labels.txt), so a SPELL.OVL of another build is refused.
(W60) Overlay 19's ovl_bss (v/ovlbss.s) is filled in too: for overlays
1-22 and 27, the address of the bank's variable (_bankN, _bankt), the
overlay's BSS address and size (0 when it has none).

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import os
import struct
import sys
import zlib


def main(d, ovls):
    labels = {}
    for line in open(os.path.join(d, 'labels.txt')):
        f = line.split()
        if len(f) == 3:
            labels[f[2].lstrip('.')] = int(f[1], 16)
    main_bin = open(os.path.join(d, 'cw.bin'), 'rb').read()
    parts = [open(os.path.join(d, 'cw.bin.%s' % n), 'rb').read() for n in ovls]
    mark = zlib.crc32(main_bin + b''.join(parts)) & 0xFFFFFFFF
    out = bytearray(b'WSPO' + struct.pack('<I', mark))
    for p in parts:
        if len(p) > 8191:
            sys.exit('spovl.py: an overlay of %d bytes' % len(p))
        out += struct.pack('<H', len(p)) + p
    open(os.path.join(d, 'SPELL.OVL'), 'wb').write(out)
    at = labels['_sp_want'] - 0xA000
    p22 = os.path.join(d, 'cw.bin.22')
    b = bytearray(open(p22, 'rb').read())
    if b[at:at + 4] != b'\0\0\0\0':
        sys.exit('spovl.py: overlay 22\'s sp_want is not where the labels say')
    b[at:at + 4] = struct.pack('<I', mark)
    open(p22, 'wb').write(b)
    p19 = os.path.join(d, 'cw.bin.19')
    b = bytearray(open(p19, 'rb').read())
    at = labels['_ovl_bss'] - 0xA000
    if b[at:at + 23 * 6] != bytes(23 * 6):
        sys.exit('spovl.py: overlay 19\'s ovl_bss is not where the labels say')
    for k, n in enumerate(list(range(1, 23)) + [27]):
        bank = labels['_bankt' if n == 27 else '_bank%d' % n]
        run = labels.get('__OVL%dBSS_RUN__' % n, 0)
        size = labels.get('__OVL%dBSS_SIZE__' % n, 0) if run else 0
        b[at + 6 * k:at + 6 * k + 6] = struct.pack('<HHH', bank, run, size)
    open(p19, 'wb').write(b)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2:])
