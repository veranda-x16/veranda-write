#!/usr/bin/env python3
"""Veranda Write documents on the host (docs/FORMAT.md): read, write,
check, sum.

usage: cwrd.py dump <file.cwr>            the text with its styles
       cwrd.py check <file.cwr>           well formed (exit 1 if not)
       cwrd.py big <out.cwr> [bytes]      a big styled document (the gate's)

As a module: read(path) -> Doc (chars(): a list of (character, font
name, size, flags); paras(): each paragraph's style, a tuple of PARA's 8
bytes: kind, align, left, right, first (signed), before, after,
spacing; charset: INFO's byte 6, 1.3, W38: 0 Latin-1, 1 ISO-8859-16;
page: 1.4's PAGE, W44, a dict, None without it; pbreaks(): the page
breaks), write(path, chars, paras=None, minor=1: the text's bytes as they
are; minor 3 marks them ISO-8859-16; breaks: the characters a page break
comes before, each the CR of an empty Body paragraph; page: a PAGE dict,
minor 4), text(doc) (the characters as a str, decoded by
the file's set), doc_sum(chars) and
par_sum(paras) (the sums the test build logs: src/v/test.c).

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import struct
import sys

K_FIRST, K_LAST, K_CS, K_PS, K_PB = 0x10, 0x17, 0x10, 0x11, 0x12
PAGE_FMT = '<BBHHHHHH64s64s'            # (1.4: the PAGE chunk, 142 bytes)
PAGE_KEYS = ('paper', 'flags', 'w', 'h', 'top', 'bottom', 'left', 'right', 'head', 'foot')
CR = 0x0D
# the named paragraph styles (src/text.c's named[]): kind, align, left,
# right, first, before, after, spacing
BODY = (0, 0, 0, 0, 0, 0, 0, 2)
NAMED = [BODY, (1, 0, 0, 0, 0, 16, 6, 2), (2, 0, 0, 0, 0, 12, 4, 2), (3, 0, 0, 0, 0, 8, 2, 2),
         (4, 0, 40, 40, 0, 4, 4, 2), (5, 0, 32, 0, -16, 0, 2, 2), (6, 0, 32, 0, -24, 0, 2, 2)]


class FormatError(Exception):
    pass


class Doc:
    def __init__(self):
        self.fonts = []
        self.styles = []        # (font, size, flags, colour)
        self.text = b''
        self.cursor = 0
        self.width = 520
        self.charset = 0        # (INFO byte 6, 1.3: 1 ISO-8859-16)
        self.version = (1, 0)
        self.chunks = []
        self.pstyles = [BODY]
        self.page = None

    def pbreaks(self):
        """the page breaks: [$12 0 $12] codes"""
        t, i, n = self.text, 0, 0
        while i < len(t):
            if K_FIRST <= t[i] <= K_LAST:
                n += t[i] == K_PB
                i += 3
            else:
                i += 1
        return n

    def chars(self):
        """the characters with their styles: (c, font name, size, flags)"""
        out = []
        s = 0
        t = self.text
        i = 0
        while i < len(t):
            c = t[i]
            if K_FIRST <= c <= K_LAST:
                if i + 2 >= len(t) or t[i + 2] != c:
                    raise FormatError('a broken code at %d' % i)
                if c == K_PS:
                    if i and t[i - 1] != CR:
                        raise FormatError('a paragraph code at %d, not first in its paragraph' % i)
                    if not 0 < t[i + 1] < len(self.pstyles):
                        raise FormatError('paragraph style %d at %d: there are %d' % (t[i + 1], i, len(self.pstyles)))
                if c == K_CS:
                    s = t[i + 1]
                    if s >= len(self.styles):
                        raise FormatError('style %d at %d: there are %d' % (s, i, len(self.styles)))
                i += 3
                continue
            if not (c in (9, CR) or 0x20 <= c < 0x7F or c >= 0xA0):
                raise FormatError('byte %02X at %d' % (c, i))
            f, z, g, _ = self.styles[s]
            out.append((c, self.fonts[f], z, g))
            i += 1
        return out

    def paras(self):
        """each paragraph's style (PARA's entry)"""
        t = self.text
        out = [self.pstyles[0]]
        i = 0
        while i < len(t):
            c = t[i]
            if K_FIRST <= c <= K_LAST:
                if c == K_PS and t[i + 1] < len(self.pstyles):
                    out[-1] = self.pstyles[t[i + 1]]
                i += 3
                continue
            if c == CR:
                out.append(self.pstyles[0])
            i += 1
        return out


def read(path):
    data = open(path, 'rb').read()
    if data[:4] != b'CWRD':
        raise FormatError('not a Veranda Write document')
    d = Doc()
    d.version = (data[4], data[5])
    p = 8
    seen_end = False
    while p + 8 <= len(data):
        tag, n = data[p:p + 4], struct.unpack('<I', data[p + 4:p + 8])[0]
        body = data[p + 8:p + 8 + n]
        if len(body) != n:
            raise FormatError('chunk %r cut short' % tag)
        d.chunks.append(tag.decode('latin-1'))
        p += 8 + n
        if tag == b'END ':
            seen_end = True
            break
        if tag == b'INFO':
            d.cursor, d.width = struct.unpack('<IH', body[:6])
            d.charset = body[6] if len(body) > 6 else 0     # (1.3, W38: 0 Latin-1, 1 ISO-8859-16)
        elif tag == b'FONT':
            q = 1
            for _ in range(body[0]):
                ln = body[q]
                d.fonts.append(body[q + 1:q + 1 + ln].decode('latin-1'))
                q += 1 + ln
        elif tag == b'STYL':
            for k in range(body[0]):
                d.styles.append(tuple(body[1 + 4 * k:5 + 4 * k]))
        elif tag == b'PARA':
            n = body[0]
            k = (len(body) - 1) // n if n else 8
            d.pstyles = []
            for j in range(n):
                e = body[1 + k * j:9 + k * j]
                d.pstyles.append(tuple(e[:4]) + (e[4] - 256 if e[4] > 127 else e[4],) + tuple(e[5:8]))
        elif tag == b'PAGE' and n >= struct.calcsize(PAGE_FMT):
            v = struct.unpack(PAGE_FMT, body[:struct.calcsize(PAGE_FMT)])
            d.page = dict(zip(PAGE_KEYS, v))
            for k in ('head', 'foot'):
                d.page[k] = d.page[k].split(b'\0')[0].decode('iso8859_16')
        elif tag == b'TEXT':
            d.text = body
    if not seen_end:
        raise FormatError('no END chunk')
    return d


def text(d):
    """d's characters (codes left out, CR as \\n) as a str, by its set"""
    out, i, t = [], 0, d.text
    while i < len(t):
        c = t[i]
        if K_FIRST <= c <= K_LAST:
            i += 3
            continue
        out.append('\n' if c == CR else bytes([c]).decode('iso8859_16' if getattr(d, 'charset', 0) == 1 else 'latin-1'))
        i += 1
    return ''.join(out)


def write(path, chars, fonts=None, paras=None, minor=1, breaks=(), page=None, lang=0):
    """a document of chars ((c, font, size, flags)...), a style code
    wherever the style changes; paras: each paragraph's style (a code at
    the start of each that is not Body); minor 0: a 1.0 file (no PARA);
    lang (1.5, W57): the spelling language, 1 English, 2 Polish"""
    fonts = list(fonts or [])
    styles = []
    text = bytearray()
    cur = None
    for c, f, z, g in chars:
        if f not in fonts:
            fonts.append(f)
        st = (fonts.index(f), z, g, 0)
        if st not in styles:
            styles.append(st)
        k = styles.index(st)
        if k != cur:
            if cur is not None or k != 0:
                text += bytes([K_CS, k, K_CS])
            cur = k
    # (style 0 is the first character's, so the text needs no code at its start)
    pst = [BODY]
    paras = list(paras or [])
    for p in paras:
        if p not in pst:
            pst.append(p)
    text = bytearray()
    cur = 0
    np = 0

    def pcode():
        if np < len(paras) and paras[np] != BODY:
            text.extend(bytes([K_PS, pst.index(paras[np]), K_PS]))
    pcode()
    for i, (c, f, z, g) in enumerate(chars):
        if i in breaks:
            text += bytes([K_PB, 0, K_PB])
        k = styles.index((fonts.index(f), z, g, 0))
        if k != cur:
            text += bytes([K_CS, k, K_CS])
            cur = k
        text.append(c)
        if c == CR:
            np += 1
            pcode()
    if not styles:
        fonts = fonts or ['Gate Sans']
        styles = [(0, 12, 0, 0)]
    out = bytearray(b'CWRD\x01' + bytes([minor]) + b'\x00\x00')

    def chunk(tag, body):
        out.extend(tag + struct.pack('<I', len(body)) + body)
    chunk(b'INFO', struct.pack('<IHBB', 0, 520, 1 if minor >= 3 else 0, lang))   # (1.3: the set, W38; 1.5: the language)
    fb = bytes([len(fonts)]) + b''.join(bytes([len(f)]) + f.encode('latin-1') for f in fonts)
    chunk(b'FONT', fb)
    chunk(b'STYL', bytes([len(styles)]) + b''.join(bytes(s) for s in styles))
    if minor:
        chunk(b'PARA', bytes([len(pst)]) + b''.join(bytes(p[:4]) + bytes([p[4] & 255]) + bytes(p[5:])
                                                    for p in pst))
    if page:
        v = [page[k] for k in PAGE_KEYS]
        v[8:] = [x.encode('iso8859_16') for x in v[8:]]
        chunk(b'PAGE', struct.pack(PAGE_FMT, *v))
    chunk(b'TEXT', bytes(text))
    chunk(b'END ', b'')
    open(path, 'wb').write(out)


def doc_sum(chars):
    """src/v/test.c's sum: c + 256 flags + 4096 size + 65536 (the font
    name's bytes summed), 32 bits"""
    s = 0
    for c, f, z, g in chars:
        s += c + (g << 8) + (z << 12) + (sum(f.encode('latin-1')) << 16)
    return s & 0xFFFFFFFF


def par_sum(paras):
    """src/v/test.c's paragraphs' sum: for each paragraph, s = s * 31 +
    (kind + 8 align + left + 3 right + 5 first + 7 before + 11 after + 13
    spacing), 32 bits"""
    s = 0
    for p in paras:
        k, a, l, r, f, b, af, sp = p
        s = (s * 31 + k + 8 * a + l + 3 * r + 5 * (f & 255) + 7 * b + 11 * af + 13 * sp) & 0xFFFFFFFF
    return s


WORDS = ('the quick brown fox jumps over a lazy dog while Commander Write wraps every line of '
         'this long document across many RAM banks and keeps its styles bold italic and '
         'underlined text in Gate Sans and Gate Mono at several sizes').split()


def big_chars(n):
    """about n bytes of paragraphs, every fifth word in another style"""
    x = 7
    out = []
    styles = [('Gate Sans', 12, 0), ('Gate Sans', 12, 1), ('Gate Sans', 12, 2), ('Gate Sans', 12, 4),
              ('Gate Mono', 10, 0), ('Gate Sans', 18, 1)]
    size = 0
    para = 0
    while size < n:
        para += 1
        words = 40 + para % 60
        for w in range(words):
            x = (x * 1103515245 + 12345) & 0x7FFFFFFF
            word = WORDS[(x >> 8) % len(WORDS)]
            st = styles[(x >> 4) % len(styles)] if w % 5 == 4 else styles[0]
            for ch in word:
                out.append((ord(ch), st[0], st[1], st[2]))
            out.append((32, 'Gate Sans', 12, 0))
            size += len(word) + 1
        out[-1] = (CR, 'Gate Sans', 12, 0)
        size += 7
    return out


def main():
    a = sys.argv[1:]
    if len(a) >= 2 and a[0] == 'dump':
        d = read(a[1])
        print('version %d.%d, chunks %s, fonts %s, %d styles, %d paragraph styles, %d bytes of text, cursor %d'
              % (d.version + (d.chunks, d.fonts, len(d.styles), len(d.pstyles), len(d.text), d.cursor)))
        line = ''
        for c, f, z, g in d.chars():
            line += chr(c) if c != CR else '\n'
        print(line)
    elif len(a) >= 2 and a[0] == 'check':
        try:
            d = read(a[1])
            ch = d.chars()
        except FormatError as e:
            sys.exit('%s: %s' % (a[1], e))
        ps = d.paras()
        print('%s: ok, %d characters, %d styles, %d paragraphs, sum %08X, paragraphs %08X'
              % (a[1], len(ch), len(d.styles), len(ps), doc_sum(ch), par_sum(ps)))
    elif len(a) >= 2 and a[0] == 'big':
        n = int(a[2]) if len(a) > 2 else 200000
        ch = big_chars(n)
        write(a[1], ch)
        print('%s: %d characters' % (a[1], len(ch)))
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
