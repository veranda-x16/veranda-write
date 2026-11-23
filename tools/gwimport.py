#!/usr/bin/env python3
"""geoWrite documents on the host: Veranda Write's importer in Python,
the reference for src/gwimport.c (the same rules: DECISIONS W24-W27,
docs/FORMAT.md, "geoWrite documents"), and test documents made from the
format's description.

usage: gwimport.py info <file.cvt>          what is in it (no text shown)
       gwimport.py convert <file.cvt> <out.cwr>
       gwimport.py text <file.cvt>          the text converted
       gwimport.py sample <out.cvt>         the gate's test document

As a module: convert(data) -> Conv (fonts, styles, pstyles, text: as
tools/cwrd.py's Doc; pages, pictures, header, footer, bad, tabs, sty: what
was left out), doc(conv) -> a cwrd.Doc, sample() -> the bytes of a test
document, write_cwr(path, conv).

Written from public descriptions of the format only (W1): Michael Steil,
"Inside geoWrite - 7: File Format and Pagination" (pagetable.com, 2020);
Veranda's docs/APPFORMAT.md (the CVT file); Peter Schepers' GEOS.TXT
(the info block).

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cwrd  # noqa: E402

K_CS, K_PS, K_PB, CR, TAB = 0x10, 0x11, 0x12, 0x0D, 0x09
BODY = cwrd.BODY
SPACING = (2, 3, 4, 2)
FONTS = {0: 'System', 1: 'University', 2: 'California', 3: 'Roma', 300: 'Gate Sans', 301: 'Gate Mono'}
MAXSTY, MAXFONT, MAXPAR = 128, 8, 32


def check(data):
    """0 not a GEOS file, 1 a GEOS file but not geoWrite, 2 geoWrite"""
    if len(data) < 508 or data[34:53] != b'formatted GEOS file':
        return 0
    if data[21] != 1 or data[329:342] != b'Write Image V':
        return 1
    return 2


def records(data):
    """the record table's records: {n: bytes}"""
    t = data[508:762]
    out = {}
    p = 762
    for n in range(127):
        b, last = t[2 * n], t[2 * n + 1]
        if b == 0:
            if last == 0:
                break
            continue
        size = (b - 1) * 254 + (last - 1 if last else 0)
        out[n] = data[p:p + size]
        if len(out[n]) < size:
            raise ValueError('record %d cut short' % n)
        p += b * 254
    return out


def font_name(fid):
    return FONTS.get(fid, 'GEOS font %d' % fid)


class Conv:
    def __init__(self):
        self.fonts = ['Gate Sans']
        self.styles = [(0, 12, 0, 0)]
        self.pstyles = [BODY]
        self.text = bytearray()
        self.pages = self.pictures = self.bad = 0
        self.header = self.footer = False
        self.tabs = False
        self.sty = 0
        self.cls = ''


def convert(data):
    if check(data) != 2:
        raise ValueError('not a geoWrite document')
    c = Conv()
    c.cls = data[329:349].split(b'\0')[0].decode('latin-1')
    recs = records(data)
    c.header, c.footer = 61 in recs, 62 in recs
    st = {'cs': 0, 'ts': 0, 'nps': 0, 'start': True, 'brk': False, 'pic': False}
    out = c.text

    def code(k, a):
        out.extend((k, a, k))

    def emit(ch):
        if st['pic']:
            st['pic'] = False
            if ch != CR:
                emit(CR)
        if st['start']:
            if st['brk']:
                code(K_PB, 0)
                out.append(CR)
            st['start'] = False
            if st['nps']:
                code(K_PS, st['nps'])
        st['brk'] = False
        if st['ts'] != st['cs']:
            code(K_CS, st['ts'])
            st['cs'] = st['ts']
        out.append(ch)
        if ch == CR:
            st['start'] = True

    def ruler(rb):
        lm, rm = struct.unpack('<HH', rb[0:4])
        if rm <= lm:
            return
        for i in range(8):
            t = struct.unpack('<H', rb[4 + 2 * i:6 + 2 * i])[0] & 0x7FFF
            if lm < t < rm:
                c.tabs = True
        w = 479 if rm < 480 else 639
        left = min(lm, 200)
        right = min(w - rm, 200)
        f = struct.unpack('<H', rb[20:22])[0] - left
        f = min(max(f, -left), 120)
        p = (0, rb[22] & 3, left, right, f, 0, 0, SPACING[(rb[22] >> 2) & 3])
        if p not in c.pstyles:
            if len(c.pstyles) >= MAXPAR:
                c.sty |= 1
                st['nps'] = 0
                return
            c.pstyles.append(p)
        st['nps'] = c.pstyles.index(p)

    def cardset(rb):
        fid = rb[1] * 4 + rb[0] // 64
        size = rb[0] & 63
        name = font_name(fid)
        if name not in c.fonts:
            if len(c.fonts) >= MAXFONT:
                c.sty |= 1
                name = c.fonts[0]
            else:
                c.fonts.append(name)
        b = rb[2]
        fl = (1 if b & 0x48 else 0) | (2 if b & 0x10 else 0) | (4 if b & 0x80 else 0)
        if b & 0x26:
            c.sty |= 2
        s = (c.fonts.index(name), size or 12, fl, 0)
        if s not in c.styles:
            if len(c.styles) >= MAXSTY:
                c.sty |= 1
                st['ts'] = 0
                return
            c.styles.append(s)
        st['ts'] = c.styles.index(s)

    for n in range(61):
        if n not in recs:
            continue
        c.pages += 1
        r = recs[n]
        i = 0
        while i < len(r):
            ch = r[i]
            i += 1
            if ch == 0:
                break
            if ch in (0x11, 0x17, 0x10):
                k = {0x11: 26, 0x17: 3, 0x10: 4}[ch]
                if i + k > len(r):
                    break
                rb = r[i:i + k]
                i += k
                if ch == 0x11:
                    ruler(rb)
                elif ch == 0x17:
                    cardset(rb)
                else:
                    if not st['start']:
                        emit(CR)
                    for x in b'[picture]':
                        emit(x)
                    st['pic'] = True
                    c.pictures += 1
            elif ch == 0x0C:
                st['brk'] = True
            elif ch in (TAB, CR) or 0x20 <= ch < 0x7F:
                emit(ch)
            else:
                emit(ord('?'))
                c.bad += 1
    return c


def doc(c):
    d = cwrd.Doc()
    d.fonts, d.styles, d.pstyles, d.text = list(c.fonts), list(c.styles), list(c.pstyles), bytes(c.text)
    d.version = (1, 2)
    return d


def write_cwr(path, c):
    out = bytearray(b'CWRD\x01\x02\x00\x00')

    def chunk(tag, body):
        out.extend(tag + struct.pack('<I', len(body)) + body)
    chunk(b'INFO', struct.pack('<IHH', 0, 520, 0))
    chunk(b'FONT', bytes([len(c.fonts)]) + b''.join(bytes([len(f)]) + f.encode('latin-1') for f in c.fonts))
    chunk(b'STYL', bytes([len(c.styles)]) + b''.join(bytes(s) for s in c.styles))
    chunk(b'PARA', bytes([len(c.pstyles)]) + b''.join(bytes(p[:4]) + bytes([p[4] & 255]) + bytes(p[5:])
                                                      for p in c.pstyles))
    chunk(b'TEXT', bytes(c.text))
    chunk(b'END ', b'')
    open(path, 'wb').write(out)


# ---- test documents, from the format's description ----
def ruler(lm, rm, tabs=(), pm=0, align=0, spacing=0):
    t = list(tabs) + [rm] * (8 - len(tabs))
    return bytes([0x11]) + struct.pack('<HH8HHB', lm, rm, *t, pm, align | spacing << 2) + b'\0\0\0'


def font(fid, size, style=0):
    return bytes([0x17]) + struct.pack('<HB', fid << 6 | size, style)


def picture(wcards, height, rec):
    return bytes([0x10, wcards]) + struct.pack('<H', height) + bytes([rec])


def cvt(name, recs, cls='Write Image V2.1'):
    """a CVT file: the entry and signature, the info block, the record
    table, the records ({n: bytes})"""
    b0 = bytearray(254)
    b0[0] = 0x83
    nm = name.encode('latin-1')[:16]
    b0[3:19] = nm + b'\xa0' * (16 - len(nm))
    b0[21], b0[22] = 1, 7
    b0[23:28] = bytes([126, 11, 21, 12, 0])
    sig = b'PRG formatted GEOS file V1.0'
    b0[30:30 + len(sig)] = sig
    info = bytearray(254)
    info[0:3] = b'\x03\x15\xbf'
    info[66:69] = bytes([0x83, 7, 1])
    c = cls.encode('latin-1')
    info[75:75 + len(c)] = c
    info[115:131] = b'Commander Write '
    info[135:137] = struct.pack('<H', 1)
    tbl = bytearray(254)
    body = bytearray()
    last = max(recs) if recs else -1
    for n in range(127):
        if n in recs:
            r = recs[n]
            blocks = max(1, (len(r) + 253) // 254)
            tbl[2 * n] = blocks
            tbl[2 * n + 1] = len(r) - (blocks - 1) * 254 + 1
            body += r + b'\0' * (blocks * 254 - len(r))
        elif n < last:
            tbl[2 * n + 1] = 0xFF
    return bytes(b0 + info + tbl + body)


def sample():
    """the gate's document: three pages, fonts and styles, rulers (centred,
    indented, double spacing, a tab stop), a page break between
    paragraphs, a page full inside a paragraph, a picture, a character
    geoWrite's text has not, a header and a footer"""
    p1 = (ruler(0, 479) + font(300, 18, 0x40) + b'A letter from geoWrite\r' + font(300, 12)
          + b'Dear reader, this came from ' + font(300, 12, 0x10) + b'geoWrite' + font(300, 12)
          + b' and is now in ' + font(300, 12, 0x80) + b'Commander Write' + font(300, 12) + b'.\r'
          + ruler(40, 439, pm=80, align=3, spacing=1)
          + b'An indented paragraph, justified, one and a half lines apart, its first line further in, long '
            b'enough to wrap over more than one line of the page.\r'
          + ruler(0, 479, tabs=(120,), align=1) + b'Centred\tafter a tab\r' + b'\x0c')
    p2 = (ruler(0, 479, tabs=(120,), align=1) + font(300, 12) + b'Page two starts here.\r'
          + ruler(0, 479) + b'A picture:' + picture(8, 40, 64) + b'Its caption.\r'
          + font(301, 10) + b'mono 10 \x7f\r' + font(300, 12, 0x08) + b'Outline, and the page is full in the'
          + b'\x0c')
    p3 = ruler(0, 479) + font(300, 12, 0x08) + b' middle of this paragraph.\r' + font(300, 9) + b'The end.' + b'\0'
    hdr = ruler(0, 479) + font(300, 9) + b'A header\r\0'
    ftr = ruler(0, 479) + font(300, 9) + b'A footer\r\0'
    pic = bytes([8]) + struct.pack('<H', 40) + bytes([40, 0xAA]) * 8
    return cvt('Geo letter', {0: p1, 1: p2, 2: p3, 61: hdr, 62: ftr, 64: pic})


def main():
    a = sys.argv[1:]
    if len(a) >= 2 and a[0] == 'info':
        data = open(a[1], 'rb').read()
        k = check(data)
        if k != 2:
            sys.exit('%s: %s' % (a[1], 'not a GEOS file' if k == 0 else 'a GEOS file, not a geoWrite document'))
        c = convert(data)
        d = doc(c)
        ch = d.chars()
        words = len(bytes(x[0] for x in ch).replace(b'\r', b' ').replace(b'\t', b' ').split())
        first = bytes(x[0] for x in ch).split(b'\r')[0].decode('latin-1')
        print('%s: %s, %d pages, %d characters, %d words, %d paragraphs; fonts %s; first line %d characters; '
              'left out: header %s, footer %s, pictures %d, characters %d, tab stops %s, styles %d'
              % (a[1], c.cls, c.pages, len(ch), words, len(d.paras()), c.fonts, len(first),
                 c.header, c.footer, c.pictures, c.bad, c.tabs, c.sty))
    elif len(a) >= 2 and a[0] == 'text':
        c = convert(open(a[1], 'rb').read())
        print(bytes(x[0] for x in doc(c).chars()).decode('latin-1').replace('\r', '\n'))
    elif len(a) >= 3 and a[0] == 'convert':
        c = convert(open(a[1], 'rb').read())
        write_cwr(a[2], c)
        print('%s: %d bytes of text' % (a[2], len(c.text)))
    elif len(a) >= 2 and a[0] == 'sample':
        open(a[1], 'wb').write(sample())
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
