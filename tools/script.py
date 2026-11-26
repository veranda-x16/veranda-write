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
APPM, FILEM, EDIT, INSERT, TEXT, STYLE, FORMAT, FONT, SIZE, TOOLS, TABLE = range(11)
# the File menu's items (1.6: New from template and Save as template among them)
F_NEW, F_NTEMPL, F_OPEN, F_SAVE, F_SAVEAS, F_STEMPL, F_PAGE, F_PDF, F_DOCX, F_ODT, F_CLOSE, F_QUIT = range(12)


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


F7, F8 = 0x88, 0x8C                     # (the ROM's codes: Veranda's KEY_F7, KEY_F8)


def spell_text(lang, n, seed):
    """about n words of running text in the language, drawn by frequency
    (Polish: Writer's FrequencyWords list, the words PL.DIC has; English:
    SCOWL's commonest lists), sentences of 6-14 words, paragraphs of 5-9
    sentences, and an unknown word at its end (Check spelling reads it all)"""
    import random
    import build_dictionary as bd
    rnd = random.Random(seed)
    dic = open('dict/%s.DIC' % lang.upper(), 'rb').read()
    words, weights = [], []
    if lang == 'pl':
        src = os.path.expanduser(os.environ.get('PL_FREQ', '~/Projects/Writer/dict-src/pl_full.txt'))
        for line in open(src, encoding='utf-8'):
            w, c = line.split()
            try:
                b = w.encode('iso8859_16')
            except UnicodeEncodeError:
                continue
            if len(w) > 1 and w.isalpha() and bd.lookup(dic, b):
                words.append(w)
                weights.append(int(c))
            if len(words) >= 6000:
                break
    else:
        f = 'build/dsrc/scowl/scowl-2020.12.07/final/english-words.'
        for k in ('10', '20'):
            for line in open(f + k, encoding='latin-1'):
                w = line.strip()
                if len(w) > 1 and w.isalpha() and w.islower() and bd.lookup(dic, w.encode('iso8859_16')):
                    words.append(w)
        rnd.shuffle(words)
        weights = [1.0 / (i + 10) for i in range(len(words))]
    out, k = [], 0
    while k < n:
        para = []
        for _ in range(rnd.randint(5, 9)):
            m = rnd.randint(6, 14)
            sw = rnd.choices(words, weights, k=m)
            sw[0] = sw[0][0].upper() + sw[0][1:]
            para.append(' '.join(sw) + '.')
            k += m
        out.append(' '.join(para))
    return '\r'.join(out) + ' Zzzqx.', k + 1


def session_spell(run):
    """spelling (SESSION=spell; W56-W59), under R49 at 512 KB on a host
    folder and an SD card image: a Polish document with words the
    dictionary has not, live marks (a screenshot), Check spelling (F7)
    from its start: Change (Return) to the first suggestion, Ignore,
    Ignore all, Add (the user's words' file checked), the end; saved; an
    English document: a right click on a word and its pop-up, the Tools
    menu's languages, Change all, put aside (Alt+Tab) and back, F8 on a
    word typed; the lookups timed ('L' $7F: tools/check.py prints them);
    Check spelling over 2,000 words of Polish and of English as Writer
    did it (no hash, a 16 KB cache), from cold and again; typing timed
    with the marks on"""
    import subprocess
    s = Script()
    files = {}
    st = [0]

    def chars(t):
        return [(b, SANS, 12, 0) for b in t.encode('iso8859_16')]

    def text(m):
        return bytes(c[0] for c in m.c).decode('iso8859_16')

    def first(words, lang):
        r = subprocess.run([os.environ.get('TEST_CW', 'build/host/test_cw')], capture_output=True,
                           env=dict(os.environ, CW_SUGG=' '.join(w.lower() for w in words), CW_LANG=lang))
        out = {}
        for line in r.stdout.decode('utf-8').splitlines():
            f = line.split('\t')
            out[f[0]] = f[2] if len(f) > 2 else None
        return out

    def cased(w, sg):
        if len(w) > 1 and w.upper() == w:
            return sg.upper()
        return sg[0].upper() + sg[1:] if w[0].isupper() else sg

    def rep(m, old, new, at=0, every=False):
        """old replaced by new (the first character's style) from at"""
        t = text(m)
        while True:
            i = t.find(old, at)
            if i < 0:
                return
            sty = m.c[i][1:]
            m.c[i:i + len(old)] = [(b,) + sty for b in new.encode('iso8859_16')]
            m.mod = True
            t = text(m)
            at = i + len(new)
            if not every:
                return

    def state(m, name, label=None):
        st[0] += 1
        s.L(st[0], m.state(name))
        if label:
            s.labels[st[0]] = label
    s.labels = {}
    s.spell = []

    def found(word, first_s=None):
        s.spell.append([1, word, first_s])

    def done():
        s.spell.append([0, '', None])

    def opn(letter):
        s.K(ord('o'), CTRL)
        s.W(40)
        s.K(ord(letter))
        s.W(10)
        s.K(RET)
        s.W(90)

    PL = ('Ksiazka leży na stole, a kot pije mleko.\rTo jest mój komputre i mój stół.\r'
          'Prosze, daj mi ten zolw teraz.\rŁadny dzień, Spoczynski był tu.\r'
          'Biało-czerwony NATO i ABCDEFG, ABCDEFG.')
    EN = ("Thsi letter: I recieve teh letters, then teh cat sat on teh mat.\r"
          "Seperate them, please. Don't worry: the well-known children's book is mine.")
    sg_pl = first(['ksiazka', 'komputre', 'zolw'], 'pl')
    sg_en = first(['thsi', 'recieve', 'teh', 'seperate', 'wrold'], 'en')
    cwrd.write(os.path.join(run, 'Polski.cwr'), chars(PL), minor=5, lang=2)
    cwrd.write(os.path.join(run, 'English.cwr'), chars(EN), minor=5, lang=1)
    kpl, npl = spell_text('pl', int(os.environ.get('SPELL_WORDS', '2000')), 3)
    ken, nen = spell_text('en', int(os.environ.get('SPELL_WORDS', '2000')), 5)
    cwrd.write(os.path.join(run, 'Kpl.cwr'), chars(kpl), minor=5, lang=2)
    cwrd.write(os.path.join(run, 'Words.cwr'), chars(ken), minor=5, lang=1)
    s.words = {'Kpl.cwr': npl, 'Words.cwr': nen}

    s.console = ['CW DESK RESTART', 'CW KEYBOARD POLISH', 'CW KEYBOARD NOT POLISH']
    cn = [0]

    def cfg(c, live, loaded, marks, lang):
        """(W60) 'L' $7B: "CW SPCFG n settings live loaded marks language"
        (src/v/ovlclr.c)"""
        cn[0] += 1
        s.b += bytes([ord('L'), 0x7B])
        s.W(30)
        f = [cn[0], c, live, loaded] + ([marks, lang] if marks is not None else [])
        s.console.append('CW SPCFG ' + ' '.join('%02X' % v for v in f))

    s.W(90)
    # (W60) no settings file and Veranda's keyboard setting Polish ('L' $7A
    # turns PREFS_POLISH on and off): the new document is English
    s.b += bytes([ord('L'), 0x7A])
    s.W(30)
    s.K(F7)
    s.W(90)
    done()                                      # (an empty document: nothing to check)
    s.K(ESC)
    s.W(30)
    cfg(0x01, 1, 1, 0, 1)
    s.b += bytes([ord('L'), 0x7A])
    s.W(30)
    # the Polish document: its marks, then Check spelling from the start
    m = Model(chars(PL))
    opn('P')
    state(m, 'Polski.cwr')
    if os.environ.get('SPELL_QUICK'):           # (a quick look: the lookups timed, F7)
        s.L(0x7F, None)
        s.states.pop()
        s.W(30)
        s.K(F7)
        s.W(90)
        s.stop('end')
        return s, files
    s.W(240)
    s.stop('splivepl')
    s.K(F7)
    s.W(90)
    found('Ksiazka', cased('Ksiazka', sg_pl['ksiazka']))
    s.K(RET)                                    # Change: the first suggestion, in its case
    rep(m, 'Ksiazka', cased('Ksiazka', sg_pl['ksiazka']))
    s.W(60)
    found('komputre', sg_pl['komputre'])
    s.K(RET)
    rep(m, 'komputre', sg_pl['komputre'])
    s.W(60)
    found('Prosze')
    s.K(ord('i'), ALT)                          # Ignore
    s.W(60)
    found('zolw', sg_pl['zolw'])
    s.W(30)
    s.stop('spdlg')                             # (the panel with Polish suggestions)
    s.K(RET)
    rep(m, 'zolw', sg_pl['zolw'])
    s.W(60)
    found('Spoczynski')
    s.K(ord('a'), ALT)                          # Add: the user's words
    s.W(90)
    found('ABCDEFG')
    s.K(ord('g'), ALT)                          # Ignore all: the second one too
    s.W(60)
    done()
    s.stop('spdone')
    s.K(ESC)
    s.W(30)
    state(m, 'Polski.cwr')
    s.K(ord('s'), CTRL)
    s.W(60)
    m.mod = False
    state(m, 'Polski.cwr')
    files['Polski.cwr'] = (list(m.c), list(m.p))
    files['VERANDA/.SYSTEM/WRITE.UDIC'] = {'raw': b'spoczynski\n'}
    # the English document: a right click on its first word, the pop-up
    e = Model(chars(EN))
    opn('E')
    state(e, 'English.cwr')
    s.W(240)
    s.stop('spliveen')
    s.P(76, 78)
    s.b += b'G'
    s.W(4)
    s.b += b'U'
    s.W(40)
    s.stop('sppop')
    s.K(ord('1'))
    rep(e, 'Thsi', cased('Thsi', sg_en['thsi']))
    s.W(60)
    state(e, 'English.cwr')
    s.menu(TOOLS, 3, shot='splang')             # (English: already the document's)
    s.W(30)
    s.K(CTRL_HOME, CTRL)
    s.K(F7)
    s.W(90)
    found('recieve', sg_en['recieve'])
    s.K(RET)
    rep(e, 'recieve', sg_en['recieve'])
    s.W(60)
    found('teh', sg_en['teh'])
    s.K(ord('l'), ALT)                          # Change all: the three of them, one step
    rep(e, 'teh', sg_en['teh'], every=True)
    s.W(90)
    found('Seperate', cased('Seperate', sg_en['seperate']))
    s.K(RET)
    rep(e, 'Seperate', cased('Seperate', sg_en['seperate']))
    s.W(60)
    done()
    s.K(ESC)
    s.W(30)
    state(e, 'English.cwr')
    s.K(ord('z'), CTRL)                         # (Change all undone and redone: one step)
    s.W(30)
    s.K(ord('y'), CTRL)
    s.W(30)
    state(e, 'English.cwr')
    # put aside with the dictionary's banks, back; Check spelling again
    s.K(TAB, ALT)
    s.W(120)
    state(e, 'English.cwr')
    s.K(CTRL_HOME, CTRL)
    s.K(F7)
    s.W(90)
    done()
    s.K(ESC)
    s.W(20)
    # a word typed, F8: its pop-up, the first suggestion
    s.K(CTRL_END, CTRL)
    e.move(len(e.c))
    s.T(' Wrold')
    e.type(' Wrold', step=False)
    s.K(KEY_LEFT)
    s.W(200)
    s.K(F8)
    s.W(40)
    s.K(ord('1'))
    rep(e, 'Wrold', cased('Wrold', sg_en['wrold']))
    s.W(60)
    state(e, 'English.cwr')
    s.K(ord('s'), CTRL)
    s.W(60)
    e.mod = False
    state(e, 'English.cwr')
    files['English.cwr'] = (list(e.c), list(e.p))
    # the lookups timed
    s.L(0x7F, None)
    s.states.pop()
    s.W(30)
    # Check spelling over 2,000 words: as Writer did it, from cold, again
    for name, letter in (('Kpl.cwr', 'K'), ('Words.cwr', 'W')):
        k = Model(chars(kpl if letter == 'K' else ken))
        opn(letter)
        state(k, name)
        s.W(120)
        for mode in (0x7E, 0x7D, None):
            if mode:
                s.L(mode, None)
                s.states.pop()
                s.W(20)
            s.K(CTRL_HOME, CTRL)
            s.K(F7)
            s.W(60)
            found('Zzzqx')
            s.K(ESC)
            s.W(20)
        if letter == 'W':
            # typing at the end, the marks on (timed: 'Q')
            s.K(CTRL_END, CTRL)
            k.move(len(k.c))
            s.W(20)
            for ch in ' I recieve the letter, teh end':
                s.b += bytes([ord('Q'), ord(ch)])
                k.type(ch, step=False)
            s.W(120)
            state(k, name, 'typing at the end of 2,000 words, the marks on')
    # (W60) the settings, kept in VERANDA/.SYSTEM/WRITE.CFG: Tools > Mark
    # misspellings off (kept at once), Tools > Spelling options: the
    # language of new documents Polish (a screenshot); the program quits and
    # starts again (the session's second part, CW.PL2: src/v/test.c,
    # tools/desk): the settings as they were; typing: no marks, spelling's
    # overlays not loaded; F7 on demand, in Polish; the options: spelling
    # only when asked. "CW SPCFG n settings live loaded marks language"
    s.menu(TOOLS, 2)                            # Mark misspellings: off
    s.W(30)
    cfg(0x05, 0, 1, None, None)                 # (the rows keep the counts they were last drawn with)
    s.menu(TOOLS, 5)                            # Spelling options
    s.W(30)
    s.K(TAB)
    s.K(TAB)
    s.K(KEY_RIGHT)                              # (English, Polish, the keyboard's)
    s.W(20)
    s.stop('spopts')
    s.K(RET)
    s.W(30)
    cfg(0x06, 0, 1, None, None)
    s.b += bytes([ord('L'), 0x7C])              # quit, started again (src/v/test.c)
    s.W(255)
    s.split = len(s.b)
    cn[0] = 0                                   # (the program's count starts again)
    s.W(120)
    cfg(0x06, 0, 0, 0, 0)                       # (a new document: no language yet)
    s.T('Ksiazka i komputre.')
    s.W(240)
    s.stop('spnomark')
    cfg(0x06, 0, 0, 0, 0)                       # typed, a pause: no marks (overlay 23 draws them: not loaded)
    if not os.environ.get('CARD'):
        # (on the host folder the part ends here: the emulator's host file
        # system then refuses the swap's first file, which loading spelling
        # takes a bank for after typing (the swap's folder removed by Quit
        # and made again); the SD card image's run goes on)
        files['VERANDA/.SYSTEM/WRITE.CFG'] = {'raw': b'WCFG\x01\x01\x02'}
        s.stop('end')
        return s, files
    s.K(CTRL_HOME, CTRL)
    s.K(F7)
    s.W(90)
    found('Ksiazka', cased('Ksiazka', sg_pl['ksiazka']))   # (Polish: the settings')
    s.K(ESC)
    s.W(30)
    cfg(0x06, 0, 1, 0, 2)
    s.menu(TOOLS, 5)
    s.W(30)
    s.K(ord(' '))                               # Check spelling as you work: off
    s.W(20)
    s.stop('spopts2')
    s.K(RET)
    s.W(30)
    cfg(0x0E, 0, 1, 0, 2)
    files['VERANDA/.SYSTEM/WRITE.CFG'] = {'raw': b'WCFG\x01\x03\x02'}
    s.stop('end')
    return s, files


def session_nodict(run):
    """(W61) spelling without the dictionaries (SESSION=nodict, NODICT=1:
    only SPELL.OVL in /VERANDA/DICT, as Veranda's package has it now):
    English typed, a pause: live checking finds no dictionary and stays
    quiet (no box, no line: the keys after it go to the text);
    Check spelling (F7): the panel says where the dictionaries come from
    ("CW SPELL 03"); more typed, a pause: nothing said; Tools > Get
    dictionaries: asked (Yes), START.REQ and UPDATE.REQ written in
    /VERANDA/.SYSTEM (their bytes checked), Write put aside (the test
    desktop brings it back); then English arrives while Write runs
    (DICTNEW: EN.NEW renamed EN.DIC, 'L' $79) and F7 finds the first word
    the dictionary has not"""
    s = Script()
    files = {}
    s.spell = []
    s.W(90)
    m = Model()

    def typ(t):
        s.T(t)
        m.type(t)
    typ('Thsi is teh text. ')
    s.W(240)                                    # (a pause: live checking, no dictionary)
    s.L(1, m.state(''))
    s.K(F7)
    s.W(60)
    s.spell.append([3, '', None])               # (no dictionary: the panel says where they come from)
    s.stop('ndpanel')
    s.K(ESC)
    s.W(20)
    typ('More teh words. ')
    s.W(240)                                    # (nothing said again: no box took the keys)
    s.L(2, m.state(''))
    s.menu(TOOLS, 6)                            # Get dictionaries...
    s.W(30)
    s.stop('ndask')
    s.click(*YES_BUTTON)                        # Yes: the requests, put aside, back
    s.W(240)
    s.L(3, m.state(''))
    files['VERANDA/.SYSTEM/UPDATE.REQ'] = {'raw': b'dict\r\nback Veranda Write\r\n'}
    files['VERANDA/.SYSTEM/START.REQ'] = {'raw': b'/VERANDA/APPS/Veranda Update\r\n'}
    s.b += bytes([ord('L'), 0x79])              # English arrives (src/v/test.c)
    s.W(30)
    s.K(CTRL_HOME, CTRL)
    s.K(F7)
    s.W(120)
    s.spell.append([1, 'Thsi', None])           # (found now: looked for again)
    s.stop('ndfound')
    s.K(ESC)
    s.W(20)
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
    s.menu(FILEM, F_PAGE)                            # File > Page setup...
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
    ref = os.path.join('build', 'office-ref', os.path.basename(os.path.normpath(run)))   # (one a run: the gate's screenshots run four at once)
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
    export(F_DOCX, 'w', 2, m1, shot='oxsave')
    export(F_ODT, 'v', 3, m1)
    m2, d2 = model('Notes.odt')
    opn('N', 'Notes.odt', 'o', shot='oxodt')
    s.K(RET)
    s.W(30)
    s.L(4, m2.state(''))
    m3, d3 = model('Big report.docx')
    opn('B', 'Big report.docx', 'b')            # (no note: nothing in it changed)
    s.L(5, m3.state(''))
    export(F_DOCX, 'x', 6, m3)
    export(F_ODT, 'y', 7, m3)
    s.stop('end')
    s.office = [{'path': 'DOCUMENTS/Report.docx', 'state': 2, 'page': d1.page},
                {'path': 'DOCUMENTS/Report.odt', 'state': 3, 'page': d1.page},
                {'path': 'DOCUMENTS/Big report.docx', 'state': 6, 'page': d3.page},
                {'path': 'DOCUMENTS/Big report.odt', 'state': 7, 'page': d3.page}]
    s.console = ['CW PDF 0', note(1, fx), note(2, fo), note(1, {})]
    return s, files


def session_oxspeed(run):
    """(W71, may be renumbered) opening .docx and .odt documents timed
    (SESSION=oxspeed, under R49 at 512 KB, on a host folder and an SD card
    image): the user's kind of document, made by LibreOffice from HTML
    (tests/office/, made by tools/md2html.py and soffice: headings, lists,
    tables, LibreOffice's verbose XML), "Veranda Desktop Guide.docx" (its
    .odt as Guide.odt) and "Writing and Asking on Veranda.docx", then
    Report.docx and Notes.odt (tools/office_fix.py's), Big report.docx
    (600 KB of document.xml), tools/oxstress.py's Stress.docx, the Guide
    with fixed codes and in stored blocks; the Guide exported as Word and
    as OpenDocument. Each state and note against the host's conversion of
    the same file; the times ("TIME c": tools/check.py) and the import's phases
    ("CW MARK": odocx.c's, oemit.c's)"""
    import shutil
    import subprocess
    import office_fix
    s = Script()
    ref = os.path.join('build', 'office-ref', os.path.basename(os.path.normpath(run)))
    os.makedirs(ref, exist_ok=True)
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tests', 'office')
    shutil.copy(os.path.join(src, 'Veranda Desktop Guide.docx'), run)
    shutil.copy(os.path.join(src, 'Writing and Asking on Veranda.docx'), run)
    shutil.copy(os.path.join(src, 'Veranda Desktop Guide.odt'), os.path.join(run, 'Guide.odt'))
    office_fix.docx_features(os.path.join(run, 'Report.docx'))
    office_fix.odt_features(os.path.join(run, 'Notes.odt'))
    office_fix.docx_big(os.path.join(run, 'Big report.docx'), int(os.environ.get('OX_BIG', '600000')))
    import oxstress
    oxstress.stress(os.path.join(run, 'Stress.docx'))
    oxstress.rezip(oxstress.GUIDE, os.path.join(run, 'Fixed codes.docx'), 'fixed')
    oxstress.rezip(oxstress.GUIDE, os.path.join(run, 'Kept in blocks.docx'), 'stored')
    notes = []

    def model(name):
        out = os.path.join(ref, name + '.cwr')
        r = subprocess.run([os.environ.get('TEST_CW', 'build/host/test_cw'), 'ox', os.path.join(run, name), out],
                           capture_output=True, text=True)
        if r.returncode:
            sys.exit('script.py: the host does not convert %s' % name)
        # (the X16's note: v/oxnote.c's "CW NOTE", the host's counts)
        import re
        c = {k: int(v) for k, v in re.findall(r'(\w+) (\d+)', r.stdout.split(';')[-1])}
        notes.append('CW NOTE %d %s' % (1 if name.endswith('.docx') else 2,
                                        ' '.join('%d' % c.get(k, 0) for k in ('tables', 'pictures', 'boxes', 'notes',
                                                                              'comments', 'changes', 'fields', 'lost'))))
        d = cwrd.read(out)
        m = Model(d.chars(), d.paras())
        m.mod = True
        return m, d

    def opn(letter, t, n, name, discard=True, box=False):
        m, d = model(name)
        s.K(ord('o'), CTRL)
        s.W(40)
        if discard:
            s.click(*NO_BUTTON)
            s.W(40)
        s.K(ord(letter))
        s.W(10)
        s.Z()
        s.K(RET)
        s.W(60)
        s.Y(t)
        if box:
            s.K(RET)                            # (the note's box: OK)
            s.W(30)
        s.L(n, m.state(''))
        return m, d

    def export(item, t, n, m):
        s.menu(FILEM, item)
        s.W(40)
        s.Z()
        s.K(RET)
        s.W(60)
        s.Y(t)
        s.L(n, m.state(''))
    s.W(90)
    one = os.environ.get('OXONLY')              # (a profile: only that file opened, then the end)
    if one:
        name = {'V': 'Veranda Desktop Guide.docx', 'W': 'Writing and Asking on Veranda.docx', 'G': 'Guide.odt',
                'R': 'Report.docx', 'N': 'Notes.odt', 'B': 'Big report.docx', 'S': 'Stress.docx',
                'F': 'Fixed codes.docx', 'K': 'Kept in blocks.docx'}[one]
        opn(one, 'P', 1, name, discard=False, box=one in 'RN')
        s.stop('end')
        s.console = []
        return s, {}
    opn('W', 'A', 1, 'Writing and Asking on Veranda.docx', discard=False)
    opn('G', 'O', 2, 'Guide.odt')
    opn('R', 'd', 3, 'Report.docx', box=True)
    opn('N', 'o', 4, 'Notes.odt', box=True)
    opn('B', 'b', 5, 'Big report.docx')
    opn('S', 'S', 6, 'Stress.docx', box=True)
    opn('F', 'F', 7, 'Fixed codes.docx')
    opn('K', 'K', 8, 'Kept in blocks.docx')
    mg, dg = opn('V', 'G', 9, 'Veranda Desktop Guide.docx')
    export(F_DOCX, 'X', 10, mg)
    export(F_ODT, 'Q', 11, mg)
    s.stop('end')
    s.office = [{'path': 'DOCUMENTS/Veranda Desktop Guide.docx', 'state': 10, 'page': dg.page},
                {'path': 'DOCUMENTS/Veranda Desktop Guide.odt', 'state': 11, 'page': dg.page}]
    s.console = ['CW PDF 0'] + notes
    return s, {}


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
        s.menu(FILEM, F_PAGE)
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


class TModel(Model):
    """(1.6, W62-W66) Model with tables: each paragraph's table code (t:
    None, ('R', flags, width) a row's first, ('C', width) a cell's first,
    ('P', 0) a cell's further paragraph) and the table edits as
    tabedit.c makes them"""

    def __init__(self, *a):
        Model.__init__(self, *a)
        self.t = [None] * len(self.p)

    def snap(self):
        self.hist.append((list(self.c), self.cur, self.anc, list(self.p), list(self.t)))
        self.redo = []

    def undo(self):
        self.redo.append((list(self.c), self.cur, self.anc, list(self.p), list(self.t)))
        self.c, self.cur, self.anc, self.p, self.t = self.hist.pop()
        self.mod = True
        self.moved()

    def do_redo(self):
        self.hist.append((list(self.c), self.cur, self.anc, list(self.p), list(self.t)))
        self.c, self.cur, self.anc, self.p, self.t = self.redo.pop()
        self.mod = True

    def par_start(self, q):
        return self.par_range(q)[0]

    def rows(self):
        """the rows: (first paragraph, end paragraph)"""
        out, q = [], 0
        while q < len(self.t):
            if self.t[q] and self.t[q][0] == 'R':
                e = q + 1
                while e < len(self.t) and self.t[e] and self.t[e][0] in 'CP':
                    e += 1
                out.append((q, e))
                q = e
            else:
                q += 1
        return out

    def row_of(self, q):
        for r in self.rows():
            if r[0] <= q < r[1]:
                return r
        return None

    def cells(self, r):
        """a row's cells: their first paragraphs (the row's end last)"""
        return [q for q in range(*r) if self.t[q][0] in 'RC'] + [r[1]]

    def table_of(self, q):
        rs = self.rows()
        k = next(i for i, r in enumerate(rs) if r[0] <= q < r[1])
        a = k
        while a and rs[a - 1][1] == rs[a][0]:
            a -= 1
        b = k
        while b + 1 < len(rs) and rs[b][1] == rs[b + 1][0]:
            b += 1
        return rs[a:b + 1]

    def width(self, q):
        return self.t[q][2] if self.t[q][0] == 'R' else self.t[q][1]

    def set_width(self, q, w):
        self.t[q] = ('R', self.t[q][1], w) if self.t[q][0] == 'R' else ('C', w)

    def style_before(self, a):
        return self.c[a - 1][1:] if a else (SANS, 12, 0)

    def ins_pars(self, q, codes):
        """empty paragraphs (their CRs in the style before) before paragraph q"""
        a = self.par_start(q) if q < len(self.p) else len(self.c)
        st = self.style_before(a)
        for k, code in enumerate(codes):
            self.c.insert(a + k, (cwrd.CR,) + st)
            self.p.insert(q + k, BODY)
            self.t.insert(q + k, code)
        return a

    def del_pars(self, q0, q1):
        a = self.par_start(q0)
        b = self.par_start(q1) if q1 < len(self.p) else len(self.c)
        del self.c[a:b]
        del self.p[q0:q1]
        del self.t[q0:q1]
        return a

    def cell_at(self):
        q = self.par_of(self.cur)
        r = self.row_of(q)
        cs = self.cells(r)
        k = max(i for i in range(len(cs) - 1) if cs[i] <= q)
        return r, cs, k

    def go_cell(self, r, k):
        self.move(self.par_start(self.cells(r)[k]))

    def insert(self, cols, rows):
        self.snap()
        a = self.cur
        q = self.par_of(a)
        if a != self.par_start(q):
            self.ty = self.typing_style(a)
            self.type('\r', step=False)
            q += 1
        w = [min(255, 256 // cols + (k < 256 % cols)) for k in range(cols)]
        codes = [('R', 0, w[k]) if not k else ('C', w[k]) for _ in range(rows) for k in range(cols)]
        a = self.ins_pars(q, codes)
        self.move(a)
        self.mod = True

    def tab(self, fwd=True):
        r, cs, k = self.cell_at()
        rs = self.rows()
        i = rs.index(r)
        if fwd:
            if k + 1 < len(cs) - 1:
                return self.go_cell(r, k + 1)
            if i + 1 < len(rs) and rs[i + 1][0] == r[1]:
                return self.go_cell(rs[i + 1], 0)
            self.add_row(True)
            return
        if k:
            return self.go_cell(r, k - 1)
        if i and rs[i - 1][1] == r[0]:
            self.go_cell(rs[i - 1], len(self.cells(rs[i - 1])) - 2)

    def ret(self):
        self.snap()
        q = self.par_of(self.cur)
        self.type('\r', step=False)
        self.t[q + 1] = ('P', 0)
        self.mod = True

    def type(self, s, step=True):
        for i, ch in enumerate(s):
            a, b = self.sel()
            if a != b:
                q = self.par_of(a)
                m = sum(1 for x in self.c[a:b] if x[0] == cwrd.CR)
                del self.t[q + 1:q + 1 + m]
            n = len(self.p)
            q = self.par_of(a)
            Model.type(self, ch, step and not i)
            if len(self.p) > n:
                self.t.insert(q + 1, None)

    def add_row(self, below):
        self.snap()
        r, cs, k = self.cell_at()
        f = self.t[r[0]][1]
        codes = [('R', f, self.width(q)) if not i else ('C', self.width(q)) for i, q in enumerate(cs[:-1])]
        a = self.ins_pars(r[1] if below else r[0], codes)
        self.move(a)
        self.mod = True

    def add_col(self, right):
        r0, cs0, kc = self.cell_at()
        rows = self.table_of(r0[0])
        if any(len(self.cells(r)) - 1 >= 8 for r in rows):
            return
        self.snap()
        ri = self.rows().index(r0)
        for r in reversed(rows):
            cs = self.cells(r)
            n = len(cs) - 1
            k = min(kc, n - 1)
            w = max(1, self.width(cs[k]) >> 1)
            self.set_width(cs[k], max(1, self.width(cs[k]) - w))
            if not right and not k:
                f = self.t[cs[0]][1]
                self.t[cs[0]] = ('C', self.width(cs[0]))
                self.ins_pars(cs[0], [('R', f, w)])
            else:
                self.ins_pars(cs[k + right], [('C', w)])
        self.go_cell(self.rows()[ri], kc + 1 if right else kc)
        self.mod = True

    def del_row(self):
        r, cs, k = self.cell_at()
        if len(self.table_of(r[0])) == 1:
            return self.del_table()
        self.snap()
        self.move(self.del_pars(*r))
        self.mod = True

    def del_col(self):
        r0, cs0, kc = self.cell_at()
        rows = self.table_of(r0[0])
        if max(len(self.cells(r)) - 1 for r in rows) < 2:
            return self.del_table()
        self.snap()
        ri = self.rows().index(r0)
        for r in reversed(rows):
            cs = self.cells(r)
            if kc >= len(cs) - 1 or len(cs) - 1 < 2:
                continue
            k = kc - 1 if kc else 1
            self.set_width(cs[k], min(255, self.width(cs[k]) + self.width(cs[kc])))
            if not kc:
                self.t[cs[1]] = ('R', self.t[cs[0]][1], self.width(cs[1]))
            self.del_pars(cs[kc], cs[kc + 1])
        r = self.rows()[ri]
        self.go_cell(r, min(kc, len(self.cells(r)) - 2))
        self.mod = True

    def del_table(self):
        self.snap()
        rows = self.table_of(self.par_of(self.cur))
        self.move(self.del_pars(rows[0][0], rows[-1][1]))
        self.mod = True

    def widths(self, ws):
        self.snap()
        for r in self.table_of(self.par_of(self.cur)):
            for k, q in enumerate(self.cells(r)[:-1][:len(ws)]):
                self.set_width(q, ws[k] or 1)
        self.mod = True

    def grid(self, on):
        self.snap()
        for r in self.table_of(self.par_of(self.cur)):
            f = self.t[r[0]][1]
            self.t[r[0]] = ('R', f & ~1 if on else f | 1, self.t[r[0]][2])
        self.mod = True

    def tcodes(self):
        out = []
        for n, t in enumerate(self.t):
            if not t:
                continue
            if t[0] == 'R':
                out += [(n, cwrd.K_TROW, t[1]), (n, cwrd.K_TCELL, t[2])]
            else:
                out.append((n, cwrd.K_TCELL if t[0] == 'C' else cwrd.K_TPARA, t[1]))
        return out

    def tstate(self):
        return list(cwrd.tab_sum(self.tcodes()))


def session_table(run):
    """(1.6, W62-W66) tables (SESSION=table, under R49 at 512 KB): Insert
    > Table (its box), typed in, Tab and Shift+Tab, a cell's text
    wrapped, bold in a cell, Return in a cell, Tab in the last cell (a
    row more), the Table menu's rows and columns added and deleted,
    column widths (the box), equal widths, the borders off and on, undo
    and redo; Backspace after the table (into its last cell), Delete at a
    cell's end and Backspace at a cell's start (nothing), a selection over
    two cells typed over (refused), Tab out of it; saved (CWRD 1.6) and
    opened again; exported as PDF, Word and OpenDocument (their tables
    read back on the host). After each step its state (characters,
    styles, paragraphs), its tables' codes ("CW TABLE") and the redraw
    check (W55: the text area as the edits drew it against a full
    drawing)"""
    s = Script()
    s.W(90)
    m = TModel()
    n = [0, 0]
    s.redraws = []
    s.tables = []

    def V(shot=None):
        n[1] += 1
        if shot:
            s.stop(shot)
        s.L(0x80 | n[1], None)
        s.states.pop()
        s.redraws.append(n[1])

    def st(shot=None, name=''):
        n[0] += 1
        s.L(n[0], m.state(name))
        s.b += bytes([ord('L'), 0x78])
        s.W(20)
        s.tables.append([len(s.tables) + 1] + m.tstate())
        V(shot)

    def typ(t):
        s.T(t.encode('iso8859_16'))
        m.type(t.encode('iso8859_16').decode('latin-1'))

    def tmenu(item, shot=None):
        s.menu(TABLE, item, shot=shot)
        s.W(30)
    typ('Tables in Veranda Write.\r')
    s.menu(INSERT, 3)                           # Insert > Table... (its box: 3 x 3)
    s.W(30)
    s.stop('tbdlg')
    for _ in range(5):
        s.K(BS)
    s.T(b'4 x 2')                               # 4 columns, 2 rows
    s.K(RET)
    s.W(30)
    m.insert(4, 2)
    st()
    typ('Name')
    s.K(TAB)
    m.tab()
    typ('A longer text that wraps in its narrow cell, three lines or more of it.')
    s.K(TAB)
    m.tab()
    s.K(ord('b'), CTRL)                         # bold in a cell
    m.toggle(1)
    typ('Bold')
    s.K(ord('b'), CTRL)
    m.toggle(1)
    typ('.')
    s.K(TAB)
    m.tab()
    typ('Zażółć')
    st('tbtyped')
    s.K(RET)                                    # Return in a cell: a further paragraph of it
    m.ret()
    typ('gęślą jaźń')
    st()
    s.K(TAB)                                    # the next row's first cell
    m.tab()
    typ('Row two')
    s.M(SHIFT)                                  # Shift+Tab: back to the first row's last cell
    s.K(TAB)
    s.M(0)
    m.tab(False)
    typ('!')
    st()
    for _ in range(5):                          # Tab to the last cell, Tab: a row more
        s.K(TAB)
        m.tab()
    s.K(TAB)
    m.tab()
    typ('Row three')
    st('tbrow')
    tmenu(1)                                    # Insert row above
    m.add_row(False)
    typ('Above')
    st()
    tmenu(2)                                    # Insert row below
    m.add_row(True)
    typ('Below')
    st()
    tmenu(4)                                    # Insert column right
    m.add_col(True)
    typ('Right')
    st()
    tmenu(3, shot='tbmenu')                     # Insert column left (the menu open: a screenshot)
    m.add_col(False)
    typ('Left')
    st('tbcols')
    tmenu(6)                                    # Delete column (the one just made)
    m.del_col()
    st()
    tmenu(5)                                    # Delete row
    m.del_row()
    st()
    tmenu(8)                                    # Column widths: 15 40 20 15
    s.stop('tbwdlg')
    for _ in range(16):
        s.K(BS)
    s.T(b'15 40 20 15')                         # (5 columns: the last shares what is left)
    s.K(RET)
    s.W(30)
    w = [(v * 256 + 50) // 100 for v in (15, 40, 20, 15)]
    m.widths(w + [256 - sum(w)])
    st('tbwidths')
    tmenu(10)                                   # Borders off
    m.grid(False)
    st('tbnogrid')
    tmenu(10)                                   # and on
    m.grid(True)
    st()
    tmenu(9)                                    # Equal column widths
    nc = len(m.cell_at()[1]) - 1
    m.widths([min(255, 256 // nc + (k < 256 % nc)) for k in range(nc)])
    st()
    s.K(ord('z'), CTRL)                         # undone, undone, redone
    m.undo()
    st()
    s.K(ord('z'), CTRL)
    m.undo()
    st()
    s.K(ord('y'), CTRL)
    m.do_redo()
    st()
    # the edges: Backspace after the table goes into its last cell,
    # Delete at a cell's end does nothing, Backspace at a cell's start
    # does nothing, a selection over two cells is refused
    s.K(CTRL_END, CTRL)
    m.move(len(m.c))
    s.K(BS)
    m.move(len(m.c) - 1)                        # (the last cell's end)
    s.K(DEL)
    typ('E')
    st()
    s.M(SHIFT)                                  # Shift+Tab: the cell before, at its start
    s.K(TAB)
    s.M(0)
    m.tab(False)
    s.K(BS)
    typ('<')
    st()
    s.K(KEY_LEFT, SHIFT)                        # (the cursor before '<': over the cell's start)
    s.K(KEY_LEFT, SHIFT)
    s.T(b'x')                                   # refused
    s.W(20)
    st('tbrefused')
    s.K(TAB)                                    # Tab: from the cell before, into this one again
    m.move(m.cur - 1)
    typ('>')
    st()
    # saved (CWRD 1.6) and opened again
    s.K(ord('s'), CTRL | SHIFT)
    s.W(40)
    for _ in range(len('Untitled.cwr')):
        s.K(BS)
    s.T(b'Table.cwr')
    s.K(RET)
    s.W(40)
    m.mod = False
    st(name='Table.cwr')
    files = {'Table.cwr': (list(m.c), list(m.p))}
    s.tfiles = {'Table.cwr': m.tstate()}
    s.K(ord('n'), CTRL)
    s.W(20)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('T'))
    s.W(10)
    s.K(RET)
    s.W(60)
    st('tbopened', name='Table.cwr')
    # exported as PDF, Word and OpenDocument
    s.menu(FILEM, F_PDF)
    s.W(40)
    s.K(RET)
    s.W(60)
    st(name='Table.cwr')
    s.pdfs = [{'path': 'DOCUMENTS/Table.pdf', 'pages': 1,
               'on': [[1, 'Tables in Veranda Write.'], [1, 'Name'], [1, 'Row three'], [1, 'Bold.']]}]
    s.office = []
    for item, ext in ((F_DOCX, 'docx'), (F_ODT, 'odt')):
        s.menu(FILEM, item)
        s.W(40)
        s.Z()
        s.K(RET)
        s.W(60)
        s.Y(ext[0])
        st('tb' + ext, name='Table.cwr')
        s.office.append({'path': 'DOCUMENTS/Table.' + ext, 'state': n[0]})
        s.tfiles['DOCUMENTS/Table.%s.cwr' % ext] = m.tstate()      # (check.py's read-back)
    s.title = 'tables'
    s.m = m
    return s, files


def session_tspeed(run):
    """(1.6, W63) typing in a table against typing in a paragraph
    (SESSION=tspeed, under R49 at 512 KB; tools/speed.sh's 'Q' keys):
    Grid.cwr, a paragraph and a table of 10 rows by 5 columns, typed in
    at the paragraph's end and in a cell in the table's middle; Hundred.cwr,
    a table of 100 rows by 3 columns, typed in in its 50th row. Each key's
    time from the ROM's queue to the drawing after it ("CW LAT"); the
    states (no key lost)"""
    s = Script()
    s.labels = {}
    n = [0]

    def doc(rows, cols, mark_rc, mark, intro):
        chars, paras, t = [], [], []

        def par(text, code=None):
            if paras:
                chars.append((cwrd.CR, SANS, 12, 0))
            paras.append(BODY)
            t.append(code)
            chars.extend((ord(ch), SANS, 12, 0) for ch in text)
        par(intro)
        w = [256 // cols + (k < 256 % cols) for k in range(cols)]
        for r in range(rows):
            for c in range(cols):
                text = mark if (r, c) == mark_rc else 'Row %d, cell %d' % (r + 1, c + 1)
                par(text, ('R', 0, w[0]) if not c else ('C', w[c]))
        par('')
        return chars, paras, t

    def opn(letter, name, d):
        cwrd.write(os.path.join(run, name), d[0], paras=d[1], minor=6, tcodes=d[2])
        m = TModel(list(d[0]), list(d[1]))
        m.t = [(x[0], 0) if x and x[0] == 'P' else x for x in d[2]]
        s.K(ord('o'), CTRL)
        s.W(40)
        s.K(ord(letter))
        s.W(10)
        s.K(RET)
        s.W(60)
        return m

    def state(m, name, label=None):
        n[0] += 1
        st = m.state(name)
        st['cur_in'] = 1
        s.L(n[0], st)
        if label:
            s.labels[n[0]] = label

    def find(word, m):
        s.K(CTRL_HOME, CTRL)
        s.K(ord('f'), CTRL)
        s.T(word.encode())
        s.K(RET)
        s.W(20)
        s.K(ESC)
        s.K(KEY_RIGHT)
        t = ''.join(chr(ch[0]) for ch in m.c)
        m.move(t.index(word) + len(word))

    def q(t, m):
        for ch in t:
            s.b += bytes([ord('Q'), ord(ch)])
            m.type(ch)
        s.W(10)
    s.W(90)
    g = doc(10, 5, (4, 2), 'CELLMARK in a cell', 'A paragraph before the table, PARAMARK, and its text.')
    m = opn('G', 'Grid.cwr', g)
    state(m, 'Grid.cwr')
    find('PARAMARK', m)
    q(' typed in', m)
    state(m, 'Grid.cwr', 'typing in a paragraph (Grid.cwr)')
    find('CELLMARK', m)
    q(' typed in', m)
    state(m, 'Grid.cwr', 'typing in a cell of a table of 10 rows by 5 columns')
    s.K(ord('s'), CTRL)                         # (saved: the Open after it asks nothing)
    s.W(40)
    h = doc(100, 3, (49, 1), 'ROWMARK in a cell', 'A table of a hundred rows.')
    m = opn('H', 'Hundred.cwr', h)
    state(m, 'Hundred.cwr')
    find('ROWMARK', m)
    q(' typed in', m)
    state(m, 'Hundred.cwr', 'typing in a cell in the 50th row of a table of 100 rows')
    s.title = 'tspeed'
    return s, {}


def session_templ(run):
    """(1.6, W67) templates (SESSION=templ, TPLCFG=1: the documents'
    language the keyboard's; under R49 at 512 KB): File > New from
    template, the chooser (Veranda's English templates, a blank
    document); each template opened as Untitled, {date} today's; the
    keyboard made Polish ('L' $7A): the Polish ones, the date in Polish;
    one edited and saved by File > Save as template into
    /DOCUMENTS/Templates, listed as the user's and opened from there;
    the blank document. Each state against the template's file (its
    {date} today's); the shipped files unchanged"""
    import datetime
    s = Script()
    files = {}
    s.W(90)
    t = datetime.date.today()
    en = '%d %s %d' % (t.day, t.strftime('%B'), t.year)
    plm = ['stycznia', 'lutego', 'marca', 'kwietnia', 'maja', 'czerwca', 'lipca', 'sierpnia', 'września',
           'października', 'listopada', 'grudnia']
    pl = '%d %s %d' % (t.day, plm[t.month - 1], t.year)
    n = [0]

    def tmodel(lang, name, date):
        d = cwrd.read(os.path.join('build', 'veranda', 'TEMPLATES', lang, name + '.cwr'))
        ch = d.chars()
        key = [ord(x) for x in '{date}']
        out, i = [], 0
        while i < len(ch):
            if [c[0] for c in ch[i:i + 6]] == key:
                out += [(b,) + ch[i][1:] for b in date.encode('iso8859_16')]
                i += 6
            else:
                out.append(ch[i])
                i += 1
        return Model(out, d.paras())

    def choose(k, shot=None):
        s.menu(FILEM, F_NTEMPL)                 # File > New from template...
        s.W(40)
        if shot:
            s.stop(shot)
        for _ in range(k):
            s.K(KEY_DOWN)
        s.K(RET)
        s.W(90)

    def st(m, name='', shot=None):
        n[0] += 1
        s.L(n[0], m.state(name))
        if shot:
            s.stop(shot)
    # the English templates (the chooser: Blank, CV, Letter, Meeting notes, Report)
    if os.environ.get('TPL_FIRST'):
        choose(int(os.environ['TPL_FIRST']))
        s.stop('tpfirst')
    choose(2, shot='tpchoose')
    m = tmodel('EN', 'Letter', en)
    m.mod = False
    st(m, shot='tpletter')
    choose(1)
    m = tmodel('EN', 'CV', en)
    m.mod = False
    st(m, shot='tpcv')
    choose(4)
    m = tmodel('EN', 'Report', en)
    m.mod = False
    st(m, shot='tpreport')
    choose(3)
    m = tmodel('EN', 'Meeting notes', en)
    m.mod = False
    st(m, shot='tpmeet')
    # the Polish ones (the keyboard Polish; Blank, CV, List, Notatka ze spotkania, Raport)
    s.b += bytes([ord('L'), 0x7A])
    s.W(20)
    choose(4, shot='tpchoosepl')
    m = tmodel('PL', 'Raport', pl)
    m.mod = False
    st(m, shot='tpraport')
    # typed in, saved as a template of the user's
    s.K(CTRL_END, CTRL)
    s.T(b' Dodatek.')
    m.move(len(m.c))
    m.type(' Dodatek.')
    st(m)
    s.menu(FILEM, F_STEMPL)                     # File > Save as template...
    s.W(40)
    for _ in range(len('Untitled.cwr')):
        s.K(BS)
    s.T(b'Moj raport.cwr')
    s.stop('tpsave')
    s.K(RET)
    s.W(60)
    m.mod = False
    st(m, 'Moj raport.cwr')
    files['DOCUMENTS/Templates/Moj raport.cwr'] = (list(m.c), list(m.p))
    # listed as the user's, opened from there (as Untitled)
    choose(5, shot='tpmine')
    m.mod = False
    st(m)
    choose(1)                                   # the Polish CV
    m = tmodel('PL', 'Curriculum vitae', pl)
    m.mod = False
    st(m, shot='tpcvpl')
    choose(0)                                   # a blank document
    st(Model())
    s.b += bytes([ord('L'), 0x7A])              # (the keyboard as it was)
    s.W(20)
    for lang, name in (('EN', 'Letter'), ('PL', 'Raport')):
        files['VERANDA/TEMPLATES/%s/%s.cwr' % (lang, name)] = {
            'raw': open(os.path.join('build', 'veranda', 'TEMPLATES', lang, name + '.cwr'), 'rb').read()}
    return s, files

AI = 11                                     # (W68) the bar's AI menu (src/v/menus.c)
AI_REWRITE, AI_SHORTEN, AI_GRAMMAR, AI_TO_EN, AI_TO_PL, AI_SUMMARY, AI_ASK, AI_CHECK, AI_ABOUT = range(9)
AI_ACTS = ['rewrite', 'shorten', 'grammar', 'translate-en', 'translate-pl', 'summarise', 'ask']
T_AIHELP = 7                                # (Tools > AI help: on, off)


def ai_pars(ans):
    """an answer's paragraphs as v/aiui.c puts them in (src/ai.c): CR,
    LF and CR LF end one, a run of them one, those at the ends left out;
    the characters the text cannot hold left out"""
    out, brk = [], False
    for ch in ans:
        c = ord(ch)
        if c in (13, 10):
            brk = bool(out)
            continue
        if not (c == 9 or 0x20 <= c < 0x7F or c >= 0xA0):
            continue
        if brk:
            out.append('\r')
            brk = False
        out.append(ch)
    return ''.join(out)


def ai_apply(m, ans, after):
    """(W68) the answer put in as src/ai.c does, one undo step: in place of
    the selection (its first character's style; the paragraphs its
    paragraph's style; in a cell the cell's), or after the paragraph of
    the selection's end (a CR first: Body after a heading, its characters
    12 points, not bold); the answer selected"""
    m.snap()
    a, b = m.sel()
    t = ai_pars(ans.encode('iso8859_16').decode('latin-1'))
    st = m.c[a][1:]
    if not after:
        q = m.par_of(a)
        k = sum(1 for x in m.c[a:b] if x[0] == cwrd.CR)
        del m.p[q + 1:q + 1 + k]
        del m.t[q + 1:q + 1 + k]
        del m.c[a:b]
        at, ps = a, m.p[q]
    else:
        q = m.par_of(b - 1 if b > a else a)
        at = m.par_range(q)[0]
        while at < len(m.c) and m.c[at][0] != cwrd.CR:
            at += 1
        ps = m.p[q]
        if is_head(ps[0]):
            f, z, g = st
            st, ps = (f, 12, g & ~1), BODY
        t = '\r' + t
    cell = m.row_of(q) is not None
    for k, ch in enumerate(t):
        m.c.insert(at + k, (ord(ch) if ch != '\r' else cwrd.CR,) + st)
    n = t.count('\r')
    for _ in range(n):
        m.p.insert(q + 1, ps)
        m.t.insert(q + 1, ('P', 0) if cell else None)
    if after:
        m.anc, m.cur = at + 1, at + len(t)
    else:
        m.anc, m.cur = at, at + len(t)
    m.mod = True


def session_ai(run):
    """(W68) AI help (SESSION=ai, ASKFAKE=1: tools/askfake, a stand-in for
    Veranda Ask that answers as /AIFAKE says, nothing sent anywhere; under
    R49 at 512 KB): each action from the AI menu (Rewrite with a selection,
    Shorten the paragraph at the cursor, grammar on a Polish paragraph,
    Translate to English (two paragraphs back), Translate to Polish
    cancelled, Summarise and Ask (the question's box) after the
    paragraph), undo and redo; Veranda Ask not signed in, another error;
    a cell's text rewritten (the cell's paragraphs), a selection over two
    cells and the whole table refused; an answer that comes later (Veranda
    Ask put aside: not answered yet, Check for the answer, then Alt+Tab),
    one that comes after the text changed (not used); Tools > AI help off
    and on; a text over 8,192 characters refused. Each request's AI.IN and
    AI.REQ (the stand-in's copies) byte for byte; the states, the redraw
    check after each change, no request file left"""
    s = Script()
    s.W(90)
    m = TModel()
    n = [0, 0]
    s.redraws = []
    s.tables = []
    s.aifake = []
    files = {}
    s.console = []
    s.absent = ['VERANDA/.SYSTEM/' + f for f in ('AI.IN', 'AI.REQ', 'AI.OUT', 'AI.RES', 'START.REQ',
                                                  'AILATER.OUT', 'AILATER.RES')]
    req = [0]

    def V(shot=None):
        n[1] += 1
        if shot:
            s.stop(shot)
        s.L(0x80 | n[1], None)
        s.states.pop()
        s.redraws.append(n[1])

    def st(shot=None, name=''):
        n[0] += 1
        s.L(n[0], m.state(name))
        s.b += bytes([ord('L'), 0x78])
        s.W(20)
        s.tables.append([len(s.tables) + 1] + m.tstate())
        V(shot)

    def enc(t):
        return t.encode('iso8859_16')

    def typ(t):
        s.T(enc(t))
        m.type(enc(t).decode('latin-1'))

    def ask(item, fake, text, question=None, shot=None, wait=300):
        """an AI menu item: the stand-in's answer fake (its /AIFAKE line);
        text: what AI.IN must hold (None: no request expected)"""
        s.menu(AI, item, shot=shot)
        if question is not None:
            s.W(30)
            s.stop('aiask')
            s.T(enc(question))
            s.K(RET)
        if text is None:
            s.W(60)
            return
        k = chr(ord('A') + req[0])
        req[0] += 1
        s.aifake.append(enc(fake))
        files['AIIN.' + k] = {'raw': enc(text)}
        r = 'AIREQ 1\r\naction %s\r\n' % AI_ACTS[item]
        if question is not None:
            r += 'question %s\r\n' % question
        files['AIREQ.' + k] = {'raw': enc(r + 'back Veranda Write\r\n')}
        W(wait)

    def W(k):
        while k > 0:
            s.W(min(k, 250))
            k -= 250

    def para_sel():
        """nothing selected: the paragraph at the cursor, selected (v/aiui.c)"""
        q = m.par_of(m.cur)
        a, b = m.par_range(q)
        if b > a and b <= len(m.c) and m.c[b - 1][0] == cwrd.CR:
            b -= 1
        m.anc, m.cur = a, b

    def text_of(a, b):
        return ''.join(chr(c[0]) for c in m.c[a:b]).encode('latin-1').decode('iso8859_16')

    P1 = 'Veranda Write asks for help with this sentence.'
    P2 = 'A second paragraph, in plain words.'
    P3 = 'Zażółć gęślą jaźń: to jest tekst po polsku.'
    typ(P1 + '\r' + P2 + '\r' + P3)
    st()
    # Rewrite: the first paragraph selected (Shift+End)
    s.K(CTRL_HOME, CTRL)
    m.move(0)
    s.K(END, SHIFT)
    m.move(len(P1), True)
    s.W(10)
    s.stop('aibefore')
    ans = 'Veranda Write asks Veranda Ask to make this sentence clearer.'
    ask(AI_REWRITE, 'ok ' + ans, P1, shot='aimenu')
    ai_apply(m, ans, False)
    s.console.append('CW AI OK rewrite')
    st('aiafter')
    # undone (one step), redone
    s.K(ord('z'), CTRL)
    m.undo()
    st()
    s.K(ord('y'), CTRL)
    m.do_redo()
    m.anc, m.cur = 0, len(ans)
    st()
    # Shorten, nothing selected: the paragraph at the cursor
    s.K(KEY_DOWN)
    s.K(HOME)
    m.move(m.par_range(1)[0])
    s.W(10)
    para_sel()
    ans = 'A short second paragraph.'
    ask(AI_SHORTEN, 'ok ' + ans, P2)
    ai_apply(m, ans, False)
    st()
    # grammar: the Polish paragraph, Polish back
    s.K(CTRL_END, CTRL)
    m.move(len(m.c))
    para_sel()
    ans = 'Zażółć gęślą jaźń; to jest tekst po polsku!'
    ask(AI_GRAMMAR, 'ok ' + ans, P3)
    ai_apply(m, ans, False)
    st()
    # Translate to English: two paragraphs back (CR LF between, ends left out)
    para_sel()
    ans = 'Make the goose\'s heart yellow.|Two paragraphs: it is a text in Polish.|'
    ask(AI_TO_EN, 'ok ' + ans, 'Zażółć gęślą jaźń; to jest tekst po polsku!')
    ai_apply(m, ans.replace('|', '\r'), False)
    st()
    # Translate to Polish: cancelled in Veranda Ask, nothing changes
    s.K(CTRL_HOME, CTRL)
    m.move(0)
    para_sel()
    ask(AI_TO_PL, 'cancelled', text_of(m.anc, m.cur))
    s.console.append('CW AI CANCELLED')
    st()
    # Summarise the first two paragraphs: after the second
    s.K(CTRL_HOME, CTRL)
    m.move(0)
    s.K(KEY_DOWN, SHIFT)
    s.K(END, SHIFT)
    m.move(m.par_range(1)[1] - 1, True)
    a, b = m.sel()
    ans = 'A summary: Write asks, and the paragraphs are short.'
    ask(AI_SUMMARY, 'ok ' + ans, text_of(a, b))
    ai_apply(m, ans, True)
    st('aisummary')
    # Ask about this text: the question's box, the answer after the paragraph
    s.K(CTRL_HOME, CTRL)
    m.move(0)
    para_sel()
    ans = 'The tone is neutral and polite.'
    ask(AI_ASK, 'ok ' + ans, text_of(m.anc, m.cur), question='What is the tone of it?')
    s.console.append('CW AI QUESTION What is the tone of it?')
    ai_apply(m, ans, True)
    st()
    # Veranda Ask not signed in: the box; nothing changes
    s.K(CTRL_HOME, CTRL)
    m.move(0)
    para_sel()
    ask(AI_REWRITE, 'error not-set-up Veranda Ask is not signed in. Sign in in Veranda Ask first.',
        text_of(m.anc, m.cur))
    s.stop('ainosign')
    s.K(RET)
    s.W(30)
    s.stop('ainosign2')
    s.console.append('CW AI NOT SET UP')
    st()
    # another error: its message
    ask(AI_REWRITE, 'error bad-request The request could not be read by Veranda Ask (its format).',
        text_of(m.anc, m.cur))
    s.stop('aierror')
    s.K(RET)
    s.W(30)
    s.console.append('CW AI error bad-request')
    st()
    # an answer that comes later: Veranda Ask put aside (Alt+Tab there)
    s.K(CTRL_END, CTRL)
    m.move(len(m.c))
    para_sel()
    ans = 'This line came later.'
    ask(AI_REWRITE, 'later ' + ans, text_of(m.anc, m.cur))
    s.stop('ainotyet')
    s.K(RET)
    s.W(30)
    s.menu(AI, AI_CHECK)                        # Check for the answer: not yet
    s.W(60)
    s.K(RET)
    s.W(30)
    s.console += ['CW AI NOT YET', 'CW DESK LATER']
    st()
    s.K(TAB, ALT)                               # Write aside and back: the answer is there now
    W(300)
    ai_apply(m, ans, False)
    st()
    # one that comes after the text changed: not used
    para_sel()
    ask(AI_SHORTEN, 'later Not used.', text_of(m.anc, m.cur))
    s.K(RET)
    s.W(30)
    typ('!')                                    # (over the selection: the text changed)
    s.K(TAB, ALT)
    W(300)
    s.stop('aichanged')
    s.K(RET)
    s.W(30)
    s.console.append('CW AI CHANGED')
    st()
    # a table: a cell's text rewritten (the cell's paragraphs), refused over two cells
    s.K(CTRL_END, CTRL)
    m.move(len(m.c))
    typ('\r')
    s.menu(INSERT, 3)                           # Insert > Table... (3 x 3 offered)
    s.W(30)
    for _ in range(5):
        s.K(BS)
    s.T(b'2 x 2')
    s.K(RET)
    s.W(30)
    m.insert(2, 2)
    typ('A cell with its text')
    s.K(TAB)
    m.tab()
    typ('The other cell')
    st()
    s.M(SHIFT)                                  # Shift+Tab: the first cell, at its start
    s.K(TAB)
    s.M(0)
    m.tab(False)
    s.K(END, SHIFT)
    m.move(m.cur + len('A cell with its text'), True)
    ans = 'A cell|rewritten in two'
    ask(AI_REWRITE, 'ok ' + ans, 'A cell with its text')
    ai_apply(m, ans.replace('|', '\r'), False)
    st('aicell')
    s.K(TAB)                                    # the second cell; Shift+Left twice: over the cell's start
    m.tab()
    s.K(KEY_LEFT, SHIFT)
    s.K(KEY_LEFT, SHIFT)
    ask(AI_REWRITE, None, None)
    s.stop('aicells')
    s.K(RET)
    s.W(30)
    s.K(ord('a'), CTRL)                         # all of it: a table in it, refused
    ask(AI_SUMMARY, None, None)
    s.K(RET)
    s.W(30)
    s.console.append('CW AI CELLS')
    s.K(CTRL_HOME, CTRL)
    m.move(0)
    st()
    # Tools > AI help off: no AI menu; on again (WRITE.CFG)
    s.menu(TOOLS, T_AIHELP)
    s.W(60)
    s.stop('aioff')
    s.menu(TOOLS, T_AIHELP, shot='aitools')
    s.W(60)
    files['VERANDA/.SYSTEM/WRITE.CFG'] = {'raw': b'WCFG\x01\x00\x01'}
    # About AI help
    s.menu(AI, AI_ABOUT)
    s.W(30)
    s.stop('aiabout')
    s.K(RET)
    s.W(30)
    st()
    # a text over 8,192 characters: refused before Veranda Ask is started
    long = ''.join('Word%04d ' % i for i in range(1000))
    cwrd.write(os.path.join(run, 'Long.cwr'), [(ord(c), SANS, 12, 0) for c in long], minor=5)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.click(*NO_BUTTON)                         # (the changes: not saved)
    s.W(40)
    s.K(ord('L'))
    s.W(10)
    s.K(RET)
    s.W(120)
    m = TModel([(ord(c), SANS, 12, 0) for c in long])
    s.K(ord('a'), CTRL)
    ask(AI_REWRITE, None, None)
    s.stop('ailong')
    s.K(RET)
    s.W(30)
    s.console.append('CW AI LONG')
    st(name='Long.cwr')
    # (W69) the box over a long bulleted list, scrolled, an item selected:
    # Veranda Ask not signed in (as the user saw it on an X16: the box's
    # text over the page's when its Y fields were bytes); its rectangle as
    # over blank paper (tools/check_box.py: ainosignlist against ainosign),
    # then the redraw check
    items = ['Item %02d of the desktop guide: the dock, the switcher and the card' % i for i in range(1, 61)]
    d = [(ord(c), SANS, 12, 0) for c in '\r'.join(items)]
    pp = [NAMED[5]] * len(items)
    cwrd.write(os.path.join(run, 'Guide.cwr'), d, paras=pp, minor=6)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('G'))
    s.W(10)
    s.K(RET)
    s.W(120)
    m = TModel(list(d), list(pp))
    s.K(CTRL_END, CTRL)
    for _ in range(12):
        s.K(KEY_UP)
    s.K(HOME)
    m.move(m.par_range(len(items) - 13)[0])
    para_sel()
    ask(AI_REWRITE, 'error not-set-up Veranda Ask is not signed in. Sign in in Veranda Ask first.',
        text_of(m.anc, m.cur))
    s.stop('ainosignlist')
    s.K(RET)
    s.W(30)
    s.console.append('CW AI NOT SET UP')
    st(name='Guide.cwr')
    # (W70) a long guide paged out to the card at 512 KB (AI_BIG items, a
    # Body paragraph every 25th): an item two thirds down, the cursor in it
    # (Find, Home), Rewrite: AI.IN exactly its text, the answer in place of
    # exactly that item; then Find's selection there, Shorten
    nb = int(os.environ.get('AI_BIG', '5000'))
    pars, pp = [], []
    for i in range(nb):
        if i % 25 == 0:
            pars.append('Part %04d. The desktop shows one window at a time; the dock holds the programs put aside.' % i)
            pp.append(BODY)
        else:
            pars.append('Item %04d: the sidebar, the places Veranda, Apps, Documents and Mods, and the devices:'
                        ' the SD card, the printer and the trash.' % i)
            pp.append(NAMED[5])
    d = [(ord(c), SANS, 12, 0) for c in '\r'.join(pars)]
    cwrd.write(os.path.join(run, 'Desk.cwr'), d, paras=pp, minor=6)
    s.K(ord('o'), CTRL)
    s.W(40)
    s.K(ord('D'))
    s.W(10)
    s.K(RET)
    W(600)
    m = TModel(list(d), list(pp))
    st(name='Desk.cwr')

    def found(pat, home=True):
        s.K(ord('f'), CTRL)
        for _ in range(found.n):
            s.K(BS)
        s.T(pat)
        s.K(RET)
        s.K(ESC)
        s.W(30)
        found.n = len(pat)
        m.find(pat)
        if home:
            s.K(HOME)
            m.move(m.anc)
            para_sel()
    found.n = 0
    k = nb * 2 // 3 + 1
    found('Item %04d:' % k)
    ans = 'Item %04d, rewritten: the sidebar lists the places and the devices.' % k
    ask(AI_REWRITE, 'ok ' + ans, pars[k], wait=600)
    ai_apply(m, ans, False)
    st('aibig', name='Desk.cwr')
    found('Item %04d: the sidebar, the places' % (k + 2), home=False)
    ans = 'Item %04d: the places' % (k + 2)
    ask(AI_SHORTEN, 'ok ' + ans, text_of(m.anc, m.cur), wait=600)
    ai_apply(m, ans, False)
    st(name='Desk.cwr')
    # (W70) Veranda Ask put aside with a request on its screen (Alt+Tab back
    # to Write: not answered yet); another request then is refused (that
    # Veranda Ask would answer the first, as seen on an X16: another
    # document's text shown to be sent); Alt+Tab to it: its answer in place
    # of the first request's text, not of the paragraph at the cursor
    k += 5
    found('Item %04d:' % k)
    a0, b0 = m.anc, m.cur
    ask(AI_REWRITE, 'aside', pars[k], wait=600)
    s.K(RET)                                    # ("not answered yet")
    s.W(30)
    s.console += ['CW AI NOT YET', 'CW DESK RESUME WRITE']
    found('Item %04d:' % (k + 1))
    s.menu(AI, AI_REWRITE)
    s.W(60)
    s.stop('aiaside')
    s.K(RET)
    s.W(30)
    s.console.append('CW AI ASK ASIDE')
    st(name='Desk.cwr')
    s.K(TAB, ALT)                               # to the Veranda Ask aside: it answers the first
    W(600)
    s.console += ['CW DESK RESUME ASK', 'ASKFAKE ASIDE BACK']
    m.anc, m.cur = a0, b0
    ai_apply(m, 'ANSWER TO ' + enc(pars[k])[:64].decode('latin-1'), False)
    st(name='Desk.cwr')
    # (W70) Veranda Ask did not start (too little memory): AI.REQ unread when
    # Write is back: said, the request's files deleted
    found('Item %04d:' % (k + 3))
    s.aifake.append(b'nostart')
    req[0] += 1
    s.menu(AI, AI_REWRITE)
    W(600)
    s.stop('ainostart')
    s.K(RET)
    s.W(30)
    s.console.append('CW AI NOT STARTED')
    st(name='Desk.cwr')
    s.stop('end')
    return s, files


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    run = a[0]
    stop = a[a.index('--stop') + 1] if '--stop' in a else None
    s, files = {'big': session_big, 'speed': session_speed, 'page': session_page,
                 'office': session_office, 'redraw': session_redraw, 'spell': session_spell,
                 'nodict': session_nodict, 'table': session_table, 'templ': session_templ,
                 'tspeed': session_tspeed, 'ai': session_ai, 'oxspeed': session_oxspeed}.get(os.environ.get('SESSION'), session)(run)
    b = bytes(s.b)
    if stop:
        b = b[:s.names[stop]]
    # (W60) a session in two parts (s.split): the program quits after the
    # first and starts again with the second, CW.PL2 (src/v/test.c)
    sp = getattr(s, 'split', None)
    parts = [b[:sp], b[sp:]] if sp is not None and len(b) > sp else [b]
    parts[-1] += b'S'
    for name, b in zip(('CW.PLY', 'CW.PL2'), parts):
        if len(b) > 8192:
            sys.exit('script.py: %s is %d bytes (8192 at most: the session\'s bank, src/v/test.c)' % (name, len(b)))
        open(os.path.join(run, name), 'wb').write(b)
    if getattr(s, 'aifake', None) is not None:      # (W68: the stand-in for Veranda Ask's answers)
        open(os.path.join(run, 'AIFAKE'), 'wb').write(b'\n'.join(s.aifake) + b'\n')
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
               'tables': getattr(s, 'tables', []), 'tfiles': getattr(s, 'tfiles', {}),
               'spell': getattr(s, 'spell', []), 'words': getattr(s, 'words', {}), 'once': getattr(s, 'once', []),
               **({'SESSION': os.environ['SESSION']} if os.environ.get('SESSION') in ('big', 'speed', 'page', 'office', 'redraw', 'spell', 'nodict', 'table', 'templ', 'tspeed', 'ai', 'oxspeed')
                  else {})},
              open(os.path.join(run, 'expect.json'), 'w'), indent=1)

if __name__ == '__main__':
    main()
