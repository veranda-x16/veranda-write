#!/usr/bin/env python3
"""Veranda Write's gate: no direct call into an overlay that may not be
mapped (the first crash of the X16 build was one: the clipboard, overlay
7, calling a routine of overlay 1).

usage: check_ovl.py <build folder>      (build/veranda: the .o files)

Each object's code is in main RAM (CODE) or in one overlay (OVERLAYn);
an object that uses a symbol of another object whose code is in a
different overlay is a call that crashes, unless it is one of the known
uses below: an address handed to callb() (which maps the overlay first),
a menu table's routine (the kernel calls it with appBank mapped), glue.s's
wrappers (callb, then the e_ function), the player's.

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import glob
import os
import re
import subprocess
import sys

# importer object -> symbols it only takes the address of (callb, tables)
ALLOW = {
    'glue.s.o': '*',            # the wrappers: far maps the overlay, then calls e_...
    'menus.o': '*',             # the menus' routines: the kernel calls them in appBank (overlay 1)
    'ui.o': {'_f_new', '_f_open', '_f_save', '_f_saveas', '_f_close', '_f_quit', '_f_startdoc',
             '_clip_copy', '_clip_cut', '_clip_paste', '_menus_layout', '_sizes_fill', '_menu_make',
             '_t_named', '_t_format', '_t_body', '_fp_open', '_fp_key', '_fp_press', '_fp_next',
             '_t_count', '_t_goto', '_t_date', '_f_ldstep', '_t_popup',
             '_pg_loaddef', '_pp_key', '_pp_open', '_pp_press', '_t_pbreak', '_t_pdf'},
    'pgpanel.o': {'_pg_loaddef'},
    'tools.o': {'_m_item'},     # (the lists' routine: the kernel calls it in appBank, overlay 1)
    'view.o': {'_v_all', '_v_frame', '_v_refresh', '_t_toolbar', '_v_fix_cursor', '_v_scroll',
               '_v_title', '_t_status'},
    'view2.o': {'_t_sbar'},
    'popup.o': {'_menu_make'},
    'tbar.o': {'_t_ruler'},
    'test.o': {'_log_state'},
    'cbuf.o': {'_sw_stamp'},    # (data in overlay 11's bank: cbuf.c maps it first, sw_tbank)
    'cbread.s.o': {'_sw_stamp'},
    'main.o': {'_ui_main', '_vr_file'},   # (bank 1 and bank 3 mapped first)
}


def od65(flag, path):
    return subprocess.run(['od65', flag, path], capture_output=True, text=True).stdout


def where(path):
    """the object's code: 0 main RAM, n overlay n (None: no code)"""
    segs = re.findall(r'^\s+(\w+):\s+(\d+)', od65('--dump-segsize', path), re.M)
    w = set()
    for name, size in segs:
        if int(size) == 0:
            continue
        m = re.match(r'OVERLAY(\d+)$', name)
        if m:
            w.add(int(m.group(1)))
        elif name in ('CODE',):
            w.add(0)
    return w


def names(text, kind):
    return set(re.findall(r'^\s+Name:\s+"(\w+)"', text, re.M))


def main():
    d = sys.argv[1]
    addr = {}
    for line in open(os.path.join(d, 'labels.txt')):
        f = line.split()
        if len(f) == 3:
            addr['_' + f[2].lstrip('._')] = addr.get('_' + f[2].lstrip('._'), int(f[1], 16))
    # the objects the link took (the map's modules: stale ones left out)
    mp = open(os.path.join(d, 'cw.map')).read()
    linked = set(re.findall(r'^(\S+\.o):', mp.split('Segment list')[0], re.M))
    objs = [p for p in glob.glob(os.path.join(d, '**', '*.o'), recursive=True)
            if os.path.basename(p) in {os.path.basename(x) for x in linked}
            and os.path.dirname(os.path.relpath(p, d)).split(os.sep)[0] != 'test']
    loc, exp, imp = {}, {}, {}
    for p in objs:
        b = os.path.basename(p)
        loc[b] = where(p)
        for n in names(od65('--dump-exports', p), 'e'):
            exp[n] = b
        imp[b] = names(od65('--dump-imports', p), 'i')
    bad = 0
    for b, syms in sorted(imp.items()):
        allow = ALLOW.get(b, set())
        if allow == '*':
            continue
        for s in sorted(syms):
            e = exp.get(s)
            if not e:
                continue
            src, dst = loc[b], loc[e]
            if not dst or dst == {0} or s in allow or addr.get(s, 0xFFFF) < 0xA000:
                continue                # (main RAM: code or data)
            # the callee's code is in an overlay: the caller must be in that one only
            if 0 in src or src - dst or len(dst) > 1:
                if len(dst) > 1 and 0 in dst:
                    continue            # (an object in both: e.g. test.o's resident part)
                print('check_ovl: %s (%s) uses %s of %s (%s)' % (b, sorted(src), s, e, sorted(dst)))
                bad += 1
    if bad:
        sys.exit(1)
    print('check_ovl: no call into an overlay that may not be mapped (%d objects)' % len(objs))


main()
