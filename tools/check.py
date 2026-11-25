#!/usr/bin/env python3
"""Veranda Write's gate: a session's results against expect.json.

usage: check.py <run folder> [--partial]

The console's "CW STATE" lines (src/v/test.c) against the states the
session must log (tools/script.py: the characters, their styles' sum,
modified, the document's name), the files it must leave (read with
tools/cwrd.py: well formed, the characters and styles, or the texts they
must contain), "CW ASIDE" / "CW DESK" / "CW BACK" (put aside and brought
back), no "HUNG" or "PANIC"; (stage 3, W44-W48) the pages each state
counted, the files' PAGE chunks and page breaks, the PDFs it wrote
(tools/check_pdf.py: their structure, pages, paper and text), the ones it
must not have left, the PDF's page that starts where Go to page went; the cursor's blink (W42, tools/check_blink.py:
32 frames shown, 20 hidden, the main loop fast and slowed); the timings (CW TIME, TIME) printed in
seconds at 8 MHz; (W55) each redraw check (the text area as the edits
drew it against a full drawing). --partial: a session stopped early (only what it got
to).

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import hashlib
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cwrd  # noqa: E402
import check_blink  # noqa: E402
import check_docx  # noqa: E402
import check_pdf  # noqa: E402

MHZ = 8e6


def lat(con, labels):
    """the timed keys ("CW LAT keys cycles": tools/play.s 'Q' and 'R'),
    from a key put in the ROM's queue to the program's drawing after it
    (the text and the cursor; "CW LATS": the status bar after them; "CW
    DRAW": a drawing of keys none of them timed), each group up to the
    state logged after it"""
    keys, ts, st, nd = 0, [], [], 0
    for m in re.finditer(r'CW LAT ([0-9A-F]{8}) ([0-9A-F]{8})|CW STATE ([0-9A-F]{8})|CW DRAW ([0-9A-F]{8})'
                         r'|CW LATS ([0-9A-F]{8})', con):
        if m.group(3):
            if ts:
                ts.sort()
                st.sort()
                n = int(m.group(3), 16)
                print('keys %-55s %3d keys, %3d drawings: %4.0f ms median, %4.0f min, %4.0f max (%.1f frames)%s'
                      % (labels.get(str(n), 'before state %d' % n) + ':', keys, nd, 1000 * ts[len(ts) // 2],
                         1000 * ts[0], 1000 * ts[-1], 60 * ts[len(ts) // 2],
                         '; the status bar %.0f ms' % (1000 * st[len(st) // 2]) if st else ''))
            keys, ts, st, nd = 0, [], [], 0
            continue
        if m.group(5):
            st.append(int(m.group(5), 16) / MHZ)
            continue
        nd += 1
        if m.group(4):
            keys += int(m.group(4), 16)
            continue
        keys += int(m.group(1), 16)
        ts.append(int(m.group(2), 16) / MHZ)


def main():
    a = sys.argv[1:]
    run = a[0]
    partial = '--partial' in a
    exp = json.load(open(os.path.join(run, 'expect.json')))
    con = open(os.path.join(run, 'console.txt'), 'rb').read().decode('latin-1').replace('\r', '\n')
    bad = []
    if 'HUNG' in con or 'PANIC' in con:
        bad.append('the program hung or crashed: ' + con[con.find('HUNG') if 'HUNG' in con else con.find('PANIC'):][:80])
    got = {}
    for m in re.finditer(r'CW STATE ((?:[0-9A-F]{8} ){13})(.*)', con):
        f = [int(x, 16) for x in m.group(1).split()]
        got[f[0]] = {'len': f[1], 'lines': f[2], 'cur': f[3], 'anc': f[4], 'modified': f[5], 'style': f[6],
                     'top': f[7], 'sum': f[8], 'chars': f[9], 'banks': f[10] >> 8, 'free': f[10] & 255,
                     'psum': f[11], 'pages': f[12], 'name': m.group(2).strip()}
    for n, want in exp['states']:
        g = got.get(n)
        if g is None:
            if not partial:
                bad.append('state %d: not logged' % n)
            continue
        for k, v in want.items():
            if k == 'sum_not':
                if g['sum'] == v:
                    bad.append('state %d: the sum did not change' % n)
            elif k == 'contains':
                pass
            elif k == 'cur_in':
                if g['cur'] > g['len'] or g['anc'] > g['len']:
                    bad.append('state %d: the cursor %d (%d) past the text\'s end %d' % (n, g['cur'], g['anc'], g['len']))
            elif g.get(k) != v:
                bad.append('state %d: %s %r, want %r' % (n, k, g.get(k), v))
        print('state %2d: %6d characters, %5d lines, %4d pages, sum %08X, paragraphs %08X, modified %d, banks %d (%d free) %s'
              % (n, g['chars'], g['lines'], g['pages'], g['sum'], g['psum'], g['modified'], g['banks'], g['free'],
                 g['name']))
    if not partial and exp.get('SESSION') != 'speed':
        for marker in ('CW ASIDE', 'CW DESK RESUME', 'CW BACK') if exp.get('SESSION') not in ('page', 'office', 'redraw') else ():
            if marker not in con:
                bad.append('no "%s" (Alt+Tab)' % marker)
        for name, want in exp['files'].items():
            p = os.path.join(run, name)
            if not os.path.exists(p):
                bad.append('no file %s' % name)
                continue
            if 'sha256' in want:
                if hashlib.sha256(open(p, 'rb').read()).hexdigest() != want['sha256']:
                    bad.append('%s: changed' % name)
                print('file %s: unchanged' % name)
                continue
            try:
                d = cwrd.read(p)
                ch = d.chars()
            except cwrd.FormatError as e:
                bad.append('%s: %s' % (name, e))
                continue
            text = ''.join(chr(c[0]) for c in ch)
            if 'sum' in want and (cwrd.doc_sum(ch) != want['sum'] or len(ch) != want['chars']):
                bad.append('%s: %d characters, sum %08X; want %d, %08X'
                           % (name, len(ch), cwrd.doc_sum(ch), want['chars'], want['sum']))
            if 'psum' in want and cwrd.par_sum(d.paras()) != want['psum']:
                bad.append('%s: paragraphs %08X; want %08X' % (name, cwrd.par_sum(d.paras()), want['psum']))
            for t in want.get('contains', []):
                if t not in text:
                    bad.append('%s: no "%s"' % (name, t))
            for t in want.get('absent', []):
                if t in text:
                    bad.append('%s: "%s" still there' % (name, t))
            if len(ch) < want.get('chars_min', 0):
                bad.append('%s: %d characters, at least %d' % (name, len(ch), want['chars_min']))
            print('file %s: %d characters, %d styles, version %d.%d, chunks %s'
                  % (name, len(ch), len(d.styles), d.version[0], d.version[1], ' '.join(d.chunks)))
    # stage 3: the files' page setups and page breaks, the PDFs
    for name, want in exp.get('pagefile', {}).items() if not partial else []:
        try:
            d = cwrd.read(os.path.join(run, name))
        except (OSError, cwrd.FormatError) as e:
            bad.append('%s: %s' % (name, e))
            continue
        if d.page != want['page']:
            bad.append('%s: PAGE %r, want %r' % (name, d.page, want['page']))
        if d.pbreaks() != want['pbreaks']:
            bad.append('%s: %d page breaks, want %d' % (name, d.pbreaks(), want['pbreaks']))
        print('file %s: PAGE %s, %d page breaks' % (name, d.page, d.pbreaks()))
    for want in exp.get('pdfs', []) if not partial else []:
        p = os.path.join(run, want['path'])
        if not os.path.exists(p):
            bad.append('no PDF %s' % want['path'])
            continue
        n = want.get('pages') or got.get(want.get('pages_state'), {}).get('pages')
        b, said = check_pdf.check(p, pages=n, size=want.get('size'), texts=want.get('texts', []),
                                  on=want.get('on', []), off=want.get('off', []))
        bad += b
        print('pdf %s (%d bytes)' % (said, os.path.getsize(p)))
    # stage 4: the exported .docx and .odt files: as other programs read
    # them (tools/check_docx.py), and read back on the host: the document
    # the program had (its state's sums), the page setup
    tc = os.environ.get('TEST_CW', 'build/host/test_cw')
    for want in exp.get('office', []) if not partial else []:
        p = os.path.join(run, want['path'])
        if not os.path.exists(p):
            bad.append('no %s' % want['path'])
            continue
        b, said = check_docx.check(p, tc)
        bad += b
        out = p + '.cwr'
        r = check_pdf.subprocess.run([tc, 'ox', p, out], capture_output=True, text=True)
        g = got.get(want['state'])
        if r.returncode:
            bad.append('%s: not read back: %s' % (want['path'], r.stdout.strip()))
        elif g:
            d = cwrd.read(out)
            ch = d.chars()
            if cwrd.doc_sum(ch) != g['sum'] or len(ch) != g['chars'] or cwrd.par_sum(d.paras()) != g['psum']:
                bad.append('%s read back: %d characters, sum %08X, paragraphs %08X; the program had %d, %08X, %08X'
                           % (want['path'], len(ch), cwrd.doc_sum(ch), cwrd.par_sum(d.paras()), g['chars'], g['sum'],
                              g['psum']))
            if want.get('page') and d.page != want['page']:
                bad.append('%s read back: PAGE %r, want %r' % (want['path'], d.page, want['page']))
        print('office ' + said + (' (read back: the same document)' if not b else ''))
    # the header's and footer's parts as drawn ("CW HF x w right text",
    # src/v/pgview.c): each inside the text's width; the date whole
    hf = [(int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16),
           m.group(4).rstrip().encode('latin-1').decode('iso8859_16'))
          for m in re.finditer(r'CW HF ([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4}) ([^\n]*)', con)]
    for x, w, r, t in hf:
        if x < 64 or x + w > r:
            bad.append('a header or footer part drawn at %d..%d, the text %d..%d: %r' % (x, x + w, 64, r, t))
    if exp.get('hfdate') and not partial:
        if not any(re.fullmatch(r'\d{1,2} [A-Z][a-z]+ \d{4}', t) for _, _, _, t in hf):
            bad.append('no header part with the whole date ("1 January 2000"): %r' % sorted({t for *_, t in hf}))
    if hf:
        print('header and footer: %d parts drawn, all inside the text (%s)' % (len(hf), ', '.join(sorted({t for *_, t in hf}))[:120]))
    # (W55) the partial redraw against a full one ("CW REDRAW n rows first
    # last", src/v/test.c): no row of the text area different
    rd = {int(m.group(1), 16): (int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16))
          for m in re.finditer(r'CW REDRAW ([0-9A-F]{8}) ([0-9A-F]{8}) ([0-9A-F]{8}) ([0-9A-F]{8})', con)}
    for k, (r, y0, y1) in sorted(rd.items()):
        if r:
            bad.append('redraw check %d: %d rows of the text area not as a full drawing draws them (y %d..%d)'
                       % (k, r, y0, y1))
    for k in exp.get('redraws', []) if not partial else []:
        if k not in rd:
            bad.append('redraw check %d: not logged' % k)
    if rd:
        print('redraw checks: %d, %d with rows not as a full drawing\'s' % (len(rd), sum(1 for v in rd.values() if v[0])))
    for a in exp.get('absent', []) if not partial else []:
        if os.path.exists(os.path.join(run, a)):
            bad.append('%s: still there' % a)
    for c in exp.get('console', []) if not partial else []:
        if c not in con:
            bad.append('no "%s" in the log' % c)
    ps = exp.get('pagestart')
    if ps and not partial and ps['state'] in got and os.path.exists(os.path.join(run, ps['pdf'])):
        # the text at the cursor (a byte of the document's text) is the PDF's page's first
        d = cwrd.read(os.path.join(run, ps['doc']))
        t, i, k = d.text, 0, got[ps['state']]['cur']
        out = []
        while i < len(t) and len(out) < 40:
            if cwrd.K_FIRST <= t[i] <= cwrd.K_LAST:
                i += 3
                continue
            if i >= k:
                out.append(chr(t[i]) if t[i] != cwrd.CR else ' ')
            i += 1
        want = ' '.join(''.join(out).split()[:4])
        r = check_pdf.subprocess.run(['pdftotext', '-f', str(ps['page']), '-l', str(ps['page']),
                                      os.path.join(run, ps['pdf']), '-'], capture_output=True)
        got_t = ' '.join(r.stdout.decode('utf-8', 'replace').split()[:4])
        if want != got_t:
            bad.append('Go to page %d: the cursor at "%s", the PDF\'s page starts "%s"' % (ps['page'], want, got_t))
        print('go to page %d: "%s" (the PDF\'s page: "%s")' % (ps['page'], want, got_t))
    for m in re.finditer(r'CW MARK (\S+) ([0-9A-F]{8})', con):
        print('mark %s: %.3f s' % (m.group(1), int(m.group(2), 16) / MHZ))
    for m in re.finditer(r'CW SWAP ([0-9A-F]{8}) ([0-9A-F]{8})', con):
        print('swap before a state: %d chunks read, %d written' % (int(m.group(1), 16), int(m.group(2), 16)))
    for m in re.finditer(r'CW ASIDE ([0-9A-F]{8})', con):
        print('put aside: %d banks free' % int(m.group(1), 16))
    for m in re.finditer(r'CW TIME (\S+) ([0-9A-F]{8})', con):
        print('time %s: %.3f s' % (m.group(1), int(m.group(2), 16) / MHZ))
    for m in re.finditer(r'\nTIME (\S) ([0-9A-F]{8})', con):
        big = 'SESSION' in exp
        print('time %s: %.3f s' % ({'l': 'open BIG.cwr (all of it)', 's': 'save BIG2.cwr',
                                     'f': 'find MIDDLEMARK from the start' if big else 'find in BIG.cwr (not found)',
                                     'r': 'replace all in BIG.cwr', 'e': 'Ctrl+End', 'h': 'Ctrl+Home',
                                     'g': 'go to page 60', 'm': 'open BIG2.cwr again',
                                     'p': 'export XBIG.cwr as PDF',
                                     'd': 'open Report.docx (converted)', 'o': 'open Notes.odt (converted)',
                                     'b': 'open Big report.docx (converted)', 'w': 'export Report as .docx',
                                     'v': 'export Report as .odt', 'x': 'export Big report as .docx',
                                     'y': 'export Big report as .odt'}.get(m.group(1), m.group(1)),
                                    int(m.group(2), 16) / MHZ))
    lat(con, exp.get('labels', {}))
    for n, want in exp.get('lossy', {}).items():
        g = got.get(int(n))
        if g:
            print('keys lost in state %s: %d (%d characters, %d typed)' % (n, want - g['chars'], g['chars'], want))
    if not partial and 'SESSION' not in exp:
        bb, blink = check_blink.check(con, 32, 20)
        bad += bb
        print(blink)
    for b in bad:
        print('FAIL ' + b)
    sys.exit(1 if bad else 0)


main()
