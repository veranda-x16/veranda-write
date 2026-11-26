#!/usr/bin/env python3
"""Veranda Write: the XML text of the .docx and .odt it exports, as
templates (stage 4, W52-W53): the parts that are the same in every file
and the pieces src/oexp.c puts together (a paragraph's properties, a
run's ...), compressed with a dictionary into src/oparts.h (the Makefile
makes it; src/oparts.c expands them).

usage: oparts.py <out.h>

A template's text has {name} where oexp.c puts a value (its val(): a
number, the text, another template ...): the value's number, 1-30 (but
10, \\n) a byte, 31 and up two (31, n - 31). The dictionary: up to 127
pieces of text each one byte ($80-$FE) in the templates (its pieces made
greedily: the most bytes saved first). The templates' and the values'
numbers: #defines in the header (T_name, V_name).

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import re
import sys

XML = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
OXML = '<?xml version="1.0" encoding="UTF-8"?>\n'
WNS = 'xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"'
RNS = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'
CTP = 'application/vnd.openxmlformats-officedocument.wordprocessingml.'
PNS = 'http://schemas.openxmlformats.org/package/2006/relationships'
ONS = ('xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" '
       'xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0" '
       'xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" '
       'xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" '
       'xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" '
       'xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0" office:version="1.2"')
FFD = ('<style:font-face style:name="Arial" svg:font-family="Arial" style:font-family-generic="swiss" '
       'style:font-pitch="variable"/><style:font-face style:name="Courier New" '
       'svg:font-family="&apos;Courier New&apos;" style:font-family-generic="modern" style:font-pitch="fixed"/>')
BULLET = '•'


def hstyle(n, lvl, sz):
    return ('<w:style w:type="paragraph" w:styleId="Heading%s"><w:name w:val="heading %s"/><w:basedOn w:val="Normal"/>'
            '<w:next w:val="Normal"/><w:qFormat/><w:pPr><w:keepNext/><w:outlineLvl w:val="%s"/></w:pPr>'
            '<w:rPr><w:b/><w:sz w:val="%s"/></w:rPr></w:style>' % (n, n, lvl, sz))


def pstyle(sid, name, rest=''):
    return ('<w:style w:type="paragraph" w:styleId="%s"><w:name w:val="%s"/><w:basedOn w:val="Normal"/>%s</w:style>'
            % (sid, name, rest))


def lvl(fmt, txt, hang):
    return ('<w:multiLevelType w:val="singleLevel"/><w:lvl w:ilvl="0"><w:start w:val="1"/><w:numFmt w:val="%s"/>'
            '<w:lvlText w:val="%s"/><w:lvlJc w:val="left"/><w:pPr><w:ind w:left="576" w:hanging="%s"/></w:pPr></w:lvl>'
            '</w:abstractNum>' % (fmt, txt, hang))


def olist(name, kind, attr, ind):
    return ('<text:list-style style:name="%s"><text:list-level-style-%s text:level="1" %s>'
            '<style:list-level-properties text:list-level-position-and-space-mode="label-alignment">'
            '<style:list-level-label-alignment text:label-followed-by="listtab" text:list-tab-stop-position="0.4in" '
            'fo:text-indent="%s" fo:margin-left="0.4in"/></style:list-level-properties></text:list-level-style-%s>'
            '</text:list-style>' % (name, kind, attr, ind, kind))


def ostyle(n, d, p):
    return ('<style:style style:name="%s" style:display-name="%s" style:family="paragraph" '
            'style:parent-style-name="%s"' % (n, d, p))


def ohead(n, z):
    return (ostyle('Heading_20_' + n, 'Heading ' + n, 'Standard') + ' style:default-outline-level="%s">'
            '<style:paragraph-properties fo:keep-with-next="always"/><style:text-properties fo:font-size="%spt" '
            'fo:font-weight="bold"/></style:style>' % (n, z))


def otabs(n, tabs):
    return (ostyle(n, n, 'Standard') + '><style:paragraph-properties><style:tab-stops>%s</style:tab-stops>'
            '</style:paragraph-properties><style:text-properties fo:font-size="9pt"/></style:style>' % tabs)


TC = '<style:tab-stop style:position="{tabc}" style:type="center"/>'
TR = '<style:tab-stop style:position="{tabr}" style:type="right"/>'

# the templates (their order: oexp.c's part numbers first, OP_ in office.h)
T = [
    # .docx's parts
    ('ct', XML + '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
     '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
     '<Default Extension="xml" ContentType="application/xml"/>'
     '<Override PartName="/word/document.xml" ContentType="' + CTP + 'document.main+xml"/>'
     '<Override PartName="/word/styles.xml" ContentType="' + CTP + 'styles+xml"/>'
     '<Override PartName="/word/settings.xml" ContentType="' + CTP + 'settings+xml"/>'
     '<Override PartName="/word/numbering.xml" ContentType="' + CTP + 'numbering+xml"/>{cth}{ctf}</Types>'),
    ('rels', XML + '<Relationships xmlns="' + PNS + '"><Relationship Id="rId1" Type="' + RNS + '/officeDocument" '
     'Target="word/document.xml"/></Relationships>'),
    ('drel', XML + '<Relationships xmlns="' + PNS + '"><Relationship Id="rId1" Type="' + RNS + '/styles" Target="styles.xml"/>'
     '<Relationship Id="rId2" Type="' + RNS + '/settings" Target="settings.xml"/>'
     '<Relationship Id="rId3" Type="' + RNS + '/numbering" Target="numbering.xml"/>{rlh}{rlf}</Relationships>'),
    ('doc', XML + '<w:document ' + WNS + ' xmlns:r="' + RNS + '"><w:body>{body}<w:sectPr>{href}{fref}'
     '<w:pgSz w:w="{pw}" w:h="{ph}"/><w:pgMar w:top="{mt}" w:right="{mr}" w:bottom="{mb}" w:left="{ml}" '
     'w:header="{mh}" w:footer="{mf}" w:gutter="0"/>{titlepg}</w:sectPr></w:body></w:document>'),
    ('sty', XML + '<w:styles ' + WNS + '><w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii="Arial" w:hAnsi="Arial" '
     'w:eastAsia="Arial" w:cs="Arial"/><w:sz w:val="24"/><w:szCs w:val="24"/></w:rPr></w:rPrDefault>'
     '<w:pPrDefault><w:pPr><w:spacing w:after="0" w:line="240" w:lineRule="auto"/></w:pPr></w:pPrDefault>'
     '</w:docDefaults><w:style w:type="paragraph" w:default="1" w:styleId="Normal"><w:name w:val="Normal"/>'
     '<w:qFormat/></w:style>' + hstyle('1', '0', '54') + hstyle('2', '1', '36') + hstyle('3', '2', '24')
     + pstyle('Quote', 'Quote', '<w:next w:val="Normal"/><w:qFormat/><w:rPr><w:i/></w:rPr>')
     + pstyle('ListBullet', 'List Bullet') + pstyle('ListNumber', 'List Number')
     + pstyle('Header', 'header', '<w:rPr><w:sz w:val="18"/></w:rPr>')
     + pstyle('Footer', 'footer', '<w:rPr><w:sz w:val="18"/></w:rPr>') + '</w:styles>'),
    ('set', XML + '<w:settings ' + WNS + '><w:defaultTabStop w:val="720"/><w:characterSpacingControl w:val="doNotCompress"/>'
     '<w:compat><w:compatSetting w:name="compatibilityMode" w:uri="http://schemas.microsoft.com/office/word" '
     'w:val="15"/></w:compat></w:settings>'),
    ('num', XML + '<w:numbering ' + WNS + '><w:abstractNum w:abstractNumId="0">' + lvl('bullet', BULLET, '288')
     + '<w:abstractNum w:abstractNumId="1">' + lvl('decimal', '%1.', '432')
     + '<w:num w:numId="1"><w:abstractNumId w:val="0"/></w:num>{nums}</w:numbering>'),
    ('hf', XML + '<w:{hft} ' + WNS + ' xmlns:r="' + RNS + '"><w:p><w:pPr><w:pStyle w:val="{hfs}"/><w:tabs>{dtabc}'
     '<w:tab w:val="right" w:pos="{dtabr}"/></w:tabs>{hjc}</w:pPr>{htext}</w:p></w:{hft}>'),
    ('_8', ''),
    ('mime', 'application/vnd.oasis.opendocument.text'),
    # .odt's parts
    ('man', OXML + '<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0" '
     'manifest:version="1.2"><manifest:file-entry manifest:full-path="/" manifest:version="1.2" '
     'manifest:media-type="application/vnd.oasis.opendocument.text"/>'
     '<manifest:file-entry manifest:full-path="content.xml" manifest:media-type="text/xml"/>'
     '<manifest:file-entry manifest:full-path="styles.xml" manifest:media-type="text/xml"/>'
     '<manifest:file-entry manifest:full-path="meta.xml" manifest:media-type="text/xml"/></manifest:manifest>'),
    ('ocon', OXML + '<office:document-content ' + ONS + '><office:font-face-decls>' + FFD
     + '</office:font-face-decls><office:automatic-styles>{ostyles}'
     + olist('LB', 'bullet', 'text:bullet-char="' + BULLET + '"', '-0.2in')
     + olist('LN', 'number', 'style:num-suffix="." style:num-format="1"', '-0.3in')
     + '<style:style style:name="Tb" style:family="table"><style:table-properties table:align="left"/></style:style>'
     + ''.join('<style:style style:name="Tc%s" style:family="table-cell"><style:table-cell-properties '
               'fo:padding="0.04in" fo:border="%s"/></style:style>' % (k, b) for k, b in (('G', '0.5pt solid #000000'), ('N', 'none')))
     + '</office:automatic-styles><office:body><office:text>{body}</office:text></office:body></office:document-content>'),
    ('osty', OXML + '<office:document-styles ' + ONS + '><office:font-face-decls>' + FFD
     + '</office:font-face-decls><office:styles><style:default-style style:family="paragraph">'
     '<style:paragraph-properties style:tab-stop-distance="0.5in"/><style:text-properties style:font-name="Arial" '
     'fo:font-size="12pt"/></style:default-style><style:style style:name="Standard" style:family="paragraph"/>'
     + ohead('1', '27') + ohead('2', '18') + ohead('3', '12')
     + ostyle('Quotations', 'Quotations', 'Standard') + '><style:text-properties fo:font-style="italic"/></style:style>'
     + otabs('Header', TC + TR) + otabs('Footer', TC + TR) + otabs('Header2', TR) + otabs('Footer2', TR)
     + '<style:style style:name="HC" style:family="paragraph" style:parent-style-name="Header">'
     '<style:paragraph-properties fo:text-align="center"/></style:style></office:styles><office:automatic-styles>'
     '<style:page-layout style:name="pm1"><style:page-layout-properties {opage} fo:margin-top="{omt}" '
     'fo:margin-bottom="{omb}"/><style:header-style><style:header-footer-properties fo:min-height="{ohr}" '
     'fo:margin-bottom="0in" style:dynamic-spacing="false"/></style:header-style><style:footer-style>'
     '<style:header-footer-properties fo:min-height="{ofr}" fo:margin-top="0in" style:dynamic-spacing="false"/>'
     '</style:footer-style></style:page-layout><style:page-layout style:name="pm2"><style:page-layout-properties '
     '{opage} fo:margin-top="{omt1}" fo:margin-bottom="{omb1}"/></style:page-layout></office:automatic-styles>'
     '<office:master-styles><style:master-page style:name="Standard" style:page-layout-name="pm1">{ohf}'
     '</style:master-page>{ofirst}</office:master-styles></office:document-styles>'),
    ('meta', OXML + '<office:document-meta xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" '
     'xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0" xmlns:dc="http://purl.org/dc/elements/1.1/" '
     'office:version="1.2"><office:meta><meta:generator>Veranda Write</meta:generator><dc:title>{title}</dc:title>'
     '</office:meta></office:document-meta>'),
    # pieces: the .docx's
    ('cth', '<Override PartName="/word/header1.xml" ContentType="' + CTP + 'header+xml"/>'),
    ('ctf', '<Override PartName="/word/footer1.xml" ContentType="' + CTP + 'footer+xml"/>'),
    ('rlh', '<Relationship Id="rId4" Type="' + RNS + '/header" Target="header1.xml"/>'),
    ('rlf', '<Relationship Id="rId5" Type="' + RNS + '/footer" Target="footer1.xml"/>'),
    ('href', '<w:headerReference w:type="default" r:id="rId4"/>'),
    ('fref', '<w:footerReference w:type="default" r:id="rId5"/>'),
    ('titlepg', '<w:titlePg/>'),
    ('dnum', '<w:num w:numId="{n}"><w:abstractNumId w:val="1"/><w:lvlOverride w:ilvl="0"><w:startOverride w:val="1"/>'
     '</w:lvlOverride></w:num>'),
    ('dp', '<w:p><w:pPr>{dps}<w:spacing w:before="{bef}" w:after="{aft}" w:line="{line}" w:lineRule="auto"/>'
     '<w:ind w:left="{il}" w:right="{ir}" {dfirst}/>{djc}</w:pPr>'),
    ('dpb', '<w:p><w:r><w:br w:type="page"/></w:r>'),
    ('dhead', '<w:pStyle w:val="Heading{n}"/>'),
    ('dquote', '<w:pStyle w:val="Quote"/>'),
    ('dbul', '<w:pStyle w:val="ListBullet"/><w:numPr><w:ilvl w:val="0"/><w:numId w:val="1"/></w:numPr>'),
    ('dnumb', '<w:pStyle w:val="ListNumber"/><w:numPr><w:ilvl w:val="0"/><w:numId w:val="{n}"/></w:numPr>'),
    ('dhang', 'w:hanging="{n}"'),
    ('dfirstl', 'w:firstLine="{n}"'),
    ('djc', '<w:jc w:val="{jc}"/>'),
    ('center', 'center'),
    ('right', 'right'),
    ('both', 'both'),
    ('dr', '<w:r><w:rPr><w:rFonts w:ascii="{font}" w:hAnsi="{font}"/>{rb}{ri}<w:sz w:val="{n}"/>{ru}</w:rPr>'
     '<w:t xml:space="preserve">'),
    ('db', '<w:b/>'),
    ('di', '<w:i/>'),
    ('du', '<w:u w:val="single"/>'),
    ('dre', '</w:t></w:r>'),
    ('dtab', '</w:t><w:tab/><w:t xml:space="preserve">'),
    ('dpe', '</w:p>'),
    ('dpempty', '<w:p/>'),
    ('arial', 'Arial'),
    ('courier', 'Courier New'),
    ('dtabc', '<w:tab w:val="center" w:pos="{n}"/>'),
    ('dhrun', '<w:r><w:t xml:space="preserve">'),
    ('dhtab', '<w:r><w:tab/></w:r>'),
    ('dfield', '<w:fldSimple w:instr="{finstr}"><w:r><w:t>{fres}</w:t></w:r></w:fldSimple>'),
    ('fi1', ' PAGE '),
    ('fi2', ' NUMPAGES '),
    ('fi3', ' DATE \\@ &quot;d MMMM yyyy&quot; '),
    ('fi4', ' FILENAME '),
    ('header', 'Header'),
    ('footer', 'Footer'),
    ('hdr', 'hdr'),
    ('ftr', 'ftr'),
    # pieces: the .odt's
    ('ops', '<style:style style:name="P{pname}" style:family="paragraph" style:parent-style-name="{opar}"{omp}>'
     '<style:paragraph-properties fo:margin-left="{il}" fo:margin-right="{ir}" fo:text-indent="{if}" '
     'fo:margin-top="{bef}" fo:margin-bottom="{aft}" fo:line-height="{lh}%" fo:text-align="{al}"{obrk}/>'
     '</style:style>'),
    ('omp', ' style:master-page-name="Write_20_First"'),
    ('obrk', ' fo:break-before="page"'),
    ('ots', '<style:style style:name="T{n}" style:family="text"><style:text-properties style:font-name="{font}" '
     'fo:font-size="{sz}pt" fo:font-weight="{wt}" fo:font-style="{st}" style:text-underline-style="{ul}"/></style:style>'),
    ('standard', 'Standard'),
    ('head1', 'Heading_20_1'),
    ('head2', 'Heading_20_2'),
    ('head3', 'Heading_20_3'),
    ('quotations', 'Quotations'),
    ('start', 'start'),
    ('end', 'end'),
    ('justify', 'justify'),
    ('bold', 'bold'),
    ('normal', 'normal'),
    ('italic', 'italic'),
    ('solid', 'solid" style:text-underline-width="auto" style:text-underline-color="font-color'),
    ('none', 'none'),
    ('op', '<text:p text:style-name="P{pref}">'),
    ('oh', '<text:h text:style-name="P{pref}" text:outline-level="{n}">'),
    ('ope', '</text:p>'),
    ('ohe', '</text:h>'),
    ('olb', '<text:list text:style-name="LB">'),
    ('oln', '<text:list text:style-name="LN">'),
    ('oli', '<text:list-item>'),
    ('olie', '</text:list-item>'),
    ('ole', '</text:list>'),
    ('ospan', '<text:span text:style-name="T{n}">'),
    ('ospe', '</text:span>'),
    ('otab', '<text:tab/>'),
    ('os', '<text:s/>'),
    ('opb', '<text:p text:style-name="P0b"/>'),
    ('opempty', '<text:p text:style-name="P0"/>'),
    ('opage', 'fo:page-width="{pw}" fo:page-height="{ph}" fo:margin-left="{ml}" fo:margin-right="{mr}"'),
    ('ohdr', '<style:header>{ohp}</style:header>'),
    ('oftr', '<style:footer>{ohp}</style:footer>'),
    ('ohp1', '<text:p text:style-name="HC">{htext}</text:p>'),
    ('ohp', '<text:p text:style-name="{hfs}{h2}">{htext}</text:p>'),
    ('ofirst', '<style:master-page style:name="Write_20_First" style:display-name="Write First" '
     'style:page-layout-name="pm2" style:next-style-name="Standard"/>'),
    ('of1', '<text:page-number text:select-page="current">1</text:page-number>'),
    ('of2', '<text:page-count>1</text:page-count>'),
    ('of3', '<text:date>{fres}</text:date>'),
    ('of4', '<text:file-name text:display="name">{fres}</text:file-name>'),
    # (1.6) tables: the .docx's (a grid of the first row's widths, each
    # cell its own; borders single or nil) and the .odt's (a column
    # style a width used, Tw<256ths>; the cells' TcG, TcN)
    ('dtbl', '<w:tbl><w:tblPr><w:tblW w:w="0" w:type="auto"/><w:tblBorders><w:top w:val="{bv}" w:sz="4"/><w:left '
     'w:val="{bv}" w:sz="4"/><w:bottom w:val="{bv}" w:sz="4"/><w:right w:val="{bv}" w:sz="4"/><w:insideH w:val="{bv}" '
     'w:sz="4"/><w:insideV w:val="{bv}" w:sz="4"/></w:tblBorders><w:tblLayout w:type="fixed"/></w:tblPr><w:tblGrid>'
     '{grid}</w:tblGrid>'),
    ('dgc', '<w:gridCol w:w="{tw}"/>'),
    ('dtr', '<w:tr>'),
    ('dtc', '<w:tc><w:tcPr><w:tcW w:w="{tw}" w:type="dxa"/></w:tcPr>'),
    ('dtce', '</w:tc>'),
    ('dtre', '</w:tr>'),
    ('dtble', '</w:tbl>'),
    # (the .odt's in the same order: oexp.c's T_DTBL + 7 ...)
    ('otbl', '<table:table table:name="Table{n}" table:style-name="Tb">{grid}'),
    ('ocol', '<table:table-column table:style-name="Tw{n}"/>'),
    ('otr', '<table:table-row>'),
    ('otc', '<table:table-cell table:style-name="Tc{tcs}" office:value-type="string">'),
    ('otce', '</table:table-cell>'),
    ('otre', '</table:table-row>'),
    ('otble', '</table:table>'),
    ('single', 'single'),
    ('nil', 'nil'),
    ('ocols', '<style:style style:name="Tw{n}" style:family="table-column"><style:table-column-properties '
     'style:column-width="{tw}"/></style:style>'),
]

# the values (oexp.c's val(): their numbers)
V = '''title body href fref pw ph mt mr mb ml mh mf titlepg cth ctf rlh rlf nums hft hfs dtabc dtabr hjc htext
n dps bef aft line il ir dfirst djc jc font rb ri ru finstr fres ostyles opage omt omb ohr ofr omt1 omb1 ohf ofirst
pname opar omp if lh al obrk sz wt st ul pref tabc tabr ohp h2 bv grid tw tcs'''.split()


def vcode(name):
    n = V.index(name) + 1
    if n >= 10:
        n += 1                          # (10 is \n)
    return bytes([n]) if n < 31 else bytes([31, n - 31])


def encode(text):
    out = bytearray()
    for part in re.split(r'(\{[a-z0-9]+\})', text):
        if part.startswith('{') and part.endswith('}') and part[1:-1] in V:
            out += vcode(part[1:-1])
        else:
            out += part.encode('utf-8')
    return bytes(out)


def main():
    blobs = [encode(t) for _, t in T]
    # the UTF-8 bullet (bytes >= $80) is a piece of its own, first
    bul = BULLET.encode('utf-8')
    pieces0 = [bul]
    blobs = [b.replace(bul, bytes([0x80])) for b in blobs]
    pieces, out = build_dict_shift(blobs, pieces0)
    raw = sum(len(b) for b in blobs)
    comp = sum(len(b) for b in out) + sum(len(p) + 1 for p in pieces)
    lines = ['/* made by tools/oparts.py: do not edit (%d bytes of templates, %d compressed with its dictionary) */'
             % (raw, comp)]
    for i, (name, _) in enumerate(T):
        lines.append('#define T_%s %d' % (name.upper(), i))
    lines.append('#define T_N %d' % len(T))
    for i, v in enumerate(V):
        n = i + 1
        if n >= 10:
            n += 1
        lines.append('#define V_%s %d' % (v.upper(), n))
    lines.append('#ifdef OPARTS_DATA')
    allb = bytearray()
    offs = []
    for b in out:
        offs.append(len(allb))
        allb += b
    offs.append(len(allb))
    lines.append('static const unsigned int t_off[%d] = { %s };' % (len(offs), ', '.join(map(str, offs))))
    lines.append('static const unsigned char t_bytes[%d] = {' % len(allb))
    for i in range(0, len(allb), 24):
        lines.append('    ' + ', '.join('%d' % c for c in allb[i:i + 24]) + ',')
    lines.append('};')
    db = bytearray()
    doff = []
    for p in pieces:
        doff.append(len(db))
        db += p
    doff.append(len(db))
    lines.append('const unsigned int d_off[%d] = { %s };' % (len(doff), ', '.join(map(str, doff))))
    lines.append('const unsigned char d_bytes[%d] = {' % len(db))
    for i in range(0, len(db), 24):
        lines.append('    ' + ', '.join('%d' % c for c in db[i:i + 24]) + ',')
    lines.append('};')
    lines.append('#endif')
    open(sys.argv[1], 'w').write('\n'.join(lines) + '\n')
    # (a check: expanded, each template is its text)
    for (name, t), b in zip(T, out):
        if expand(b, pieces) != encode(t):
            sys.exit('oparts: %s does not expand to itself' % name)


def build_dict_shift(blobs, first):
    """the dictionary: first's pieces ($80 ...) then the greedy ones"""
    cur = [bytearray(b) for b in blobs]
    pieces = list(first)
    while len(pieces) < 127:
        tok = 0x80 + len(pieces)
        count = {}
        for b in cur:
            L = len(b)
            for ln in range(4, 48):
                for i in range(0, L - ln + 1):
                    s = bytes(b[i:i + ln])
                    if any(c >= 0x80 or c < 0x20 for c in s):
                        continue
                    count[s] = count.get(s, 0) + 1
        best, gain = None, 0
        for s, c in count.items():
            g = c * (len(s) - 1) - len(s) - 2
            if g > gain:
                best, gain = s, g
        if not best:
            break
        pieces.append(best)
        for j, b in enumerate(cur):
            cur[j] = bytearray(bytes(b).replace(best, bytes([tok])))
    return pieces, [bytes(b) for b in cur]


def expand(b, pieces):
    out = bytearray()
    for c in b:
        out += pieces[c - 0x80] if c >= 0x80 else bytes([c])
    return bytes(out)


if __name__ == '__main__':
    main()
