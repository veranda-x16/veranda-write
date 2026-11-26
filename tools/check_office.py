#!/usr/bin/env python3
"""Veranda Write's tests: .docx and .odt documents opened (stage 4,
W49-W54) against what they must give (tools/office_fix.py's
expect.json), and the ones Veranda Write exported (tools/check_docx.py).

usage: check_office.py <test_cw> <folder>

Each fixture converted by test_cw ox (the same code as the X16's
importer: src/ozip.c, oxml.c, oemit.c, odocx.c, oodt.c) into a .cwr,
read with tools/cwrd.py: its paragraphs (their kinds, alignment and
text, a page break's own), runs' styles, the page setup and the counts
the note says. With LibreOffice (soffice), the fixtures also go through
it (a .docx made .odt, an .odt made .docx: the XML as it writes it),
and those, converted, must have the same text, headings, lists and page
breaks.

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import json
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cwrd  # noqa: E402

K_PB, K_PS, K_CS = 0x12, 0x11, 0x10


def paragraphs(d):
    """[(kind, align, left, before, after, text, [(char, flags, size, font)], pb)]"""
    out, t, i, s = [], d.text, 0, 0
    cur = {'ps': d.pstyles[0], 'text': [], 'runs': [], 'pb': 0}
    cs = 'iso8859_16' if d.charset == 1 else 'latin-1'
    while i <= len(t):
        if i == len(t) or t[i] == cwrd.CR:
            out.append(cur)
            cur = {'ps': d.pstyles[0], 'text': [], 'runs': [], 'pb': 0}
            i += 1
            continue
        c = t[i]
        if cwrd.K_FIRST <= c <= cwrd.K_LAST:
            if c == K_PS:
                cur['ps'] = d.pstyles[t[i + 1]]
            elif c == K_CS:
                s = t[i + 1]
            elif c == K_PB:
                cur['pb'] = 1
            i += 3
            continue
        ch = bytes([c]).decode(cs)
        f, z, g, _ = d.styles[s]
        cur['text'].append(ch)
        cur['runs'].append((ch, g, z, d.fonts[f]))
        i += 1
    return out


def flags(s):
    return (1 if 'b' in s else 0) | (2 if 'i' in s else 0) | (4 if 'u' in s else 0)


def check_one(name, cwr, want, log, bad):
    d = cwrd.read(cwr)
    ps = paragraphs(d)
    if 'chars' in want:
        n = sum(len(p['text']) for p in ps) + len(ps) - 1
        if n != want['chars']:
            bad.append('%s: %d characters, want %d' % (name, n, want['chars']))
    if 'paras_n' in want and len(ps) != want['paras_n'] + sum(p['pb'] for p in ps):
        bad.append('%s: %d paragraphs, want %d (and the page breaks)' % (name, len(ps), want['paras_n']))
    if 'paras' in want:
        wp = want['paras']
        if len(wp) != len(ps):
            bad.append('%s: %d paragraphs, want %d: %s' % (name, len(ps), len(wp),
                                                            [''.join(p['text'])[:20] for p in ps]))
        for k, (w, g) in enumerate(zip(wp, ps)):
            txt = ''.join(g['text'])
            if w.get('pb'):
                if not g['pb'] or txt:
                    bad.append('%s: paragraph %d not a page break: %r' % (name, k, txt))
                continue
            if g['pb']:
                bad.append('%s: paragraph %d a page break, want %r' % (name, k, w['text']))
                continue
            if txt != w['text']:
                bad.append('%s: paragraph %d %r, want %r' % (name, k, txt, w['text']))
            kind, align, left, right, first, before, after, sp = g['ps']
            if kind != w['kind']:
                bad.append('%s: paragraph %d (%s) kind %d, want %d' % (name, k, txt[:20], kind, w['kind']))
            if align != w.get('align', 0):
                bad.append('%s: paragraph %d (%s) aligned %d, want %d' % (name, k, txt[:20], align, w.get('align', 0)))
            for key, v in (('left', left), ('after', after), ('before', before)):
                if key in w and v != w[key]:
                    bad.append('%s: paragraph %d (%s) %s %d, want %d' % (name, k, txt[:20], key, v, w[key]))
            for word, fl, size in w.get('runs', []):
                at = txt.find(word)
                if at < 0:
                    bad.append('%s: paragraph %d: no %r' % (name, k, word))
                    continue
                for c, g2, z, font in g['runs'][at:at + len(word)]:
                    mono = font == 'Gate Mono'
                    if (g2 & 7) != flags(fl) or z != size or mono != ('mono' in fl):
                        bad.append('%s: %r in %s %d pt (flags %d), want %s %d pt' % (name, word, font, z, g2, fl, size))
                        break
    if 'page' in want:
        for k, v in want['page'].items():
            got = d.page.get(k) if d.page else None
            if isinstance(v, int) and isinstance(got, int) and abs(got - v) <= 2:
                continue
            if got != v:
                bad.append('%s: page %s %r, want %r' % (name, k, got, v))
    if 'tcodes' in want:
        # (1.6) the table's codes: from its first paragraph, in order
        tc = d.tables()
        first = tc[0][0] if tc else 0
        got_t = [[n - first, k, a] for n, k, a in tc]
        if got_t != want['tcodes']:
            bad.append('%s: the table\'s codes %r, want %r' % (name, got_t, want['tcodes']))
    if 'pbreaks' in want and d.pbreaks() != want['pbreaks']:
        bad.append('%s: %d page breaks, want %d' % (name, d.pbreaks(), want['pbreaks']))
    m = re.search(r'lost (\d+), tables (\d+), pictures (\d+), boxes (\d+), notes (\d+), comments (\d+), '
                  r'changes (\d+), fields (\d+)', log)
    got = dict(zip(('lost', 'tables', 'pics', 'boxes', 'notes', 'comments', 'changes', 'fields'),
                   map(int, m.groups()))) if m else {}
    for k, v in want.get('counts', {}).items():
        if got.get(k) != v:
            bad.append('%s: %s %r, want %r' % (name, k, got.get(k), v))
    return d, ps


def main():
    exe, folder = sys.argv[1], sys.argv[2]
    exp = json.load(open(os.path.join(folder, 'expect.json')))
    bad = []
    for name, want in exp.items():
        src = os.path.join(folder, name)
        out = src + '.cwr'
        r = subprocess.run([exe, 'ox', src, out], capture_output=True, text=True)
        if r.returncode:
            bad.append('%s: not converted: %s' % (name, r.stdout.strip()))
            continue
        d, ps = check_one(name, out, want, r.stdout, bad)
        print('office %s: %d paragraphs, %d characters, %d page breaks; %s'
              % (name, len(ps), sum(len(p['text']) for p in ps), d.pbreaks(), r.stdout.split(';', 1)[-1].strip()))
    # LibreOffice's own XML: the fixtures through it
    so = shutil.which('soffice')
    if so and not os.environ.get('NO_SOFFICE'):
        lo = os.path.join(folder, 'lo')
        os.makedirs(lo, exist_ok=True)
        jobs = (('features.docx', 'odt'), ('features.odt', 'docx'), ('polish.docx', 'odt'))
        for name, to in jobs:
            subprocess.run([so, '--headless', '-env:UserInstallation=file://' + os.path.abspath(lo) + '/profile',
                            '--convert-to', to, '--outdir', lo, os.path.join(folder, name)],
                           capture_output=True, timeout=180)
        for name, to in jobs:
            src = os.path.join(lo, os.path.splitext(name)[0] + '.' + to)
            if not os.path.exists(src):
                bad.append('LibreOffice did not make %s' % src)
                continue
            out = src + '.cwr'
            r = subprocess.run([exe, 'ox', src, out], capture_output=True, text=True)
            if r.returncode:
                bad.append('%s (LibreOffice\'s): not converted: %s' % (src, r.stdout.strip()))
                continue
            # LibreOffice keeps the text, the headings, lists and breaks (in
            # their order; its own spacing, sizes, and what it adds)
            b0 = len(bad)
            d = cwrd.read(out)
            got = [p for p in paragraphs(d) if not p['pb']]
            k = 0
            for w in exp[name].get('paras', []):
                if w.get('pb') or w.get('lo', 1) == 0:
                    continue
                while k < len(got) and ''.join(got[k]['text']) != w['text']:
                    k += 1
                if k == len(got):
                    bad.append('%s: no paragraph %r (in its order)' % (os.path.basename(src), w['text']))
                    break
                kind = got[k]['ps'][0]
                if kind != w['kind'] and (kind or w['kind'] != 4):
                    bad.append('%s: %r kind %d, want %d' % (os.path.basename(src), w['text'][:20], kind, w['kind']))
                k += 1
            if d.pbreaks() < exp[name].get('pbreaks', 0):
                bad.append('%s: %d page breaks, want %d' % (os.path.basename(src), d.pbreaks(), exp[name]['pbreaks']))
            # (1.6) a table through LibreOffice: its rows and cells (their
            # widths LibreOffice's)
            if 'tcodes' in exp[name]:
                w = exp[name]['tcodes']
                want_rc = (sum(1 for x in w if x[1] == 0x14), sum(1 for x in w if x[1] == 0x15))
                got_rc = cwrd.tab_sum(d.tables())[:2]
                if got_rc != want_rc:
                    bad.append('%s: the table %d rows, %d cells; want %d, %d' % ((os.path.basename(src),) + got_rc + want_rc))
            print('office %s (LibreOffice %s): %s' % (os.path.basename(src), 'ok' if len(bad) == b0 else 'FAIL',
                                                       r.stdout.split(';', 1)[-1].strip()))
    for b in bad:
        print('FAIL office: ' + b)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
