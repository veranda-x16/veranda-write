#!/usr/bin/env python3
"""Veranda Write's tests: a .docx or .odt it exported (stage 4, W52-W53),
checked as other programs will read it.

usage: check_docx.py [--test-cw build/host/test_cw] <file.docx|file.odt>...

For each file: the ZIP (its entries whole, their CRCs right, stored, the
central directory's names its local headers'; an .odt's mimetype first,
stored, without an extra field, its text the type's); every XML part well
formed; a .docx's [Content_Types].xml naming its parts, its
relationships' targets there; an .odt's manifest naming its parts. Then
the text: Veranda Write's own reader (test_cw ox) and, when LibreOffice
is there (soffice --headless --convert-to txt), LibreOffice's: the same
words in the same order (LibreOffice's list labels aside). As a module:
check(path, test_cw) -> (problems, summary).

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
from xml.dom import minidom

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cwrd  # noqa: E402

ODT_MIME = b'application/vnd.oasis.opendocument.text'


def words(t):
    return re.findall(r'[^\s•]+', t)


def check(path, test_cw=None):
    bad = []
    data = open(path, 'rb').read()
    odt = path.lower().endswith('.odt')
    try:
        z = zipfile.ZipFile(path)
    except zipfile.BadZipFile as e:
        return ['%s: not a ZIP: %s' % (path, e)], ''
    r = z.testzip()
    if r:
        bad.append('%s: %s damaged (its CRC)' % (path, r))
    infos = z.infolist()
    names = [i.filename for i in infos]
    for i in infos:
        if i.compress_type != zipfile.ZIP_STORED:
            bad.append('%s: %s not stored' % (path, i.filename))
        # the local header: its name as the directory's
        h = data[i.header_offset:i.header_offset + 30]
        if h[:4] != b'PK\x03\x04':
            bad.append('%s: %s: no local header at %d' % (path, i.filename, i.header_offset))
            continue
        n, x = struct.unpack('<HH', h[26:30])
        if data[i.header_offset + 30:i.header_offset + 30 + n].decode('latin-1') != i.filename:
            bad.append('%s: %s: the local header names another' % (path, i.filename))
        if odt and i.filename == 'mimetype' and x:
            bad.append('%s: mimetype has an extra field' % path)
    xml = {}
    for nm in names:
        if nm.endswith('.xml') or nm.endswith('.rels'):
            b = z.read(nm)
            try:
                xml[nm] = minidom.parseString(b)
            except Exception as e:
                bad.append('%s: %s not well formed: %s' % (path, nm, e))
    if odt:
        if not names or names[0] != 'mimetype' or z.read('mimetype') != ODT_MIME:
            bad.append('%s: mimetype not first, or not %s' % (path, ODT_MIME.decode()))
        for need in ('content.xml', 'styles.xml', 'META-INF/manifest.xml'):
            if need not in names:
                bad.append('%s: no %s' % (path, need))
        man = xml.get('META-INF/manifest.xml')
        if man:
            listed = {e.getAttribute('manifest:full-path') for e in man.getElementsByTagName('manifest:file-entry')}
            for nm in names:
                if nm not in ('mimetype', 'META-INF/manifest.xml') and nm not in listed:
                    bad.append('%s: %s not in the manifest' % (path, nm))
    else:
        for need in ('[Content_Types].xml', '_rels/.rels', 'word/document.xml'):
            if need not in names:
                bad.append('%s: no %s' % (path, need))
        ct = xml.get('[Content_Types].xml')
        if ct:
            over = {e.getAttribute('PartName') for e in ct.getElementsByTagName('Override')}
            for nm in names:
                if nm.endswith('.xml') and nm != '[Content_Types].xml' and '/' + nm not in over:
                    bad.append('%s: %s has no content type' % (path, nm))
        for rels, base in (('_rels/.rels', ''), ('word/_rels/document.xml.rels', 'word/')):
            d = xml.get(rels)
            if not d:
                continue
            for e in d.getElementsByTagName('Relationship'):
                t = base + e.getAttribute('Target')
                if t not in names:
                    bad.append('%s: %s names %s, not there' % (path, rels, t))
    said = '%s: %d parts, %d bytes' % (os.path.basename(path), len(names), len(data))
    # the text: Veranda Write's reader, LibreOffice's
    if test_cw and not bad:
        tmp = tempfile.mkdtemp(dir=os.path.dirname(os.path.abspath(path)))
        try:
            out = os.path.join(tmp, 'x.cwr')
            r = subprocess.run([test_cw, 'ox', path, out], capture_output=True, text=True)
            if r.returncode:
                bad.append('%s: Veranda Write does not read it: %s' % (path, r.stdout.strip()))
                return bad, said
            own = cwrd.text(cwrd.read(out))
            so = shutil.which('soffice')
            if so and not os.environ.get('NO_SOFFICE'):
                subprocess.run([so, '--headless', '-env:UserInstallation=file://' + os.path.join(tmp, 'profile'),
                                '--convert-to', 'txt:Text (encoded):UTF8', '--outdir', tmp, path],
                               capture_output=True, timeout=240)
                txt = os.path.join(tmp, os.path.splitext(os.path.basename(path))[0] + '.txt')
                if not os.path.exists(txt):
                    bad.append('%s: LibreOffice does not open it' % path)
                else:
                    lo = open(txt, encoding='utf-8-sig').read()
                    # (LibreOffice's text: the numbered lists' labels, "1." ...)
                    lw = [w for w in words(lo) if not re.fullmatch(r'\d+\.', w)]
                    ow = words(own)
                    if lw != ow:
                        k = next((i for i in range(min(len(lw), len(ow))) if lw[i] != ow[i]), min(len(lw), len(ow)))
                        bad.append('%s: LibreOffice reads %d words, Veranda Write %d; at word %d: %r / %r'
                                   % (path, len(lw), len(ow), k, lw[k:k + 5], ow[k:k + 5]))
                    said += '; LibreOffice reads its %d words' % len(lw)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    return bad, said


def main():
    a = sys.argv[1:]
    tc = None
    if '--test-cw' in a:
        i = a.index('--test-cw')
        tc = a[i + 1]
        del a[i:i + 2]
    bad = []
    for p in a:
        b, said = check(p, tc)
        bad += b
        print('office export ' + said + ('' if b else ': ok'))
    for b in bad:
        print('FAIL ' + b)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
