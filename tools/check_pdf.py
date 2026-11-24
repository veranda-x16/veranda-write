#!/usr/bin/env python3
"""Veranda Write's PDF files checked on the PC (DECISIONS W47): their
structure with qpdf (--check), their pages and paper with pdfinfo, their
text (selectable, searchable: Polish too) with pdftotext; the tools of
poppler and qpdf, each check skipped (said) when its tool is missing.
The structure is also checked here without them: the header, the
cross-reference table (every object where it says, 20 bytes an entry),
startxref, the trailer, each stream's /Length.

usage: check_pdf.py <folder>              the unit tests' files (make test:
                                          build/host/pdf.txt names them)
       check_pdf.py <file.pdf> [--pages N] [--size WxH] [--text T]...
                    [--page K T]... [--not-page K T]...

--text: somewhere in it; --page K T: on page K; --not-page K T: not on
page K (a header left off the first page). T is UTF-8.

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import os
import re
import shutil
import subprocess
import sys


def own(data):
    """the file's own structure: a list of what is wrong"""
    bad = []
    if not data.startswith(b'%PDF-1.4\n'):
        bad.append('no %PDF-1.4 header')
    m = re.search(rb'startxref\n(\d+)\n%%EOF\n$', data)
    if not m:
        return bad + ['no startxref at the end']
    x = int(m.group(1))
    if data[x:x + 5] != b'xref\n':
        return bad + ['startxref %d is not the table' % x]
    m = re.match(rb'xref\n0 (\d+)\n', data[x:])
    n = int(m.group(1))
    p = x + m.end()
    offs = {}
    for i in range(n):
        e = data[p + 20 * i:p + 20 * i + 20]
        if len(e) != 20 or e[-2:] != b' \n' or not re.match(rb'\d{10} \d{5} [nf]', e):
            bad.append('xref entry %d: %r' % (i, e))
            break
        if i:
            offs[i] = int(e[:10])
    for i, o in offs.items():
        if not data[o:].startswith(b'%d 0 obj' % i):
            bad.append('object %d is not at %d' % (i, o))
            if len(bad) > 5:
                break
    t = data[p + 20 * n:]
    if not re.match(rb'trailer\n<< /Size %d /Root 1 0 R /Info 13 0 R >>' % n, t):
        bad.append('the trailer: %r' % t[:60])
    # each stream's length: an indirect object, the bytes to endstream
    for m in re.finditer(rb'(\d+) 0 obj\n<< /Length (\d+) 0 R >>\nstream\n', data):
        k = int(m.group(2))
        lo = offs.get(k)
        if lo is None:
            bad.append('stream %s: no length object' % m.group(1).decode())
            continue
        ln = int(re.match(rb'\d+ 0 obj\n(\d+)', data[lo:]).group(1))
        s = m.end()
        if data[s + ln:s + ln + 11] != b'\nendstream\n':
            bad.append('stream %s: /Length %d is not its' % (m.group(1).decode(), ln))
            if len(bad) > 5:
                break
    return bad


def check(path, pages=None, size=None, texts=(), on=(), off=()):
    """path's checks; (failures, a line said)"""
    bad = []
    data = open(path, 'rb').read()
    bad += ['%s: %s' % (path, b) for b in own(data)]
    said = []
    if shutil.which('qpdf'):
        r = subprocess.run(['qpdf', '--check', path], capture_output=True, text=True)
        if r.returncode != 0 or 'No syntax or stream encoding errors' not in r.stdout:
            bad.append('%s: qpdf --check: %s' % (path, (r.stdout + r.stderr).strip()[-300:]))
        else:
            said.append('qpdf --check ok')
    else:
        said.append('(no qpdf)')
    if shutil.which('pdfinfo'):
        r = subprocess.run(['pdfinfo', path], capture_output=True, text=True)
        info = dict(re.findall(r'^([^:]+):\s+(.*)$', r.stdout, re.M))
        if r.returncode != 0:
            bad.append('%s: pdfinfo: %s' % (path, r.stderr.strip()))
        n = int(info.get('Pages', '0'))
        if pages is not None and n != pages:
            bad.append('%s: %d pages, want %d' % (path, n, pages))
        if size:
            w, h = [float(v) for v in re.match(r'([\d.]+) x ([\d.]+)', info.get('Page size', '0 x 0')).groups()]
            if abs(w - size[0]) > 0.6 or abs(h - size[1]) > 0.6:
                bad.append('%s: page size %s, want %gx%g' % (path, info.get('Page size'), size[0], size[1]))
        said.append('%d pages, %s' % (n, info.get('Page size', '?')))
    else:
        said.append('(no pdfinfo)')
    if shutil.which('pdftotext'):
        def text(k=None):
            a = ['pdftotext', '-enc', 'UTF-8'] + (['-f', str(k), '-l', str(k)] if k else []) + [path, '-']
            return subprocess.run(a, capture_output=True).stdout.decode('utf-8', 'replace')
        all_t = text()
        one = re.sub(r'\s+', ' ', all_t)
        for t in texts:
            if t not in all_t and t not in one:
                bad.append('%s: no "%s" in its text' % (path, t))
        for k, t in on:
            pt = text(k)
            if t not in pt and t not in re.sub(r'\s+', ' ', pt):
                bad.append('%s: no "%s" on page %d' % (path, t, k))
        for k, t in off:
            if t in text(k):
                bad.append('%s: "%s" on page %d' % (path, t, k))
        said.append('the text: %d characters' % len(all_t))
    else:
        said.append('(no pdftotext)')
    return bad, '%s: %s' % (os.path.basename(path), ', '.join(said))


def host(d):
    """the unit tests' files: pdf.txt has a line for each (name, pages)"""
    want = {}
    lst = os.path.join(d, 'pdf.txt')
    if not os.path.exists(lst):
        return ['no %s (make test writes it)' % lst], []
    for line in open(lst):
        f = line.split()
        if len(f) == 2:
            want[f[0]] = int(f[1])
    pl = 'Zażółć gęślą jaźń'
    spec = {
        'styles': dict(size=(595.28, 841.89), texts=[pl, '(paren) back\\slash', 'underlined'],
                       on=[(2, 'styles'), (2, 'Zażółć'), (2, '23 November 2026'), (2, 'Page 2 of %d')],
                       off=[(1, '23 November 2026'), (1, 'Page 1 of')]),
        'letter': dict(size=(612, 792), texts=[pl]),
        'empty': dict(size=(595.28, 841.89)),
        'big': dict(texts=['ł'], on=[(5, 'Page 5'), (5, '5/%d')]),
    }
    bad, said = [], []
    for name, n in want.items():
        sp = dict(spec.get(name, {}))
        sp['on'] = [(k, t % n if '%d' in t else t) for k, t in sp.get('on', [])]
        b, s = check(os.path.join(d, name + '.pdf'), pages=n, **sp)
        bad += b
        said.append(s)
    return bad, said


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    if os.path.isdir(a[0]):
        bad, said = host(a[0])
    else:
        kw = {'texts': [], 'on': [], 'off': []}
        i = 1
        while i < len(a):
            if a[i] == '--pages':
                kw['pages'] = int(a[i + 1])
                i += 2
            elif a[i] == '--size':
                kw['size'] = tuple(float(v) for v in a[i + 1].split('x'))
                i += 2
            elif a[i] == '--text':
                kw['texts'].append(a[i + 1])
                i += 2
            elif a[i] in ('--page', '--not-page'):
                kw['on' if a[i] == '--page' else 'off'].append((int(a[i + 1]), a[i + 2]))
                i += 3
            else:
                sys.exit('check_pdf.py: %s?' % a[i])
        bad, s = check(a[0], **kw)
        said = [s]
    for s in said:
        print('pdf ' + s)
    for b in bad:
        print('FAIL pdf ' + b)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
