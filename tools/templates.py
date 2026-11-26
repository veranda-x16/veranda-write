#!/usr/bin/env python3
"""Veranda Write's templates (DECISIONS W67): Veranda Write documents
(CWRD 1.6, tools/cwrd.py) made from readable sources, templates/en/*.txt
and templates/pl/*.txt, so they can be made again and read in a diff.

usage: templates.py <templates folder> <out folder>
       (make veranda: templates build/veranda/TEMPLATES; out/EN, out/PL)

A source is lines of text, a paragraph a line (an empty line an empty
paragraph), in UTF-8 (Veranda's set, ISO-8859-16: Polish too):

  # ...                 a comment
  @lang en|pl           the document's spelling language
  @file NAME            the file's name (NAME.cwr)
  @header TEXT          the page's header and footer (Veranda Write's
  @footer TEXT          "left|centre|right" with {page} {pages} {date}
                        {name}: docs/FORMAT.md's PAGE)
  @widths P P ...       the next table's columns' widths, per cent of
                        the text's (equal when not said)
  = TEXT, == , ===      Heading 1, 2, 3
  * TEXT                a bulleted list's item
  >TEXT                 aligned right
  | A | B | C |         a table's row: its cells (a row's lines next to
                        each other make a table); a cell's "\\" at a
                        line's end goes on in the next line as a
                        further paragraph of the cell
  **TEXT**              bold (in any paragraph or cell)

The text "{date}" stays as it is: Veranda Write puts the day's date there
when it opens a template (File > New from template), in the template's
language. Gate Sans 12 throughout, the headings in their named styles'
sizes (27, 18, 12, bold), A4 with margins of 2.5 cm.

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cwrd  # noqa: E402

SANS = 'Gate Sans'
CR = cwrd.CR
H_SIZE = {1: 27, 2: 18, 3: 12}
RIGHT = (0, 2, 0, 0, 0, 0, 0, 2)        # Body, aligned right
A4 = {'paper': 0, 'flags': 0, 'w': 11906, 'h': 16838, 'top': 1417, 'bottom': 1417, 'left': 1417,
      'right': 1417, 'head': '', 'foot': ''}


def runs(text, size=12, flags=0):
    """the characters of text, **bold** in bold"""
    out = []
    bold = False
    for k, part in enumerate(re.split(r'\*\*', text)):
        for ch in part:
            b = ch.encode('iso8859_16')
            out.append((b[0], SANS, size, flags | (1 if k % 2 else 0)))
    return out


def widths(pcts, n):
    """n cells' widths, 256ths of the text's (the per cents given, else equal)"""
    if not pcts or len(pcts) != n:
        return [min(255, 256 // n + (k < 256 % n)) for k in range(n)]
    w = [max(1, (p * 256 + 50) // 100) for p in pcts]
    s = sum(w)
    if s > 256:
        w = [max(1, x * 256 // s) for x in w]
    return [min(255, x) for x in w]


def make(src):
    chars, paras, tcodes = [], [], []
    meta = {'lang': 1, 'file': None, 'page': dict(A4)}
    pend = None

    def para(text, ps=cwrd.BODY, t=None, size=12, flags=0):
        if paras:
            chars.append((CR, SANS, chars[-1][2] if chars else 12, chars[-1][3] if chars else 0))
        paras.append(ps)
        tcodes.append(t)
        chars.extend(runs(text, size, flags))

    lines = open(src, encoding='utf-8').read().split('\n')
    if lines and lines[-1] == '':
        lines.pop()
    i = 0
    while i < len(lines):
        ln = lines[i]
        i += 1
        if ln.startswith('#'):
            continue
        if ln.startswith('@'):
            key, _, val = ln[1:].partition(' ')
            if key == 'lang':
                meta['lang'] = 2 if val.strip() == 'pl' else 1
            elif key == 'file':
                meta['file'] = val.strip()
            elif key in ('header', 'footer'):
                meta['page']['head' if key == 'header' else 'foot'] = val.strip()
            elif key == 'widths':
                pend = [int(x) for x in val.split()]
            continue
        if ln.startswith('|'):
            # a row: its cells (a "\" at a line's end: the cell goes on)
            row = ln
            while row.endswith('\\') and i < len(lines):
                row = row[:-1] + '\n' + lines[i]
                i += 1
            cells = [c.strip() for c in row.strip().strip('|').split('|')]
            ws = widths(pend, len(cells))
            for k, cell in enumerate(cells):
                for j, part in enumerate(cell.split('\n')):
                    t = ('P',) if j else ('R', 0, ws[0]) if not k else ('C', ws[k])
                    para(part.strip(), t=t)
            if i >= len(lines) or not lines[i].startswith('|'):
                pend = None             # (the table's end)
            continue
        m = re.match(r'(=+) (.*)', ln)
        if m:
            k = min(3, len(m.group(1)))
            para(m.group(2), cwrd.NAMED[k], size=H_SIZE[k], flags=1)
        elif ln.startswith('* '):
            para(ln[2:], cwrd.NAMED[5])
        elif ln.startswith('>'):
            para(ln[1:], RIGHT)
        else:
            para(ln)
    # (a table last: an empty paragraph after it, as Veranda Write keeps one)
    if tcodes and tcodes[-1]:
        para('')
    return chars, paras, tcodes, meta


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    n = 0
    for lang in sorted(os.listdir(src)):
        d = os.path.join(src, lang)
        if not os.path.isdir(d):
            continue
        od = os.path.join(out, lang.upper())
        os.makedirs(od, exist_ok=True)
        for f in sorted(os.listdir(d)):
            if not f.endswith('.txt'):
                continue
            chars, paras, tcodes, meta = make(os.path.join(d, f))
            name = (meta['file'] or os.path.splitext(f)[0]) + '.cwr'
            path = os.path.join(od, name)
            cwrd.write(path, chars, paras=paras, minor=6, page=meta['page'], lang=meta['lang'], tcodes=tcodes)
            d2 = cwrd.read(path)             # (read back: well formed, its tables)
            d2.chars()
            rows, cells, _ = cwrd.tab_sum(d2.tables())
            print('templates: %s/%s: %d characters, %d paragraphs, %d table rows, %d cells'
                  % (lang.upper(), name, len(chars), len(paras), rows, cells))
            n += 1
    if not n:
        sys.exit('templates.py: no templates in %s' % src)


if __name__ == '__main__':
    main()
