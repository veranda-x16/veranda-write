#!/usr/bin/env python3
"""Veranda Write's gate: the scripted session and what it must leave.

usage: script.py <run folder> [--stop NAME]

Writes into the run folder BIG.cwr (a styled document of BIG_SIZE bytes,
140,000 by default (at 512 KB of RAM banks), four times main RAM: tools/cwrd.py; the gate's SD
card run takes 200,000 with 2 MB of RAM banks), the session CW.PLY for tools/play.s
and expect.json: the states its 'L' records must log (the characters and
the sum of their styles, src/v/test.c's) and the files it must leave.
With --stop NAME the session ends there with a VRAM dump (a screenshot).

The session, as a user would, by keys, the menus and the pointer: typing
in styles (Ctrl+B, Ctrl+I, Ctrl+U, Larger, the Font menu); a selection
(Shift+Home) made bold, undone, redone, undone; typing undone as one
step; Select all, Copy, Ctrl+End, Paste (the styles kept); a word
double-clicked and made bold, the wheel; Save as through the file
chooser (a long name); Alt+Tab: put aside, the desktop (a test one)
brings it back; New; Open (by its first letter); BIG.cwr opened, typed
in at both ends, paged through, a burst of keys typed at full speed (the
key queue full), saved as BIG2.cwr.

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cwrd  # noqa: E402
import gwimport  # noqa: E402

KEY_UP, KEY_DOWN, KEY_RIGHT, KEY_LEFT = 0x91, 0x11, 0x1D, 0x9D
HOME, END, PGUP, PGDN, DEL, BS, RET, TAB, ESC = 0x13, 0x04, 0x82, 0x02, 0x19, 0x14, 0x0D, 0x09, 0x1B
SHIFT_HOME = 0x93                       # (CLR: the ROM's Shift + Home)
CTRL_HOME, CTRL_END = 0x82, 0x84        # what Veranda's key hook makes of them (with Ctrl)
SHIFT, ALT, CTRL = 1, 2, 4
SANS, MONO = 'Gate Sans', 'Gate Mono'
SIZES = {SANS: [9, 12, 18, 27], MONO: [10], 'System': [9]}
BODY, NAMED = cwrd.BODY, cwrd.NAMED
NM_SIZE = [0, 27, 18, 12, 0, 0, 0]          # the named styles' characters (src/edit.c)
NM_FLAGS = [0, 1, 1, 1, 2, 0, 0]
PF_ALIGN, PF_INDENT, PF_SPACING, PF_AFTER = 1, 2, 3, 4
TB_Y, TB_ALIGN, TB_LIST, TB_INDENT, TB_NAME = 32, 240, 320, 368, 416   # the toolbar (src/v/cwv.h)
YES_BUTTON = (255, 271)                 # a Yes/No box's Yes (the default box, centred)
NO_BUTTON = (319, 271)                  # a Yes/No/Cancel box's No (its middle icon)
# the menu bar (src/v/menus.c, stage 3: Insert after Edit)
APPM, FILEM, EDIT, INSERT, TEXT, STYLE, FORMAT, FONT, SIZE = range(9)


def is_head(k):
    return 1 <= k <= 3


def is_list(k):
    return k >= 5


class Model:
    """the document's characters and styles, the cursor, the selection,
    the typing style, as the program's rules have them (cw.h, edit.c)"""

    def __init__(self, chars=(), paras=None):
        self.c = list(chars)
        self.p = list(paras) if paras else [BODY] * (1 + sum(1 for ch in self.c if ch[0] == cwrd.CR))
        self.cur = self.anc = 0
        self.ty = (SANS, 12, 0)
        self.kept = False
        self.hist = []
        self.redo = []
        self.mod = False

    def snap(self):
        self.hist.append((list(self.c), self.cur, self.anc, list(self.p)))
        self.redo = []

    def par_of(self, k):
        return sum(1 for ch in self.c[:k] if ch[0] == cwrd.CR)

    def par_range(self, q):
        """paragraph q's characters (its CR too): a, b"""
        a = 0
        for _ in range(q):
            a = next(i for i in range(a, len(self.c)) if self.c[i][0] == cwrd.CR) + 1
        b = a
        while b < len(self.c) and self.c[b][0] != cwrd.CR:
            b += 1
        return a, min(b + 1, len(self.c))

    def sel_pars(self):
        a, b = self.sel()
        return self.par_of(a), self.par_of(b - 1 if b > a else a)

    def sel(self):
        return min(self.cur, self.anc), max(self.cur, self.anc)

    def typing_style(self, k):
        if k == 0 or self.c[k - 1][0] == cwrd.CR:
            if k < len(self.c):
                return self.c[k][1:]
            return self.c[k - 1][1:] if k else (SANS, 12, 0)
        return self.c[k - 1][1:]

    def moved(self):
        self.kept = False
        self.ty = self.typing_style(self.sel()[0])

    def move(self, k, extend=False):
        self.cur = k
        if not extend:
            self.anc = k
        self.moved()

    def type(self, s, step=True):
        if step:
            self.snap()
        for ch in s:
            a, b = self.sel()
            if ch == '\r' and a == b:
                q = self.par_of(a)
                k = self.p[q][0]
                pa, pb = self.par_range(q)
                if is_list(k) and a == pa and (a == len(self.c) or self.c[a][0] == cwrd.CR):
                    self.named(0)       # Return in an empty list item: Body
                    continue
            if a != b:
                self.ty = self.typing_style(a)
                q = self.par_of(a)
                m = sum(1 for x in self.c[a:b] if x[0] == cwrd.CR)
                del self.p[q + 1:q + 1 + m]
                del self.c[a:b]
            st = self.ty
            if ch == '\r':
                q = self.par_of(a)
                ps = self.p[q]
                at_end = a == len(self.c) or self.c[a][0] == cwrd.CR
                np = BODY if is_head(ps[0]) and at_end else ps
                self.p.insert(q + 1, np)
            self.c.insert(a, (ord(ch) if ch != '\r' else cwrd.CR,) + st)
            self.cur = self.anc = a + 1
            if ch == '\r' and is_head(ps[0]) and np == BODY:
                f, z, g = self.ty            # a heading's end: Body's characters next
                self.ty = (f, 12, g & ~NM_FLAGS[ps[0]])
                self.kept = True
        self.mod = True

    def pbreak(self):
        """Insert > Page break (pgview.c's t_pbreak): a Return first
        inside a paragraph, then a page break's paragraph (an empty Body
        one, its CR in the style before it) before the cursor's"""
        self.snap()
        a = self.sel()[0]
        q = self.par_of(a)
        pa, _ = self.par_range(q)
        if a != pa:
            self.ty = self.typing_style(a)
            self.type('\r', step=False)
            a = self.cur
            q += 1
        st = self.c[a - 1][1:] if a else (SANS, 12, 0)
        self.c.insert(a, (cwrd.CR,) + st)
        self.p.insert(q, BODY)
        self.cur = self.anc = a + 1
        self.moved()
        self.mod = True

    def named(self, k):
        """the named style k: the selection's paragraphs and their characters"""
        self.snap()
        p0, p1 = self.sel_pars()
        lo = self.sel()[0]
        o0 = self.p[p0][0]
        kept = self.kept and self.cur == self.anc
        ty = self.ty
        for q in range(p0, p1 + 1):
            o = self.p[q][0]
            a, b = self.par_range(q)
            for i in range(a, b):
                c, f, z, g = self.c[i]
                g &= ~NM_FLAGS[o]
                if is_head(k):
                    z = NM_SIZE[k]
                elif is_head(o):
                    z = 12
                self.c[i] = (c, f, z, g | NM_FLAGS[k])
            self.p[q] = NAMED[k]
        self.mod = True
        f, z, g = ty if kept else self.typing_style(lo)
        g &= ~NM_FLAGS[o0]
        if is_head(k):
            z = NM_SIZE[k]
        elif is_head(o0):
            z = 12
        self.ty = (f, z, g | NM_FLAGS[k])
        self.kept = self.cur == self.anc

    def para(self, op, v):
        self.snap()
        p0, p1 = self.sel_pars()
        if op == PF_AFTER and self.p[p0][6]:
            v = 0
        for q in range(p0, p1 + 1):
            k, al, le, ri, fi, be, af, sp = self.p[q]
            if op == PF_ALIGN:
                al = v
            elif op == PF_INDENT:
                le = max(0, min(240, le + v))
            elif op == PF_SPACING:
                sp = v
            elif op == PF_AFTER:
                af = v
            self.p[q] = (k, al, le, ri, fi, be, af, sp)
        self.mod = True

    def fmt(self, f):
        """the style change f(style) -> style: the selection's, else the typing style's"""
        a, b = self.sel()
        if a == b:
            self.ty = f(self.ty)
            self.kept = True
            return
        self.snap()
        for i in range(a, b):
            ch = self.c[i]
            self.c[i] = (ch[0],) + f(ch[1:])
        self.mod = True
        self.ty = self.typing_style(a)

    def toggle(self, bit):
        a, b = self.sel()
        if a == b:
            on = self.ty[2] & bit
        else:
            on = all(ch[3] & bit for ch in self.c[a:b] if ch[0] != cwrd.CR)
        self.fmt(lambda s: (s[0], s[1], s[2] & ~bit if on else s[2] | bit))

    def larger(self, d):
        font, size, fl = self.ty
        sz = SIZES[font]
        n = [x for x in sz if x > size] if d > 0 else [x for x in sz if x < size]
        if n:
            v = n[0] if d > 0 else n[-1]
            self.fmt(lambda s: (s[0], v, s[2]))

    def undo(self):
        self.redo.append((list(self.c), self.cur, self.anc, list(self.p)))
        self.c, self.cur, self.anc, self.p = self.hist.pop()
        self.redo_kept = self.redo
        self.mod = True
        self.moved()

    def do_redo(self):
        self.hist.append((list(self.c), self.cur, self.anc, list(self.p)))
        self.c, self.cur, self.anc, self.p = self.redo.pop()
        self.mod = True

    # find (src/find.c's rules): case folded unless match case (Latin-1's
    # capitals too), whole words (letters, digits, _), no match across a CR
    @staticmethod
    def fold(c, case):
        if not case and (65 <= c <= 90 or (0xC0 <= c <= 0xDE and c != 0xD7)):
            return c + 32
        return c

    @staticmethod
    def wordc(c):
        return chr(c).isalnum() and c < 128 or c == 95 or (c >= 0xC0 and c not in (0xD7, 0xF7))

    def match_at(self, i, pat, case, word):
        n = len(pat)
        if i + n > len(self.c):
            return False
        for j in range(n):
            c = self.c[i + j][0]
            if c == cwrd.CR or self.fold(c, case) != self.fold(ord(pat[j]), case):
                return False
        if word:
            if i > 0 and self.wordc(self.c[i - 1][0]):
                return False
            if i + n < len(self.c) and self.wordc(self.c[i + n][0]):
                return False
        return True

    def find(self, pat, case=False, word=False):
        """Find next from the selection's end, from the start again: selected"""
        a, b = self.sel()
        start = b
        for i in list(range(start, len(self.c))) + list(range(0, start)):
            if self.match_at(i, pat, case, word):
                self.anc, self.cur = i, i + len(pat)
                self.moved()
                return True
        return False

    def replace_all(self, pat, rep, case=False, word=False):
        self.snap()
        out = []
        i = 0
        n = 0
        while i < len(self.c):
            if self.match_at(i, pat, case, word):
                st = self.c[i][1:]
                out.extend((ord(ch),) + st for ch in rep)
                i += len(pat)
                n += 1
            else:
                out.append(self.c[i])
                i += 1
        self.c = out
        self.cur = self.anc = min(self.cur, len(out))
        self.mod = True
        return n

    def state(self, name, pages=None):
        st = {'chars': len(self.c), 'sum': cwrd.doc_sum(self.c), 'psum': cwrd.par_sum(self.p),
              'modified': int(self.mod), 'name': name}
        if pages:
            st['pages'] = pages
        return st


class Script:
    def __init__(self):
        self.b = bytearray()
        self.names = {}
        self.states = []

    def P(self, x, y):
        self.b += bytes([ord('P'), x & 255, x >> 8, y & 255, y >> 8])

    def click(self, x, y, n=1):
        self.P(x, y)
        for _ in range(n):
            self.b += b'D'
            self.W(1)
            self.b += b'U'
            self.W(1)

    def menu(self, bar, item, shot=None):
        self.b += bytes([ord('C'), bar, ord('D'), ord('W'), 4, ord('U'), ord('W'), 10])
        self.b += bytes([ord('J'), item, ord('W'), 4])
        if shot:
            self.stop(shot)                     # (a screenshot with the list open)
        self.b += bytes([ord('D'), ord('W'), 4, ord('U'), ord('W'), 10])

    def popup(self, x, item, shot=None):
        """a toolbar pop-up (src/v/popup.c): clicked (its list stays
        open), then its item clicked (16 lines an item from y 48)"""
        self.click(x, TB_Y + 7)
        self.W(4)
        self.P(x, 48 + 16 * item + 8)           # (the item lit)
        self.W(4)
        if shot:
            self.stop(shot)
        self.click(x, 48 + 16 * item + 8)
        self.W(10)

    def K(self, k, mods=0):
        if mods & CTRL and 0x40 <= k < 0x80:
            k &= 0x1F
        if mods:
            self.b += bytes([ord('M'), mods])
        self.b += bytes([ord('K'), k])
        if mods:
            self.b += bytes([ord('M'), 0])

    def T(self, s):
        s = s.encode('latin-1') if isinstance(s, str) else s
        for i in range(0, len(s), 8):
            c = s[i:i + 8]
            self.b += bytes([ord('T'), len(c)]) + c

    def W(self, n):
        self.b += bytes([ord('W'), n])

    def M(self, m):
        self.b += bytes([ord('M'), m])

    def H(self, n):
        self.b += bytes([ord('H'), n & 255])

    def L(self, n, want):
        self.b += bytes([ord('L'), n])
        self.W(30)
        self.states.append((n, want))

    def B(self, m, n=7):
        """the blink probe: n changes of the cursor, the main loop slowed
        by m (tools/play.s, tools/check_blink.py)"""
        self.b += bytes([ord('B'), m, n])

    def Z(self):
        self.b += b'Z'

    def Y(self, c):
        self.b += bytes([ord('Y'), ord(c)])

    def stop(self, name):
        self.names[name] = len(self.b)


POLISH = 'Zażółć gęślą jaźń\rZAŻÓŁĆ GĘŚLĄ JAŹŃ'
LATIN1 = 'Café über Ñandú, ÅØ ½'        # (a 1.2 file's Latin-1: Ñ Å Ø ½ are not in the set)


def polish(s, run, files):
    """(W38) Polish in the Veranda set (ISO-8859-16): typed as the kernel's
    Polish keyboard gives it (the letters' codes, no Alt or Ctrl), drawn
    (the screenshot 'polish', tools/check_polish.py), saved (1.3, INFO's
    set 1) and opened again; a 1.2 document's Latin-1 converted"""
    s.K(ord('n'), CTRL)
    s.W(20)
    p = Model()
    b = POLISH.encode('iso8859_16')
    s.T(b)
    p.type(b.decode('latin-1'))
    s.L(33, p.state(''))
    s.stop('polish')
    s.K(ord('s'), CTRL | SHIFT)
    s.W(40)
    for _ in range(len('Untitled.cwr')):
        s.K(BS)
    s.T('Polish.cwr')
    s.K(RET)
    s.W(40)
    p.mod = False
    s.L(34, p.state('Polish.cwr'))
    files['Polish.cwr'] = (list(p.c), list(p.p))
    s.K(ord('n'), CTRL)
    s.W(20)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('P'))
    s.W(10)
    s.K(RET)
    s.W(60)
    s.L(35, p.state('Polish.cwr'))
    lat = [(c, SANS, 12, 0) for c in LATIN1.encode('latin-1')]
    cwrd.write(os.path.join(run, 'Latin.cwr'), lat, minor=2)
    conv = [((ord('?') if bytes([c]).decode('iso8859_16') != chr(c) else c),) + tuple(st) for c, *st in lat]
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('L'))
    s.W(10)
    s.K(RET)
    s.W(60)
    s.L(36, Model(conv).state('Latin.cwr'))
    s.stop('latin')


def codes_bs(s, files):
    """(W41) deleting the last characters after a style code: the code
    goes too (doc_tidy), so the cursor must not stay where the deleted
    character was (past the end: the status bar's column walk never
    ended, the user's hang on the X16). "W dzisie", Ctrl+E (Larger: what
    right Alt + e gives without Veranda's Polish keyboard), "ee", the
    Backspace key held (the keyboard's repeat, keys coming while it
    draws) to the empty document and past it; Delete and Cut of the last
    styled characters, typing on after each"""
    def held(k, n):
        s.b += bytes([ord('R'), k, n, 2])
        s.W(30)

    def state(n, m):
        st = m.state('')
        st['cur_in'] = 1
        s.L(n, st)

    def bs(m, n):
        del m.c[max(0, len(m.c) - n):]
        m.cur = m.anc = len(m.c)
        m.moved()

    s.K(ord('n'), CTRL)
    s.W(20)
    m = Model()
    s.T('W dzisie')
    m.type('W dzisie')
    s.K(ord('e'), CTRL)
    m.larger(1)
    s.T('ee')
    m.type('ee')
    held(BS, 2)                                 # (the hang: the second one)
    bs(m, 2)
    state(37, m)
    held(BS, 12)                                # to the empty document, and on
    bs(m, 12)
    state(38, m)
    s.T('ok')
    m.type('ok')
    state(39, m)
    s.K(ord('a'), CTRL)                         # Delete: the last character, after a code
    s.K(BS)
    m = Model()
    m.mod = True
    s.T('ab')
    m.type('ab')
    s.K(ord('b'), CTRL)
    m.toggle(1)
    s.T('c')
    m.type('c')
    s.K(KEY_LEFT)
    m.move(2)
    s.K(DEL)
    del m.c[2:]
    m.moved()
    s.T('d')
    m.type('d')
    state(40, m)
    s.K(ord('b'), CTRL)                         # Cut: the last characters, after a code
    m.toggle(1)
    s.T('ef')
    m.type('ef')
    s.K(KEY_LEFT, SHIFT)
    s.K(KEY_LEFT, SHIFT)
    s.K(ord('x'), CTRL)
    del m.c[3:]
    m.cur = m.anc = 3
    m.moved()
    s.T('g')
    m.type('g')
    state(41, m)
    s.K(ord('s'), CTRL | SHIFT)                 # saved (the Open after it asks nothing)
    s.W(40)
    for _ in range(len('Untitled.cwr')):
        s.K(BS)
    s.T('Codes.cwr')
    s.K(RET)
    s.W(40)
    m.mod = False
    s.L(42, m.state('Codes.cwr'))
    files['Codes.cwr'] = (list(m.c), list(m.p))


def session(run):
    s = Script()
    files = {}
    s.W(90)
    s.stop('start')
    s.B(0)                                      # (W42) the cursor's blink, fast and slowed
    s.B(40)
    s.B(200)
    m = Model()
    # typing in styles
    def typ(t):
        s.T(t)
        m.type(t, step=True)
    s.T('Commander Write is a word processor for the X16. ')
    m.type('Commander Write is a word processor for the X16. ')
    for key, bit, word in ((ord('b'), 1, 'Bold'), (ord('i'), 2, 'italic'), (ord('u'), 4, 'underlined')):
        s.K(key, CTRL)
        m.toggle(bit)
        typ(word)
        s.K(key, CTRL)
        m.toggle(bit)
        typ(', ' if bit == 1 else ' and ' if bit == 2 else ' text.\r')
    s.K(ord('e'), CTRL)                         # Larger: 18 points
    m.larger(1)
    typ('A heading in 18 points')
    s.K(ord('e'), CTRL | SHIFT)                 # Smaller: 12
    m.larger(-1)
    typ('\r')
    s.menu(FONT, 1)                             # Font > Gate Mono
    m.fmt(lambda st: (MONO, st[1], st[2]))
    typ('Gate Mono, for code.\r')
    s.menu(FONT, 2)                             # Font > Gate Sans
    m.fmt(lambda st: (SANS, st[1], st[2]))
    typ('Back to Gate Sans; the end of the first page of tests.')
    s.L(1, m.state(''))
    s.stop('styles')
    # a selection made bold, undone, redone, undone
    s.K(SHIFT_HOME, SHIFT)
    lo = max(i for i, ch in enumerate(m.c) if ch[0] == cwrd.CR) + 1
    m.move(lo, extend=True)
    s.K(ord('b'), CTRL)
    m.toggle(1)
    s.L(2, m.state(''))
    s.stop('selbold')
    s.K(ord('z'), CTRL)
    m.undo()
    s.L(3, m.state(''))
    s.K(ord('y'), CTRL)
    m.do_redo()
    s.L(4, m.state(''))
    s.K(ord('z'), CTRL)
    m.undo()
    s.K(END)                                    # (the selection gone: the line's end)
    m.move(len(m.c))
    s.T(' oops')                                # typing: one step
    m.type(' oops')
    s.K(ord('z'), CTRL)
    m.undo()
    m.move(len(m.c))
    s.L(5, m.state(''))
    # Select all, Copy, the end, Return, Paste
    s.K(ord('a'), CTRL)
    s.K(ord('c'), CTRL)
    s.K(CTRL_END, CTRL)
    m.move(len(m.c))
    typ('\r')
    s.K(ord('v'), CTRL)
    m.snap()
    paste = list(m.c[:-1])
    m.c[len(m.c):] = paste
    m.p = [BODY] * (1 + sum(1 for ch in m.c if ch[0] == cwrd.CR))
    m.cur = m.anc = len(m.c)
    s.L(6, m.state(''))
    s.stop('pasted')
    # the pointer: a word double-clicked on the first line, made bold; the wheel
    s.click(140, 74, 2)
    s.stop('dbl1')
    s.K(ord('b'), CTRL)
    s.stop('dbl2')
    s.L(7, {'chars': len(m.c), 'sum_not': cwrd.doc_sum(m.c), 'modified': 1, 'name': ''})
    s.K(ord('z'), CTRL)
    s.H(1)
    s.W(20)
    s.H(-1)
    s.W(20)
    s.L(8, m.state(''))
    # Save as, through the file chooser
    name = 'Stage one test.cwr'
    s.K(ord('s'), CTRL | SHIFT)
    s.W(40)
    for _ in range(len('Untitled.cwr')):
        s.K(BS)
    s.T(name)
    s.stop('saveas')
    s.K(RET)
    s.W(40)
    m.mod = False
    s.L(9, m.state(name))
    files[name] = (list(m.c), list(m.p))
    s.stop('saved')
    # Alt+Tab: put aside; the test desktop brings it back
    s.K(TAB, ALT)
    s.W(120)
    s.L(10, m.state(name))
    # New, Open by its first letter
    s.K(ord('n'), CTRL)
    s.W(20)
    e = Model()
    s.L(11, e.state(''))
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('S'))
    s.W(10)
    s.K(RET)
    s.W(60)
    m.mod = False
    s.L(12, m.state(name))
    # paragraph styles, in a new document: the Style menu, the toolbar, the keys
    s.K(ord('n'), CTRL)
    s.W(20)
    m = Model()

    def typ(t):
        s.T(t)
        m.type(t, step=True)
    s.menu(STYLE, 1)                            # Style > Heading 1
    m.named(1)
    typ('Veranda Write\r')                      # (Return at a heading's end: Body)
    typ('A word processor for the Commander X16, with paragraph styles: headings, lists, '
        'quotations, alignment and spacing.\r')
    s.menu(STYLE, 2)                            # Heading 2
    m.named(2)
    typ('Lists\r')
    s.click(TB_LIST + 9, TB_Y + 7)              # the toolbar: a bulleted list
    m.named(5)
    typ('First item\rSecond item, long enough to wrap: its second line starts under its first word, '
        'not under the bullet.\rThird item\r\r')   # (Return in an empty item: Body)
    s.popup(TB_NAME + 20, 6, 'popup')           # the toolbar's style pop-up: a numbered list
    m.named(6)
    typ('One\rTwo\rThree\r\r')
    s.K(ord('c'), CTRL | SHIFT)                 # centred
    m.para(PF_ALIGN, 1)
    typ('A centred line\r')
    s.K(ord('l'), CTRL)                         # left again
    m.para(PF_ALIGN, 0)
    s.menu(STYLE, 4)                            # a quotation
    m.named(4)
    typ('A quotation, indented on both sides and in italic, long enough to wrap onto a second '
        'line of the page.\r')
    s.menu(STYLE, 0)                            # Body, justified, a line and a half
    m.named(0)
    s.K(ord('j'), CTRL)
    m.para(PF_ALIGN, 3)
    s.menu(FORMAT, 7, 'menu')                   # Format > One and a half lines
    m.para(PF_SPACING, 3)
    typ('A justified paragraph: its lines reach both edges of the column, the spaces between '
        'its words stretched to fill them, all but its last line, which stays at the left.')
    s.L(17, m.state(''))
    s.stop('paras')
    s.K(ord('z'), CTRL)                         # undo the typing, the spacing, the alignment
    m.undo()
    s.K(ord('z'), CTRL)
    m.undo()
    s.K(ord('z'), CTRL)
    m.undo()
    s.L(18, m.state(''))
    s.K(ord('y'), CTRL)
    m.do_redo()
    s.K(ord('y'), CTRL)
    m.do_redo()
    s.K(ord('y'), CTRL)
    m.do_redo()
    s.L(19, m.state(''))
    # find and replace: Find "second" (from the start again), typed over;
    # Replace all, match case; undone and redone as one step; whole words
    s.K(ord('f'), CTRL)
    s.T('second')
    s.K(RET)
    m.find('second')
    s.stop('find')
    s.K(ESC)
    typ('2nd')
    s.L(22, m.state(''))
    s.K(ord('h'), CTRL)                         # the panel again, at Replace with
    s.T('next')
    s.K(ord('c'), ALT)                          # match case
    s.K(ord('a'), ALT)                          # Replace all
    m.replace_all('second', 'next', case=True)
    s.stop('replaced')
    s.L(23, m.state(''))
    s.K(ord('z'), CTRL)
    m.undo()
    s.L(24, m.state(''))
    s.K(ord('y'), CTRL)
    m.do_redo()
    s.L(25, m.state(''))
    s.K(ord('f'), CTRL)                         # Find: "line", whole words, any case
    for _ in range(len('second')):
        s.K(BS)
    s.T('line')
    s.K(ord('c'), ALT)
    s.K(ord('w'), ALT)
    s.K(TAB)
    for _ in range(len('next')):
        s.K(BS)
    s.T('row')
    s.K(ord('a'), ALT)
    m.replace_all('line', 'row', word=True)
    s.L(26, m.state(''))
    s.K(ESC)
    s.menu(0, 1)                                # Word count
    s.W(20)
    s.stop('count')
    s.K(RET)
    s.W(20)
    name2 = 'Styles.cwr'
    s.K(ord('s'), CTRL | SHIFT)
    s.W(40)
    for _ in range(len('Untitled.cwr')):
        s.K(BS)
    s.T(name2)
    s.K(RET)
    s.W(40)
    m.mod = False
    s.L(20, m.state(name2))
    files[name2] = (list(m.c), list(m.p))
    # a 1.0 file (stage 1's, no PARA) opens: its paragraphs Body
    old = [(ord(c), SANS, 12, 1 if i < 5 else 0) for i, c in enumerate('Stage one wrote this file.\rIn 1.0.')]
    cwrd.write(os.path.join(run, 'OLD.cwr'), old, minor=0)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('O'))
    s.W(10)
    s.K(RET)
    s.W(60)
    s.L(21, Model(old).state('OLD.cwr'))
    polish(s, run, files)
    codes_bs(s, files)
    # a geoWrite document (W24-W27): the Open dialog lists it (by its
    # class: the applications' .cvt files not), opening it asks, it is
    # converted (tools/gwimport.py's result), what was left out said; it
    # is Untitled-Geo letter, Save writes Geo letter.cwr (the original
    # kept), which opens again
    gwdata = gwimport.sample()
    open(os.path.join(run, 'Geo letter.cvt'), 'wb').write(gwdata)
    gd = gwimport.doc(gwimport.convert(gwdata))
    g = Model(gd.chars(), gd.paras())
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('G'))
    s.W(10)
    s.stop('gwlist')
    s.K(RET)
    s.W(40)
    s.stop('gwask')
    s.click(*YES_BUTTON)
    s.W(60)
    s.stop('gwnote')
    s.K(RET)                                    # (the note's OK)
    s.W(40)
    g.mod = True
    s.L(30, g.state(''))
    s.stop('gwdoc')
    s.K(ord('s'), CTRL)                         # Save: Save as, "Geo letter.cwr" offered
    s.W(40)
    s.stop('gwsave')
    s.K(RET)
    s.W(40)
    g.mod = False
    s.L(31, g.state('Geo letter.cwr'))
    files['Geo letter.cwr'] = (list(g.c), list(g.p))
    files['Geo letter.cvt'] = {'raw': gwdata}
    s.K(ord('n'), CTRL)
    s.W(20)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('G'))                               # Geo letter.cvt, then Geo letter.cwr
    s.W(10)
    s.K(ord('G'))
    s.W(10)
    s.K(RET)
    s.W(60)
    s.L(32, g.state('Geo letter.cwr'))
    # the big document
    big = cwrd.big_chars(int(os.environ.get('BIG_SIZE', '140000')))
    cwrd.write(os.path.join(run, 'BIG.cwr'), big)
    b = Model(big)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('B'))
    s.W(10)
    s.Z()
    s.K(RET)
    s.W(30)
    s.Y('l')
    s.L(13, b.state('BIG.cwr'))
    s.stop('bigopen')
    s.K(CTRL_END, CTRL)                              # typed at the end and the start
    b.move(len(b.c))
    s.T(' The end.')
    b.type(' The end.')
    s.K(CTRL_HOME, CTRL)
    b.move(0)
    s.T('Start. ')
    b.type('Start. ')
    for _ in range(3):
        s.K(PGDN)
        s.W(10)
    s.K(KEY_DOWN)
    s.L(14, b.state('BIG.cwr'))
    # find across the big document: not found (all of it read), timed;
    # Replace all "while" by "as", whole words, timed; undone, redone
    s.K(ord('f'), CTRL)                         # (the panel keeps "line", whole words)
    for _ in range(len('line')):
        s.K(BS)
    s.T('zebra')
    s.Z()
    s.K(RET)
    s.W(30)
    s.Y('f')
    for _ in range(len('zebra')):
        s.K(BS)
    s.T('while')
    s.K(ord('h'), CTRL)
    for _ in range(len('row')):
        s.K(BS)
    s.T('as')
    s.Z()
    s.K(ord('a'), ALT)
    s.W(30)
    s.Y('r')
    b.replace_all('while', 'as', word=True)
    s.L(27, b.state('BIG.cwr'))
    s.stop('bigrepl')
    s.K(ord('z'), CTRL)
    b.undo()
    s.L(28, b.state('BIG.cwr'))
    s.K(ord('y'), CTRL)
    b.do_redo()
    s.L(29, b.state('BIG.cwr'))
    s.K(ESC)
    # a burst at full speed: 8 keys at a time, the key queue full
    s.K(HOME)
    burst = 'xquick brown fox jumps over the lazy dog and keeps on typing fast'
    s.T(burst)
    s.W(30)
    s.L(15, {'contains': burst})
    s.stop('big')
    s.K(ord('s'), CTRL | SHIFT)
    s.W(40)
    s.stop('bigsave')
    for _ in range(len('BIG.cwr')):
        s.K(BS)
    s.T('BIG2.cwr')
    s.Z()
    s.K(RET)
    s.W(30)
    s.Y('s')
    s.L(16, {'modified': 0, 'name': 'BIG2.cwr'})
    files['BIG2.cwr'] = {'contains': ['Start. ', ' The end.', burst], 'chars_min': len(b.c) + len(burst),
                         'absent': [' while ']}
    s.stop('end')
    return s, files


def session_big(run):
    """a document larger than RAM (BIG_SIZE bytes, 1,000,000 by default;
    the gate runs it at 512 KB and 2 MB of RAM banks on an SD card image):
    opened (the first screen, all of it), Ctrl+End, Ctrl+Home, Page Down,
    Go to page, found in the middle (MIDDLEMARK), typed in there, undone,
    redone, put aside (Alt+Tab) and back, saved as BIG2.cwr, opened again"""
    s = Script()
    files = {}
    if os.environ.get('BIG_NOSTATE'):
        # (BIG_NOSTATE: no states logged, the test build's sum of 4 MB
        # takes minutes: the file saved is checked on the PC instead)
        s.L = lambda n, want: s.W(30)
    big = cwrd.big_chars(int(os.environ.get('BIG_SIZE', '1000000')))
    k = next(i for i in range(len(big) // 2, len(big)) if big[i][0] == cwrd.CR) + 1
    big[k:k] = [(ord(c), SANS, 12, 0) for c in 'MIDDLEMARK ']
    cwrd.write(os.path.join(run, 'BIG.cwr'), big)
    b = Model(big)
    s.W(90)
    if os.environ.get('BIG_STALE'):
        s.K(RET)                                # ("did not end the last time: its swap was removed")
        s.W(20)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('B'))
    s.W(10)
    s.Z()
    s.K(RET)
    s.W(30)
    s.Y('l')                                    # (all of it read: the player waits while it is)
    s.L(1, b.state('BIG.cwr'))
    s.stop('bigopen')
    s.Z()
    s.K(CTRL_END, CTRL)                         # the end: its lines and text from the card
    s.W(10)
    s.Y('e')
    b.move(len(b.c))
    s.stop('bigend')
    s.Z()
    s.K(CTRL_HOME, CTRL)                        # the start: long put out of RAM
    s.W(10)
    s.Y('h')
    b.move(0)
    for _ in range(3):
        s.K(PGDN)                               # (timed: CW TIME KEY82)
        s.W(10)
    s.Z()
    s.K(ord('g'), CTRL | SHIFT)                 # Go to page 60
    s.W(30)
    s.T('60')
    s.K(RET)
    s.W(10)
    s.Y('g')
    s.stop('bigpage')
    s.K(ord('f'), CTRL)                         # find the middle: through cold text
    s.T('MIDDLEMARK')
    s.Z()
    s.K(RET)
    s.W(10)
    s.Y('f')
    s.K(ESC)
    s.K(KEY_RIGHT)
    b.cur = b.anc = k + len('MIDDLEMARK')
    b.moved()
    s.T('x marks the middle, typed here. ')     # (timed: CW TIME KEY78)
    b.type('x marks the middle, typed here. ')
    s.W(20)
    s.stop('bigmid')
    s.K(ord('z'), CTRL)
    b.undo()
    s.L(2, b.state('BIG.cwr'))
    s.K(ord('y'), CTRL)
    b.do_redo()
    s.L(3, b.state('BIG.cwr'))
    s.K(TAB, ALT)                               # put aside with chunks out on the card; back
    s.W(120)
    s.L(4, b.state('BIG.cwr'))
    s.K(ord('s'), CTRL | SHIFT)                 # saved as BIG2.cwr
    s.W(40)
    for _ in range(len('BIG.cwr')):
        s.K(BS)
    s.T('BIG2.cwr')
    s.Z()
    s.K(RET)
    s.W(30)
    s.Y('s')
    b.mod = False
    s.L(5, b.state('BIG2.cwr'))
    files['BIG2.cwr'] = (list(b.c), list(b.p))
    s.K(ord('n'), CTRL)                         # opened again
    s.W(20)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('B'))
    s.K(ord('B'))
    s.W(10)
    s.Z()
    s.K(RET)
    s.W(30)
    s.Y('m')
    s.L(6, b.state('BIG2.cwr'))
    s.K(TAB, ALT)
    s.W(120)
    s.stop('end')
    return s, files


def speed_doc():
    """the speed session's document: a short paragraph, a long one (some
    1,400 characters: 16 lines or so; LONGMARK on its first line), one in
    bold and italic runs (STYLEMARK italic in it), then plain paragraphs
    to fill the screen and more"""
    pl = (SANS, 12, 0)
    out = []

    def put(t, st=pl):
        out.extend((ord(ch),) + st for ch in t)

    def words(n, x, styled=False):
        for w in range(n):
            x = (x * 1103515245 + 12345) & 0x7FFFFFFF
            st = pl
            if styled and w % 2:
                st = (SANS, 12, 1) if w % 4 == 1 else (SANS, 12, 2)
            put(cwrd.WORDS[(x >> 8) % len(cwrd.WORDS)], st)
            put(' ')
        return x
    put('Typing speed: the document for the per-key timings.\r')
    put('A long paragraph begins here, LONGMARK on its first line, and it goes on ')
    x = words(230, 11)
    put('to its end.\r')
    put('Styles: ')
    x = words(20, x, True)
    put('STYLEMARK', (SANS, 12, 2))
    put(' ', (SANS, 12, 2))
    x = words(60, x, True)
    put('end.\r')
    for k in range(30):
        x = words(30 + k % 25, x)
        put('the end of paragraph %d.\r' % k)
    put('The last line.')
    return out


def session_speed(run):
    """the per-key timings (SESSION=speed; tools/speed.sh): each key put
    with 'Q' (the clock from the moment it is in the ROM's key queue to
    the program's drawing after it, "CW LAT"), one at a time, the program
    idle between them: typing at a document's end, in the first line of
    a long paragraph (it wraps again to its end), in a run of italic,
    with Bold turned on, Backspace, Return; then keys at 30 and 60 a
    second whatever the program does ('R'), and 8 at once: none lost
    (the states). Then BIG.cwr (BIG_SIZE, 400,000 by default: more than
    the 512 KB machine's cache, so it pages) typed in at its start, its
    end and its middle, and at 30 a second at its start"""
    s = Script()
    files = {}
    doc = speed_doc()
    cwrd.write(os.path.join(run, 'SPEED.cwr'), doc)
    big = cwrd.big_chars(int(os.environ.get('BIG_SIZE', '400000')))
    k = next(i for i in range(len(big) // 2, len(big)) if big[i][0] == cwrd.CR) + 1
    big[k:k] = [(ord(c), SANS, 12, 0) for c in 'MIDDLEMARK ']
    cwrd.write(os.path.join(run, 'BIG.cwr'), big)
    state_n = [0]

    s.labels = {}
    s.lossy = {}

    def state(m, name, label=None, lossy=False):
        """lossy: keys faster than the program keeps up with may be lost
        (the ROM's queue full): the characters counted, not checked; the
        model then takes the document as the program has it (chars)"""
        state_n[0] += 1
        st = m.state(name)
        st['cur_in'] = 1
        if lossy:
            s.lossy[state_n[0]] = st['chars']
            st = {'cur_in': 1}
        s.L(state_n[0], st)
        if label:
            s.labels[state_n[0]] = label

    def q(t, m):
        for ch in t:
            s.b += bytes([ord('Q'), ord(ch) if ch != '\r' else RET])
            m.type(ch)
        s.W(10)

    def held(k, n, f, m, t):
        s.b += bytes([ord('R'), k, n, f])
        s.W(60)
        m.type(t)

    def bs(m, n):
        for _ in range(n):
            s.b += bytes([ord('Q'), BS])
            del m.c[m.cur - 1]
            m.cur = m.anc = m.cur - 1
            m.mod = True
        m.moved()
        s.W(10)

    last = ['']

    def find(word, m, at):
        s.K(CTRL_HOME, CTRL)
        s.K(ord('f'), CTRL)
        for _ in range(len(last[0])):
            s.K(BS)
        s.T(word)
        last[0] = word
        s.K(RET)
        s.W(20)
        s.K(ESC)
        s.K(KEY_RIGHT)
        m.move(at)

    def opn(letter, m, name):
        s.K(ord('o'), CTRL)
        s.W(40)
        s.K(ord(letter))
        s.W(10)
        s.Z()
        s.K(RET)
        s.W(60)
        s.Y('l')
        state(m, name)

    s.W(90)
    m = Model(doc)
    opn('S', m, 'SPEED.cwr')
    s.K(CTRL_END, CTRL)                         # 1: at the document's end
    m.move(len(m.c))
    s.W(20)
    q(' and more', m)
    state(m, 'SPEED.cwr', "typing at a document's end")

    def mark(word):
        t = ''.join(chr(ch[0]) for ch in m.c)
        return t.index(word) + len(word)
    find('LONGMARK', m, mark('LONGMARK'))       # 2: the long paragraph's first line
    q(' typed in', m)
    state(m, 'SPEED.cwr', "typing in a long paragraph's first line")
    find('STYLEMARK', m, mark('STYLEMARK'))     # 3: in a run of italic
    q(' slanted', m)
    state(m, 'SPEED.cwr', 'typing in a run of italic')
    s.K(ord('b'), CTRL)                         # 4: Bold on, then typing
    m.toggle(1)
    q(' bolder', m)
    state(m, 'SPEED.cwr', 'typing with Bold turned on')
    find('typed in', m, mark('typed in'))
    bs(m, 9)                                    # 5: Backspace in the long paragraph
    state(m, 'SPEED.cwr', 'Backspace in the long paragraph')
    q('\r\r\r', m)                              # 6: Return splits it
    state(m, 'SPEED.cwr', 'Return in the long paragraph')
    find('LONGMARK', m, mark('LONGMARK'))       # 7: keys at 30 and 60 a second, 8 at once
    held(ord('x'), 60, 2, m, 'x' * 60)
    state(m, 'SPEED.cwr', '60 keys at 30 a second (long paragraph)')
    s.T('abcdefgh')
    m.type('abcdefgh')
    s.W(30)
    state(m, 'SPEED.cwr', '8 keys at once')
    held(ord('y'), 60, 1, m, 'y' * 60)          # (faster than the keyboard's repeat: some may be lost)
    state(m, 'SPEED.cwr', '60 keys at 60 a second (long paragraph)', lossy=True)
    s.K(ord('s'), CTRL)                         # (saved: the Open after it asks nothing)
    s.W(40)
    ln = []                                     # one-line paragraphs: a list, a letter's lines
    for k in range(400):
        ln.extend((ord(ch), SANS, 12, 0) for ch in 'Line %d of a list of short paragraphs.\r' % k)
    cwrd.write(os.path.join(run, 'LINES.cwr'), ln)
    n = Model(ln)
    opn('L', n, 'LINES.cwr')
    s.K(KEY_DOWN)                               # (the second line's end)
    s.K(END)
    n.move(next(i for i in range(len(ln)) if ln[i][0] == cwrd.CR and i > 40))
    s.W(20)
    q(' and more', n)
    state(n, 'LINES.cwr', 'one-line paragraphs: typing at the second one\'s end')
    s.K(ord('s'), CTRL)
    s.W(40)
    n.mod = False
    state(n, 'LINES.cwr')
    b = Model(big)
    opn('B', b, 'BIG.cwr')                      # 8: the big document, all of it read
    s.K(CTRL_HOME, CTRL)
    b.move(0)
    s.W(20)
    q('Start ', b)
    state(b, 'BIG.cwr', "the big document: typing at its start")
    s.K(CTRL_END, CTRL)
    b.move(len(b.c))
    s.W(20)
    q(' the end', b)
    state(b, 'BIG.cwr', "the big document: typing at its end")
    t = ''.join(chr(ch[0]) for ch in b.c)
    find('MIDDLEMARK', b, t.index('MIDDLEMARK') + len('MIDDLEMARK'))
    q(' middle', b)
    state(b, 'BIG.cwr', "the big document: typing in its middle")
    s.K(CTRL_HOME, CTRL)
    b.move(0)
    s.W(20)
    held(ord('z'), 60, 2, b, 'z' * 60)
    state(b, 'BIG.cwr', 'the big document: 60 keys at 30 a second at its start', lossy=True)
    s.stop('end')
    return s, files


HEAD = '{name}|Veranda Write, zażółć|{date}'       # (the page session's header)


def session_page(run):
    """stage 3 (SESSION=page; W44-W48): page breaks typed (Insert > Page
    break, Ctrl+Return), the header and footer (Insert > Header and
    footer: the panel, its fields, not on the first page), the page setup
    (File > Page setup: Letter clicked, the top margin stepped, OK
    clicked), saved (CWRD 1.4: PAGE) and opened again, exported as PDF
    (into /DOCUMENTS, Polish in it); documents of 1.1, 1.2 (a page break)
    and 1.3 opened; a big document (BIG_SIZE, 300,000 by default) opened,
    its pages counted, exported as PDF (timed), an export stopped with
    Esc (the file deleted), Go to page 5 (the PDF's page 5 starts there)"""
    s = Script()
    files = {}
    pdfs = []
    s.W(90)
    m = Model()

    def typ(t):
        s.T(t)
        m.type(t)
    # page breaks: the menu's, Ctrl+Return's
    pl = POLISH.encode('iso8859_16').replace(b'\r', b' ')
    typ('Page one of a document in pages. ')
    s.T(pl)
    m.type(pl.decode('latin-1'))
    typ('\rIts second paragraph.')
    s.menu(INSERT, 0)                           # Insert > Page break
    m.pbreak()
    typ('Page two, after a page break from the menu.')
    s.K(RET, CTRL)                              # Ctrl+Return
    m.pbreak()
    typ('Page three, after Ctrl+Return.')
    s.L(1, m.state('', 3))
    s.stop('pbreak')
    # the header and footer: the panel at the header
    s.menu(INSERT, 2)                           # Insert > Header and footer...
    s.W(20)
    s.T(HEAD.encode('iso8859_16'))                # (Polish in it: the bands draw it in Gate Sans 12)
    s.K(TAB)
    s.T('Page {page} of {pages}')
    s.K(TAB)
    s.K(ord(' '))                               # Not on the first page
    s.W(10)
    s.stop('hfpanel')
    s.K(RET)                                    # OK
    s.W(30)
    s.L(2, m.state('', 3))
    s.stop('bands')
    # the page setup: Letter clicked, the top margin 5 steps (0.05 inch) more, OK clicked
    s.menu(FILEM, 4)                            # File > Page setup...
    s.W(20)
    s.click(180, 320)                           # (the Letter button)
    s.W(10)
    s.K(KEY_DOWN)                               # Top
    for _ in range(5):
        s.K(KEY_RIGHT)
    s.W(10)
    s.stop('setup')
    s.click(480, 416)                           # (OK)
    s.W(40)
    s.L(3, m.state('', 3))
    # saved (1.4) and opened again
    s.K(ord('s'), CTRL | SHIFT)
    s.W(40)
    for _ in range(len('Untitled.cwr')):
        s.K(BS)
    s.T('Pages.cwr')
    s.K(RET)
    s.W(40)
    m.mod = False
    s.L(4, m.state('Pages.cwr', 3))
    files['Pages.cwr'] = (list(m.c), list(m.p))
    page = {'paper': 1, 'flags': 1, 'w': 12240, 'h': 15840, 'top': 1800, 'bottom': 1417, 'left': 1417,
            'right': 1417, 'head': HEAD, 'foot': 'Page {page} of {pages}'}
    s.K(ord('n'), CTRL)
    s.W(20)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('P'))
    s.W(10)
    s.K(RET)
    s.W(60)
    s.L(5, m.state('Pages.cwr', 3))
    # exported as PDF: the dialog in /DOCUMENTS, the document's name
    s.K(ord('p'), CTRL | SHIFT)
    s.W(40)
    s.stop('pdfdlg')
    s.K(RET)
    s.W(60)
    s.L(6, m.state('Pages.cwr', 3))
    pdfs.append({'path': 'DOCUMENTS/Pages.pdf', 'pages': 3, 'size': [612, 792],
                 'texts': ['Page one of a document in pages.', POLISH.replace('\r', ' '),
                           'Page three, after Ctrl+Return.'],
                 'on': [[2, 'Veranda Write'], [2, 'zażółć'], [2, 'Pages'], [2, 'Page 2 of 3'], [3, 'Page 3 of 3'],
                        [2, 'Page two, after a page break from the menu.']],
                 'off': [[1, 'Veranda Write'], [1, 'Page 1 of 3']]})
    # older documents: 1.1 (paragraph styles), 1.2 (a page break), 1.3 (Polish)
    d1 = [(ord(c), SANS, 12, 0) for c in 'A heading\rWritten by 1.1.\r']
    p1 = [NAMED[1], BODY, BODY]
    cwrd.write(os.path.join(run, 'OLD11.cwr'), d1, paras=p1, minor=1)
    d2 = [(ord(c), SANS, 12, 0) for c in 'Page one, 1.2.\r\rPage two.']
    cwrd.write(os.path.join(run, 'OLD12.cwr'), d2, minor=2, breaks=(15,))
    d3 = [(c, SANS, 12, 0) for c in POLISH.encode('iso8859_16')]
    cwrd.write(os.path.join(run, 'OLD13.cwr'), d3, minor=3)
    for k, (d, pp, nm, pg) in enumerate(((d1, p1, 'OLD11.cwr', 1), (d2, None, 'OLD12.cwr', 2),
                                         (d3, None, 'OLD13.cwr', 1))):
        s.K(ord('o'), CTRL)
        s.W(40)
        for _ in range(k + 1):
            s.K(ord('O'))
            s.W(6)
        s.K(RET)
        s.W(60)
        s.L(7 + k, Model(d, pp).state(nm, pg))
    s.stop('old')
    # the big document: its pages, a PDF of them (timed), one stopped
    big = cwrd.big_chars(int(os.environ.get('BIG_SIZE', '300000')))
    cwrd.write(os.path.join(run, 'XBIG.cwr'), big, minor=3)
    b = Model(big)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('X'))
    s.W(10)
    s.Z()
    s.K(RET)
    s.W(30)
    s.Y('l')
    s.L(10, b.state('XBIG.cwr'))
    s.K(ord('p'), CTRL | SHIFT)
    s.W(40)
    s.Z()
    s.K(RET)
    s.W(30)
    s.Y('p')
    s.L(11, b.state('XBIG.cwr'))
    pdfs.append({'path': 'DOCUMENTS/XBIG.pdf', 'pages_state': 11, 'size': [595.28, 841.89],
                 'texts': [' '.join(''.join(chr(c[0]) for c in big[:200]).split()[:6]),
                           ' '.join(''.join(chr(c[0]) for c in big[-300:]).split()[-6:])]})
    s.K(ord('p'), CTRL | SHIFT)                 # another, stopped by Esc
    s.W(40)
    for _ in range(len('XBIG.pdf')):
        s.K(BS)
    s.T('Stop.pdf')
    s.b += bytes([ord('A'), RET, ESC, 240])     # Return, then Esc 4 seconds on, while it writes
    s.W(60)
    s.L(12, b.state('XBIG.cwr'))
    s.K(ord('g'), CTRL | SHIFT)                 # Go to page 5: where the PDF's page 5 starts
    s.W(30)
    s.T('5')
    s.W(10)
    s.stop('goto')
    s.K(RET)
    s.W(30)
    s.L(13, {'name': 'XBIG.cwr'})
    s.stop('bigpages')
    s.stop('end')
    s.pdfs = pdfs
    s.absent = ['DOCUMENTS/Stop.pdf']
    s.console = ['CW PDF 0', 'CW PDF 3']
    s.pagestart = {'state': 13, 'doc': 'XBIG.cwr', 'pdf': 'DOCUMENTS/XBIG.pdf', 'page': 5}
    s.pagefile = {'Pages.cwr': {'page': page, 'pbreaks': 2}}
    s.hfdate = True                             # (the header's {date} drawn whole, inside the text)
    return s, files


def session_office(run):
    """stage 4 (SESSION=office; W49-W54): a Word document (Report.docx,
    tools/office_fix.py's: every feature, Polish, a table, a picture, a
    note, a comment, tracked changes, a header and a footer, Letter)
    opened (the conversion's note), exported as Word and as OpenDocument
    into /DOCUMENTS; an OpenDocument text (Notes.odt) opened; a big Word
    document (Big report.docx: OX_BIG bytes of document.xml, 600,000 by
    default) opened and exported both ways, timed. Each state against the
    host's conversion of the same file (build/host/test_cw ox: the same
    code); each exported file read back on the host (the same document,
    its page setup) and checked as other programs read it
    (tools/check_docx.py: its ZIP, its XML, LibreOffice's text)"""
    import subprocess
    import office_fix
    s = Script()
    files = {}
    ref = os.path.join('build', 'office-ref')
    os.makedirs(ref, exist_ok=True)
    fx = office_fix.docx_features(os.path.join(run, 'Report.docx'))['counts']
    fo = office_fix.odt_features(os.path.join(run, 'Notes.odt'))['counts']

    def note(kind, c):
        """the test build's log of the conversion's counts (src/v/oxnote.c)"""
        return 'CW NOTE %d %s' % (kind, ' '.join('%d' % c.get(k, 0) for k in
                                                 ('tables', 'pics', 'boxes', 'notes', 'comments', 'changes', 'fields',
                                                  'lost')))
    office_fix.docx_big(os.path.join(run, 'Big report.docx'), int(os.environ.get('OX_BIG', '600000')))

    def model(name):
        out = os.path.join(ref, name + '.cwr')
        r = subprocess.run([os.environ.get('TEST_CW', 'build/host/test_cw'), 'ox', os.path.join(run, name), out],
                           capture_output=True)
        if r.returncode:
            sys.exit('script.py: the host does not convert %s' % name)
        d = cwrd.read(out)
        m = Model(d.chars(), d.paras())
        m.mod = True
        return m, d

    def opn(letter, name, note, discard=True, shot=None):
        s.K(ord('o'), CTRL)
        s.W(40)
        if discard:
            s.click(*NO_BUTTON)                 # (the changes to the converted document: not saved)
            s.W(40)
        s.K(ord(letter))
        s.W(10)
        s.Z()
        s.K(RET)
        s.W(60)
        s.Y(note)
        if shot:
            s.stop(shot)

    def export(item, t, n, m, shot=None):
        s.menu(FILEM, item)                     # File > Export as Word, as OpenDocument
        s.W(40)
        if shot:
            s.stop(shot)
        s.Z()
        s.K(RET)
        s.W(60)
        s.Y(t)
        s.L(n, m.state(''))
    s.W(90)
    m1, d1 = model('Report.docx')
    opn('R', 'Report.docx', 'd', discard=False, shot='oxnote')
    s.K(RET)                                    # (the note's box: OK)
    s.W(30)
    s.L(1, m1.state(''))
    s.stop('oxdoc')
    export(6, 'w', 2, m1, shot='oxsave')
    export(7, 'v', 3, m1)
    m2, d2 = model('Notes.odt')
    opn('N', 'Notes.odt', 'o', shot='oxodt')
    s.K(RET)
    s.W(30)
    s.L(4, m2.state(''))
    m3, d3 = model('Big report.docx')
    opn('B', 'Big report.docx', 'b')            # (no note: nothing in it changed)
    s.L(5, m3.state(''))
    export(6, 'x', 6, m3)
    export(7, 'y', 7, m3)
    s.stop('end')
    s.office = [{'path': 'DOCUMENTS/Report.docx', 'state': 2, 'page': d1.page},
                {'path': 'DOCUMENTS/Report.odt', 'state': 3, 'page': d1.page},
                {'path': 'DOCUMENTS/Big report.docx', 'state': 6, 'page': d3.page},
                {'path': 'DOCUMENTS/Big report.odt', 'state': 7, 'page': d3.page}]
    s.console = ['CW PDF 0', note(1, fx), note(2, fo), note(1, {})]
    return s, files


def session_redraw(run):
    """the partial redraw against a full one (SESSION=redraw; W55): after
    each change of the layout that changes no text, and after typing, the
    text area as the edits drew it must be what a full drawing draws
    ('L' n + $80: src/v/test.c's "CW REDRAW n rows first last"). The case
    found on the X16 first (a Polish paragraph made Heading 1, then Body:
    the large lines left under the small ones), then a paragraph in the
    middle (the rows below moving up and down, the bottom's rows freed)
    and one scrolled partly off the top, each by: the Style menu, the
    toolbar's pop-up, Larger and Smaller, the Size and Font menus, Bold,
    Italic, Underline, alignment, indents, line spacing, space after, a
    list on and off, the page setup's margin, undo and redo; with and
    without a selection"""
    s = Script()
    s.W(90)
    n = [0]
    chars = [0]

    def V():
        n[0] += 1
        s.stop('v%d' % n[0])                    # (a screenshot before each check: --stop v12)
        if os.environ.get('NOCHECK'):
            return
        s.L(0x80 | n[0], None)
        s.states.pop()                          # (not a state: test.c's redraw check)
        s.redraws.append(n[0])
    s.redraws = []

    def typ(t):
        b = t.encode('iso8859_16')
        s.T(b)
        chars[0] += len(b)
    pl = 'W niedalekim czasie dołożyli do szybkiego stołu parę liści które mogą być'
    # the X16's case: typed, selected, Heading 1, Body, at the document's end
    typ(pl)
    V()
    s.K(ord('a'), CTRL)                         # selected
    V()
    s.menu(STYLE, 1)                            # Heading 1: 4 lines of 27 points
    s.stop('rdhead')
    V()
    s.menu(STYLE, 0)                            # Body again
    s.stop('rdbug')
    V()
    s.menu(STYLE, 2)                            # Heading 2, Heading 1, Body: grows and shrinks
    V()
    s.menu(STYLE, 1)
    V()
    s.menu(STYLE, 0)
    V()
    s.K(ord('z'), CTRL)                         # undone, redone
    V()
    s.K(ord('y'), CTRL)
    V()
    # text after it: the paragraph in the middle, the rows below moving
    s.K(CTRL_END, CTRL)
    typ('\r' + pl + ' Its second paragraph, in Body.')
    for i in range(14):
        typ('\rRow %d of the filler.' % i)
    typ('\r' + pl + ' The paragraph scrolled off the top.')
    for i in range(26):
        typ('\rLine %d under it.' % i)
    V()
    s.K(CTRL_HOME, CTRL)                        # the first paragraph, no selection
    V()
    for k in (1, 0, 3, 2, 0):                   # Heading 1, Body, Heading 3, Heading 2, Body
        s.menu(STYLE, k)
        V()
    s.popup(TB_NAME + 20, 1)                    # the toolbar's pop-up: Heading 1, Body
    V()
    s.popup(TB_NAME + 20, 0)
    V()
    s.menu(STYLE, 5)                            # a bulleted list, Body
    V()
    s.menu(STYLE, 0)
    V()
    for k in ((ord('c'), CTRL | SHIFT), (ord('r'), CTRL | SHIFT), (ord('j'), CTRL), (ord('l'), CTRL)):
        s.K(*k)                                 # centred, right, justified, left
        V()
    for item in (4, 4, 5, 5, 8, 7, 6, 9, 9):    # indents, spacing, space after
        s.menu(FORMAT, item)
        V()
    # a selection: the first paragraph's first two lines (Shift+Down)
    s.M(SHIFT)
    s.K(KEY_DOWN)
    s.K(KEY_DOWN)
    s.M(0)
    V()
    s.K(ord('e'), CTRL)                         # Larger, Larger, Smaller, Smaller
    V()
    s.K(ord('e'), CTRL)
    V()
    s.K(ord('e'), CTRL | SHIFT)
    V()
    s.K(ord('e'), CTRL | SHIFT)
    V()
    s.menu(SIZE, 3)                             # Size > 27, Size > 9, Size > 12
    V()
    s.menu(SIZE, 0)
    V()
    s.menu(SIZE, 1)
    V()
    s.menu(FONT, 1)                             # Gate Mono, Gate Sans
    V()
    s.menu(FONT, 2)
    V()
    for k in ('b', 'i', 'u', 'b', 'i', 'u'):    # bold, italic, underline on and off
        s.K(ord(k), CTRL)
        V()
    s.menu(STYLE, 1)                            # with the selection: Heading 1, Body
    V()
    s.menu(STYLE, 0)
    V()
    for _ in range(4):                          # undone and redone
        s.K(ord('z'), CTRL)
        V()
    for _ in range(4):
        s.K(ord('y'), CTRL)
        V()
    # the third long paragraph partly off the top: its last line the top row
    s.K(CTRL_END, CTRL)
    V()
    for _ in range(27):
        s.K(KEY_UP)
    s.W(10)
    s.L(2, {'chars': chars[0]})
    s.stop('rdtop')
    V()
    s.stop('rdtopv')
    for k in (1, 0, 2, 0):                      # Heading 1, Body, Heading 2, Body
        s.menu(STYLE, k)
        if k == 1 and 'rdtop1' not in s.names:
            s.stop('rdtop1')
        V()
    s.menu(FORMAT, 4)                           # indented, back
    V()
    s.menu(FORMAT, 5)
    V()
    s.menu(FORMAT, 8)                           # double spacing, single
    V()
    s.menu(FORMAT, 6)
    V()
    s.K(ord('z'), CTRL)
    V()
    s.K(ord('y'), CTRL)
    V()
    # the page setup: the left margin 2 cm more, then back (every line wrapped again)
    s.K(CTRL_HOME, CTRL)
    for d in (KEY_RIGHT, KEY_LEFT):
        s.menu(FILEM, 4)
        s.W(20)
        s.K(KEY_DOWN)
        s.K(KEY_DOWN)
        s.K(KEY_DOWN)
        for _ in range(20):
            s.K(d)
        s.K(RET)
        s.W(30)
        V()
    # a footer (drawn under the document's last line): typing at the end
    # wraps a line over it; the last paragraph grows and shrinks over it
    s.menu(INSERT, 2)                           # Insert > Header and footer...
    s.W(20)
    s.K(TAB)
    s.T('Page {page} of {pages}')
    s.K(RET)
    s.W(30)
    V()
    s.K(CTRL_END, CTRL)
    typ(' and more words to wrap onto a line of its own')
    V()
    for k in (1, 0, 3, 0):
        s.menu(STYLE, k)
        V()
    s.K(ord('a'), CTRL)                         # all of it: Heading 2, Body, undone, redone
    s.menu(STYLE, 2)
    V()
    s.menu(STYLE, 0)
    V()
    s.K(ord('z'), CTRL)
    V()
    s.K(ord('y'), CTRL)
    V()
    s.L(1, {'chars': chars[0]})
    s.stop('rdend')
    return s, {}


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    run = a[0]
    stop = a[a.index('--stop') + 1] if '--stop' in a else None
    s, files = {'big': session_big, 'speed': session_speed, 'page': session_page,
                 'office': session_office, 'redraw': session_redraw}.get(os.environ.get('SESSION'), session)(run)
    b = bytes(s.b)
    if stop:
        b = b[:s.names[stop]]
    b += b'S'
    if len(b) > 4096:
        sys.exit('script.py: the session is %d bytes (4096 at most: the test overlay has the rest of its bank)' % len(b))
    open(os.path.join(run, 'CW.PLY'), 'wb').write(b)
    def fexp(v):
        if isinstance(v, dict):
            return {'sha256': hashlib.sha256(v['raw']).hexdigest()} if 'raw' in v else v
        return {'sum': cwrd.doc_sum(v[0]), 'chars': len(v[0]), 'psum': cwrd.par_sum(v[1])}
    json.dump({'states': s.states, 'files': {k: fexp(v) for k, v in files.items()},
               'stops': list(s.names), 'labels': getattr(s, 'labels', {}), 'lossy': getattr(s, 'lossy', {}),
               'pdfs': getattr(s, 'pdfs', []), 'absent': getattr(s, 'absent', []),
               'console': getattr(s, 'console', []), 'pagestart': getattr(s, 'pagestart', None),
               'pagefile': getattr(s, 'pagefile', {}), 'office': getattr(s, 'office', []),
               'hfdate': getattr(s, 'hfdate', False), 'redraws': getattr(s, 'redraws', []),
               **({'SESSION': os.environ['SESSION']} if os.environ.get('SESSION') in ('big', 'speed', 'page', 'office', 'redraw')
                  else {})},
              open(os.path.join(run, 'expect.json'), 'w'), indent=1)

if __name__ == '__main__':
    main()
