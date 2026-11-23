#!/usr/bin/env python3
"""Veranda Write's gate: a session's results against expect.json.

usage: check.py <run folder> [--partial]

The console's "CW STATE" lines (src/v/test.c) against the states the
session must log (tools/script.py: the characters, their styles' sum,
modified, the document's name), the files it must leave (read with
tools/cwrd.py: well formed, the characters and styles, or the texts they
must contain), "CW ASIDE" / "CW DESK" / "CW BACK" (put aside and brought
back), no "HUNG" or "PANIC"; the cursor's blink (W42, tools/check_blink.py:
32 frames shown, 20 hidden, the main loop fast and slowed); the timings (CW TIME, TIME) printed in
seconds at 8 MHz. --partial: a session stopped early (only what it got
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

MHZ = 8e6


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
    for m in re.finditer(r'CW STATE ((?:[0-9A-F]{8} ){12})(.*)', con):
        f = [int(x, 16) for x in m.group(1).split()]
        got[f[0]] = {'len': f[1], 'lines': f[2], 'cur': f[3], 'anc': f[4], 'modified': f[5], 'style': f[6],
                     'top': f[7], 'sum': f[8], 'chars': f[9], 'banks': f[10] >> 8, 'free': f[10] & 255,
                     'psum': f[11], 'name': m.group(2).strip()}
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
        print('state %2d: %6d characters, %5d lines, sum %08X, paragraphs %08X, modified %d, banks %d (%d free) %s'
              % (n, g['chars'], g['lines'], g['sum'], g['psum'], g['modified'], g['banks'], g['free'], g['name']))
    if not partial:
        for marker in ('CW ASIDE', 'CW DESK RESUME', 'CW BACK'):
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
                                     'g': 'go to page 60', 'm': 'open BIG2.cwr again'}.get(m.group(1), m.group(1)),
                                    int(m.group(2), 16) / MHZ))
    if not partial and 'SESSION' not in exp:
        bb, blink = check_blink.check(con, 32, 20)
        bad += bb
        print(blink)
    for b in bad:
        print('FAIL ' + b)
    sys.exit(1 if bad else 0)


main()
