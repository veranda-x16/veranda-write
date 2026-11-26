#!/usr/bin/env python3
"""Veranda Write's PDF fonts (DECISIONS W47): the widths of Helvetica and
Helvetica-Bold for the 192 character slots of the Veranda set
(ISO-8859-16: $20-$7E, $A0-$FF; CSLOT in src/cw.h), and the PDF
encoding's /Differences (the 40 bytes where ISO-8859-16 is not
WinAnsiEncoding), written as src/pdfw.h.

usage: afmtab.py <Helvetica.afm> <Helvetica-Bold.afm> [<more.afm> <more-bold.afm>] > src/pdfw.h

A width is the first pair's, else the second's: Adobe's Helvetica
metrics first (TeX Live's fonts/afm/adobe/helvetic/phvr8a.afm,
phvb8a.afm), then URW's metric-compatible Nimbus Sans for the Central
European glyphs those lack (fonts/afm/urw/helvetic/uhvr8a.afm,
uhvb8a.afm: the same widths where both have a glyph but a few figures and
signs). Each width (in 1/1000 em) is kept divided by 4, rounded (a byte;
at most 2 units off: half a point in a line of 100 characters at 10
points). Courier is 600 everywhere.

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import sys
import unicodedata

# glyph names of the Adobe Glyph List for the set's letters above $9F
AGL = {
    0x00A0: 'space', 0x00AD: 'hyphen', 0x0104: 'Aogonek', 0x0105: 'aogonek', 0x0141: 'Lslash',
    0x0142: 'lslash', 0x20AC: 'Euro', 0x201E: 'quotedblbase', 0x201D: 'quotedblright', 0x0160: 'Scaron',
    0x0161: 'scaron', 0x0218: 'Scommaaccent', 0x0219: 'scommaaccent', 0x0179: 'Zacute', 0x017A: 'zacute',
    0x017B: 'Zdotaccent', 0x017C: 'zdotaccent', 0x010C: 'Ccaron', 0x010D: 'ccaron', 0x017D: 'Zcaron',
    0x017E: 'zcaron', 0x0152: 'OE', 0x0153: 'oe', 0x0178: 'Ydieresis', 0x0102: 'Abreve', 0x0103: 'abreve',
    0x0106: 'Cacute', 0x0107: 'cacute', 0x0110: 'Dcroat', 0x0111: 'dcroat', 0x0143: 'Nacute',
    0x0144: 'nacute', 0x0150: 'Ohungarumlaut', 0x0151: 'ohungarumlaut', 0x015A: 'Sacute', 0x015B: 'sacute',
    0x0170: 'Uhungarumlaut', 0x0171: 'uhungarumlaut', 0x0118: 'Eogonek', 0x0119: 'eogonek',
    0x021A: 'Tcommaaccent', 0x021B: 'tcommaaccent', 0x00A7: 'section', 0x00A9: 'copyright',
    0x00AB: 'guillemotleft', 0x00BB: 'guillemotright', 0x00B0: 'degree', 0x00B1: 'plusminus',
    0x00B6: 'paragraph', 0x00B7: 'periodcentered', 0x00DF: 'germandbls', 0x00C6: 'AE', 0x00E6: 'ae',
    0x00FF: 'ydieresis',
}
ASCII = {' ': 'space', '!': 'exclam', '"': 'quotedbl', '#': 'numbersign', '$': 'dollar', '%': 'percent',
         '&': 'ampersand', "'": 'quotesingle', '(': 'parenleft', ')': 'parenright', '*': 'asterisk',
         '+': 'plus', ',': 'comma', '-': 'hyphen', '.': 'period', '/': 'slash', ':': 'colon',
         ';': 'semicolon', '<': 'less', '=': 'equal', '>': 'greater', '?': 'question', '@': 'at',
         '[': 'bracketleft', '\\': 'backslash', ']': 'bracketright', '^': 'asciicircum', '_': 'underscore',
         '`': 'grave', '{': 'braceleft', '|': 'bar', '}': 'braceright', '~': 'asciitilde'}
DIGITS = ['zero', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine']


def name(u):
    """the AGL name of code point u"""
    if u in AGL:
        return AGL[u]
    c = chr(u)
    if c in ASCII:
        return ASCII[c]
    if c.isdigit() and u < 128:
        return DIGITS[u - 48]
    if c.isalpha() and u < 128:
        return c
    n = unicodedata.name(c)                 # LATIN CAPITAL LETTER A WITH GRAVE -> Agrave
    base, _, acc = n.partition(' WITH ')
    letter = base.split()[-1]
    letter = letter if 'CAPITAL' in base else letter.lower()
    acc = {'GRAVE': 'grave', 'ACUTE': 'acute', 'CIRCUMFLEX': 'circumflex', 'DIAERESIS': 'dieresis',
           'CEDILLA': 'cedilla', 'TILDE': 'tilde', 'RING ABOVE': 'ring'}[acc]
    return letter + acc


def afm(path):
    w = {}
    for line in open(path, encoding='latin-1'):
        if line.startswith('C '):
            f = dict(x.strip().split(' ', 1) for x in line.split(';') if x.strip())
            w[f['N']] = int(float(f['WX']))
    return w


def slots():
    """(byte, glyph name) of the 192 slots ($7F: slot 95, none)"""
    out = []
    for b in list(range(0x20, 0x80)) + list(range(0xA0, 0x100)):
        if b == 0x7F:
            out.append((b, None))
            continue
        u = ord(bytes([b]).decode('iso8859_16'))
        out.append((b, name(u)))
    return out


def main():
    if len(sys.argv) not in (3, 5):
        sys.exit(__doc__)
    reg, bold = afm(sys.argv[1]), afm(sys.argv[2])
    if len(sys.argv) == 5:
        reg = dict(afm(sys.argv[3]), **reg)
        bold = dict(afm(sys.argv[4]), **bold)
    print('/* Veranda Write: the PDF fonts\' widths and encoding (made by')
    print('   tools/afmtab.py from Helvetica\'s and Helvetica-Bold\'s AFM metrics;')
    print('   DECISIONS W47). A width is in 1/250 em (the AFM\'s / 4); Courier\'s')
    print('   are 600 / 4. Copyright (c) 2026 Marcin Spoczynski. MIT licence. */')
    for nm, w in (('pw_helv', reg), ('pw_helvb', bold)):
        if nm == 'pw_helvb':
            print('#ifndef PDFW_HELV                       /* (pdfhf.c: Helvetica\'s alone) */')
        vals = []
        for b, g in slots():
            vals.append(0 if g is None else (w[g] + 2) // 4)
        print('static const unsigned char %s[192] = {' % nm)
        for i in range(0, 192, 16):
            print('    ' + ', '.join('%3d' % v for v in vals[i:i + 16]) + ',')
        print('};')
    print('#define PW_BULLET %d, %d   /* WinAnsi\'s $95 (the lists\' bullet) */'
          % ((reg['bullet'] + 2) // 4, (bold['bullet'] + 2) // 4))
    # the encoding: WinAnsi, and where ISO-8859-16 differs
    win = bytes(range(0xA0, 0x100)).decode('cp1252')
    diff, last = [], -2
    for b in range(0xA0, 0x100):
        u = ord(bytes([b]).decode('iso8859_16'))
        if chr(u) == win[b - 0xA0]:
            continue
        g = name(u)
        assert g in reg and g in bold, g
        diff.append(('' if b == last + 1 else '%d ' % b) + '/' + g)
        last = b
    s = ' '.join(diff)
    print('#include "pdfenc.h"')
    print('#endif')
    # (1.6) the encoding's text: src/pdfenc.h, a macro (the X16 keeps it in overlay 28)
    out = ['/* (1.6: a macro: the X16 keeps it in overlay 28, v/tbui.c\'s pdf_enc,', '   for overlay 15\'s room) */',
           '#define PDF_DIFF \\']
    while s:
        k = s.rfind(' ', 0, 70) if len(s) > 70 else len(s)
        out.append('    "%s%s"%s' % (s[:k], ' ' if k < len(s) else '', ' \\' if k < len(s) else ''))
        s = s[k + 1:]
    print('/* (src/pdfenc.h) */', file=sys.stderr)
    print('\n'.join(out), file=sys.stderr)


main()
