#!/usr/bin/env python3
"""The dictionaries' GitHub release (W61): Veranda Write's spelling
dictionaries are not in Veranda's package nor in its releases; they are
the assets of one release of this repository, tagged by their format:

    tools/mkdict.py <dict-dir> <out-dir> [--repo veranda-x16/veranda-write]
                    [--version 1.0] [--date YYYY-MM-DD] [--build ID]

<out-dir> gets (made empty first):

    EN.DIC, EN.DIC.txt, PL.DIC, PL.DIC.txt   the dictionaries and their
                         licences, as they are in dict/ (an asset each, by
                         its own name: anyone may fetch one by hand)
    DICT.LST             their list for Veranda Update (Veranda's
                         DECISIONS V198), in the format of its UPDATE.LST
                         (VERANDA-UPDATE 1: a build line, the version, the
                         tag, notes, one "file" line a file with its size,
                         CRC-32, kind app, its asset's name and its place
                         on the card, VERANDA/DICT/..., and the end line)
    Veranda-Write-dictionaries.zip
                         the four files at VERANDA/DICT/ and README.txt,
                         for copying to a card by hand
    NOTES.md             the release's text

and prints the commands that publish it (only you run them: gh signed in
to the organisation). The tag is dict-<format>: dict-1 is the format
Veranda Write reads now (docs/SPELLING.md). A later revision of the
same format (more words) replaces the assets of dict-1 (gh release
upload --clobber); a new format gets a new tag (dict-2), so a Veranda
Write that reads only format 1 never fetches a dictionary it cannot
read. The release workflow .github/workflows/dictionaries.yml does the
same on GitHub (Actions > Dictionaries > Run workflow).

The dates in the zip and the list are the last commit's of dict/ (git),
so the same dictionaries give the same files.

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import argparse
import os
import re
import shutil
import subprocess
import sys
import zipfile
import zlib

FILES = ('EN.DIC', 'EN.DIC.txt', 'PL.DIC', 'PL.DIC.txt')   # (each dictionary with its licence after it)
FORMAT = 1
ZIP = 'Veranda-Write-dictionaries.zip'
NAME_RE = re.compile(r'^[0-9A-Za-z.]{1,13}$')              # (Veranda Update's asset names)
BUILD_RE = re.compile(r'^[0-9A-Za-z._+-]{1,15}$')
VERSION_RE = re.compile(r'^\d{1,3}\.\d{1,3}(\.\d{1,3})?$')

README = '''VERANDA WRITE'S SPELLING DICTIONARIES (format {fmt}, {version}, {date})

English (EN.DIC, {en} word forms) and Polish (PL.DIC, {pl} word forms)
for Veranda Write's spelling (Tools > Check spelling, the marks as you
type), each with its licence beside it (EN.DIC.txt, PL.DIC.txt).

TO INSTALL THEM BY HAND
  Copy the folder VERANDA of this zip to the top of the X16's SD card,
  into the VERANDA folder there: the four files go into /VERANDA/DICT,
  beside SPELL.OVL. Either dictionary alone is fine.

OR ON THE X16
  In Veranda Write: Tools > Get dictionaries. Veranda Update fetches them
  over the Wi-Fi modem (TexElec's Serial & ESP32 Network Card) from
  {url}

LICENCES
  EN.DIC: SCOWL (wordlist.aspell.net), its permissive notice in full in
  EN.DIC.txt. PL.DIC: Morfologik's forms (BSD 2-clause) chosen by
  FrequencyWords (CC BY-SA 4.0): see PL.DIC.txt. Keep each .txt with its
  dictionary when you pass them on.
'''

NOTES = '''Veranda Write's spelling dictionaries, format {fmt} ({version}).

- **EN.DIC**: English, {en} word forms (SCOWL; its notice: EN.DIC.txt)
- **PL.DIC**: Polish, {pl} word forms (Morfologik and FrequencyWords, CC BY-SA 4.0 and BSD; see PL.DIC.txt)

On the X16: Veranda Write, Tools > Get dictionaries (Veranda Update fetches them over the Wi-Fi modem; DICT.LST is its list).
By hand: copy the folder VERANDA from {zip} to the top of the SD card.
'''


def git(*a):
    r = subprocess.run(['git'] + list(a), capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else ''


def forms(txt):
    m = re.search(r'([\d][\d ,]*\d) (?:English|Polish) word forms', txt)
    return m.group(1).replace(' ', ',') if m else '?'


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('dict')
    ap.add_argument('out')
    ap.add_argument('--repo', default='veranda-x16/veranda-write')
    ap.add_argument('--version', default='%d.0' % FORMAT)
    ap.add_argument('--date')
    ap.add_argument('--build')
    a = ap.parse_args()
    if not VERSION_RE.match(a.version) or not a.version.startswith('%d.' % FORMAT):
        sys.exit('mkdict: --version %s: %d.N (the format first)' % (a.version, FORMAT))
    date = a.date or git('log', '-1', '--format=%cs', '--', a.dict) or '2026-11-25'
    build = a.build or 'd' + (git('log', '-1', '--format=%h', '--', a.dict) or 'local')[:14]
    if not BUILD_RE.match(build) or not re.match(r'^\d{4}-\d\d-\d\d$', date):
        sys.exit('mkdict: the build %r or the date %r' % (build, date))
    tag = 'dict-%d' % FORMAT
    if os.path.exists(a.out):
        shutil.rmtree(a.out)
    os.makedirs(a.out)
    data = {}
    for n in FILES:
        p = os.path.join(a.dict, n)
        if not os.path.isfile(p):
            sys.exit('mkdict: no %s' % p)
        if not NAME_RE.match(n) or len('VERANDA/DICT/' + n) > 63:
            sys.exit('mkdict: %s: a name Veranda Update refuses' % n)
        data[n] = open(p, 'rb').read()
        shutil.copy(p, os.path.join(a.out, n))
    en, pl = forms(data['EN.DIC.txt'].decode('utf-8', 'replace')), forms(data['PL.DIC.txt'].decode('utf-8', 'replace'))
    # the list (Veranda's tools/mkmanifest.py's format, apps/update/man.c reads it)
    out = ['VERANDA-UPDATE 1', 'build %s %s' % (build, date), 'version %s' % a.version, 'tag %s' % tag,
           'note Veranda Write\'s spelling dictionaries: English (%s word forms) and' % en,
           'note Polish (%s), each with its licence.' % pl]
    for n in FILES:
        out.append('file %d %08x app - %s VERANDA/DICT/%s' % (len(data[n]), zlib.crc32(data[n]) & 0xFFFFFFFF, n, n))
    out.append('end %d' % len(FILES))
    lst = '\n'.join(out) + '\n'
    if any(len(l) > 127 for l in out):
        sys.exit('mkdict: a line of DICT.LST is longer than Veranda Update reads')
    open(os.path.join(a.out, 'DICT.LST'), 'w', newline='\n').write(lst)
    url = 'https://github.com/%s/releases/tag/%s' % (a.repo, tag)
    readme = README.format(fmt=FORMAT, version=a.version, date=date, en=en, pl=pl, url=url).replace('\n', '\r\n')
    when = tuple(int(x) for x in date.split('-')) + (12, 0, 0)
    with zipfile.ZipFile(os.path.join(a.out, ZIP), 'w', zipfile.ZIP_DEFLATED) as z:
        for name, b in [('README.txt', readme.encode('ascii'))] + [('VERANDA/DICT/' + n, data[n]) for n in FILES]:
            zi = zipfile.ZipInfo(name, when)
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.external_attr = 0o644 << 16
            z.writestr(zi, b)
    open(os.path.join(a.out, 'NOTES.md'), 'w').write(NOTES.format(fmt=FORMAT, version=a.version, en=en, pl=pl, zip=ZIP))
    assets = ' '.join(list(FILES) + ['DICT.LST', ZIP])
    print('mkdict: %s: %s (%s, build %s of %s)' % (a.out, assets, tag, build, date))
    print('To publish (gh signed in; nothing here runs it):')
    print('  cd %s' % a.out)
    print('  the first time:  gh release create %s --repo %s --title "Veranda Write\'s dictionaries (format %d)" '
          '--notes-file NOTES.md --latest=false %s' % (tag, a.repo, FORMAT, assets))
    print('  a new revision:  gh release upload %s --repo %s --clobber %s' % (tag, a.repo, assets))
    print('  (or on GitHub: Actions > Dictionaries > Run workflow)')


if __name__ == '__main__':
    main()
