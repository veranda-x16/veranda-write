#!/usr/bin/env python3
"""(W71) A flat profile of a run under build/x16prof's emulator (X16PROF:
the cycles at each address, banked RAM by bank; its .banks: each bank's
bytes when code first ran there): by module and function (labels.txt,
cw.map, and the listings in <build>/lst/*.lst when there are any: cl65
-l), each bank's overlay found by its bytes.

usage: prof.py <profile> <build folder> [--top N] [--fn]"""
import bisect, collections, os, re, struct, sys

def libranges(bdir):
    """the C library's, Veranda's library's and glue.s's code, Veranda's
    kernel ($8800-$9EFF) and the ROM: their cycles their callers'"""
    mp = open(os.path.join(bdir, 'cw.map'), errors='replace').read()
    segstart = {m.group(1): int(m.group(2), 16)
                for m in re.finditer(r'^(\w+)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+([0-9A-F]{6})', mp, re.M)}
    r = []
    cur = None
    for l in mp.split('Segment list:')[0].splitlines():
        m = re.match(r'^(\S.*):$', l)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r'^\s+(CODE|ONCE|STARTUP)\s+Offs=([0-9A-F]+)\s+Size=([0-9A-F]+)', l)
        if m and cur and ('.lib(' in cur or cur.startswith('glue')):
            s0 = segstart[m.group(1)] + int(m.group(2), 16)
            r.append('%x-%x' % (s0, s0 + int(m.group(3), 16)))
    r.append('8400-9f00')
    r.append('c000-10000')
    return ','.join(r)

def main():
    a = sys.argv[1:]
    if a and a[0] == '--libranges':
        print(libranges(a[1]))
        return
    prof, bdir = a[0], a[1]
    top = int(a[a.index('--top') + 1]) if '--top' in a else 50
    raw = open(prof, 'rb').read()
    h = struct.unpack('<%dI' % (len(raw) // 4), raw)
    snap = open(prof + '.banks', 'rb').read()
    ovl = {}
    for f in os.listdir(bdir):
        m = re.fullmatch(r'cw\.bin\.(\d+)', f)
        if m:
            ovl[int(m.group(1))] = open(os.path.join(bdir, f), 'rb').read()
    bank_ovl = {}
    for b in range(256):
        bs = snap[b * 8192:(b + 1) * 8192]
        if not any(bs[:64]):
            continue
        best = None
        for k, o in ovl.items():
            n = min(len(o), 8192, 2048)
            if n and bs[:n] == o[:n]:
                best = k
        if best is not None:
            bank_ovl[b] = best
    mp = open(os.path.join(bdir, 'cw.map'), errors='replace').read()
    segstart = {m.group(1): int(m.group(2), 16)
                for m in re.finditer(r'^(\w+)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+([0-9A-F]{6})', mp, re.M)}
    # modules: name -> [(segment, offs, size)]
    mods = []
    cur = None
    for l in mp.split('Segment list:')[0].splitlines():
        m = re.match(r'^(\S.*):$', l)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r'^\s+(\w+)\s+Offs=([0-9A-F]+)\s+Size=([0-9A-F]+)', l)
        if m and cur and m.group(1) in segstart:
            s0 = segstart[m.group(1)] + int(m.group(2), 16)
            mods.append((m.group(1), s0, s0 + int(m.group(3), 16), cur))
    def ovl_of_seg(seg):
        m = re.match(r'(?:OVERLAY|OVL)(\d+)', seg)
        return int(m.group(1)) if m else 0
    ranges = collections.defaultdict(list)   # overlay (0 main) -> [(lo, hi, module)]
    for seg, lo, hi, mod in mods:
        if hi > lo:
            ranges[ovl_of_seg(seg)].append((lo, hi, mod, seg))
    for k in ranges:
        ranges[k].sort()
    # functions: labels.txt (exports) and the listings' labels
    labs = collections.defaultdict(list)
    for l in open(os.path.join(bdir, 'labels.txt')):
        p = l.split()
        if len(p) >= 3 and p[0] == 'al':
            labs[None].append((int(p[1], 16), p[2].lstrip('.')))
    lst = collections.defaultdict(list)      # module -> [(seg, offs, name)]
    ld = os.path.join(bdir, 'lst')
    if os.path.isdir(ld):
        for f in os.listdir(ld):
            mod = f.rsplit('.', 1)[0] + '.o'
            seg = 'CODE'
            for l in open(os.path.join(ld, f), errors='replace'):
                m = re.search(r'\.segment\s+"(\w+)"', l)
                if m:
                    seg = m.group(1)
                m = re.match(r'^([0-9A-F]{6})r\s+\d+\s+.*\.proc\s+(\w+)', l) or \
                    re.match(r'^([0-9A-F]{6})r\s+\d+\s+(?:[0-9A-F]{2}\s+)*\s*(@?\w+):', l)
                if m and not re.match(r'L[0-9A-F]{4}$', m.group(2)):
                    lst[mod].append((seg, int(m.group(1), 16), m.group(2)))
    def where(k, pc):
        for lo, hi, mod, seg in ranges.get(k, []):
            if lo <= pc < hi:
                fn = ''
                best = -1
                for s2, off, name in lst.get(mod, []):
                    if s2 == seg and lo + off <= pc and lo + off > best:
                        best = lo + off
                        fn = name
                if not fn:
                    cand = [(ad, n) for ad, n in labs[None] if lo <= ad <= pc]
                    if cand:
                        fn = max(cand)[1]
                return mod, fn
        return '?', ''
    by_mod = collections.Counter()
    by_fn = collections.Counter()
    allc = sum(h)
    for ix, c in enumerate(h):
        if not c:
            continue
        if ix < 0xA000:
            k, pc = 0, ix
            if pc < 0x0801 or pc >= 0x9F00:
                mod, fn = ('zp/stack' if pc < 0x0801 else 'io'), ''
            else:
                mod, fn = where(0, pc)
                if mod == '?':
                    mod = 'main ?%04X' % (pc & 0xFF00)
        elif ix < 0xA000 + 256 * 0x2000:
            b = (ix - 0xA000) // 0x2000
            pc = 0xA000 + (ix - 0xA000) % 0x2000
            k = bank_ovl.get(b)
            if k is None:
                mod, fn = 'bank %02X ?%04X' % (b, pc & 0xFF00), ''
            else:
                mod, fn = where(k, pc)
                mod = 'ovl%d %s' % (k, mod)
        else:
            r = (ix - 0xA000 - 256 * 0x2000) // 0x4000
            mod, fn = 'rom%d' % r, ''
        by_mod[mod] += c
        by_fn[(mod, fn)] += c
    if '--lines' in a:
        # the listing's lines of a module (overlay k's or main RAM's) by cycles
        want = a[a.index('--lines') + 1]
        lines = collections.Counter()
        for (mod, fn), c in by_fn.items():
            pass
        for ix, c in enumerate(h):
            if not c or ix >= 0xA000 + 256 * 0x2000:
                continue
            if ix < 0xA000:
                k, pc = 0, ix
            else:
                b = (ix - 0xA000) // 0x2000
                pc = 0xA000 + (ix - 0xA000) % 0x2000
                k = bank_ovl.get(b)
            for lo, hi, mod, seg in ranges.get(k, []):
                if lo <= pc < hi and mod == want:
                    lines[(seg, pc - lo)] += c
        src = {}
        f = os.path.join(bdir, 'lst', want.rsplit('.', 1)[0] + '.lst')
        seg = 'CODE'
        for l in open(f, errors='replace'):
            m = re.search(r'\.segment\s+"(\w+)"', l)
            if m:
                seg = m.group(1)
            m = re.match(r'^([0-9A-F]{6})r\s+\d+\s+((?:[0-9A-F]{2}\s)+)\s*(.*)', l)
            if m:
                src[(seg, int(m.group(1), 16))] = m.group(3).strip()
        for (seg, off), c in sorted(lines.items(), key=lambda x: -x[1])[:top]:
            print('%8.3f s  %s+%04X  %s' % (c / 8e6, seg, off, src.get((seg, off), '?')))
        return
    print('total %.3f s; banks: %s' % (allc / 8e6, ' '.join('%02X=%d' % kv for kv in sorted(bank_ovl.items()))))
    for k, c in by_mod.most_common(top):
        print('%8.3f s %5.1f%%  %s' % (c / 8e6, 100 * c / allc, k))
    if '--fn' in a:
        print('-- functions')
        for (m, f), c in by_fn.most_common(top * 2):
            print('%8.3f s %5.1f%%  %s %s' % (c / 8e6, 100 * c / allc, m, f))

main()
