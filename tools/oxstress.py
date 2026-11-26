#!/usr/bin/env python3
"""Veranda Write's tests (W71, may be renumbered): .docx files for the
X16's assembly (v/oinf.s's inflate, v/oxtok.s's tokenizer, the elements
passed over whole), opened by the gate's SESSION=oxspeed and compared
with the host's conversion (the C):

  Stress.docx            XML as Word and other programs may write it:
                         references, characters the set has not (in
                         text, in deleted text, in attributes), texts
                         longer than the tokens' buffer, comments, a
                         processing instruction and CDATA inside elements
                         passed over and outside, quotes with > and / in
                         values, spaces before />, long names, a table in
                         a table, nine list levels; its parts deflated
                         three ways (dynamic codes, fixed codes, stored
                         blocks)
  Fixed codes.docx       the Guide (tests/office) with fixed codes only
  Kept in blocks.docx    the Guide in stored deflate blocks

usage: oxstress.py <folder>

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import os
import sys
import zipfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import office_fix  # noqa: E402
from office_fix import W, XMLH, RELNS, DOCX_STYLES, esc  # noqa: E402

GUIDE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tests', 'office',
                     'Veranda Desktop Guide.docx')


def deflate(data, how):
    """raw DEFLATE: 'dyn' (zlib's), 'fixed' (Z_FIXED), 'stored' (level 0)"""
    if how == 'stored':
        c = zlib.compressobj(0, zlib.DEFLATED, -15)
    elif how == 'fixed':
        c = zlib.compressobj(9, zlib.DEFLATED, -15, 9, zlib.Z_FIXED)
    else:
        c = zlib.compressobj(9, zlib.DEFLATED, -15)
    return c.compress(data) + c.flush()


def zipwrite(path, parts):
    """parts: (name, bytes, how): a ZIP of deflated entries, each its way"""
    out = bytearray()
    cd = bytearray()
    for name, data, how in parts:
        n = name.encode()
        z = deflate(data, how)
        crc = zlib.crc32(data) & 0xFFFFFFFF
        off = len(out)
        out += b'PK\3\4' + (20).to_bytes(2, 'little') + bytes(2) + (8).to_bytes(2, 'little') + bytes(4) \
            + crc.to_bytes(4, 'little') + len(z).to_bytes(4, 'little') + len(data).to_bytes(4, 'little') \
            + len(n).to_bytes(2, 'little') + bytes(2) + n + z
        cd += b'PK\1\2' + (20).to_bytes(2, 'little') + (20).to_bytes(2, 'little') + bytes(2) \
            + (8).to_bytes(2, 'little') + bytes(4) + crc.to_bytes(4, 'little') + len(z).to_bytes(4, 'little') \
            + len(data).to_bytes(4, 'little') + len(n).to_bytes(2, 'little') + bytes(8) + bytes(4) \
            + off.to_bytes(4, 'little') + n
    end = b'PK\5\6' + bytes(4) + len(parts).to_bytes(2, 'little') * 2 + len(cd).to_bytes(4, 'little') \
        + len(out).to_bytes(4, 'little') + bytes(2)
    open(path, 'wb').write(bytes(out + cd + end))


def rezip(src, dst, how):
    with zipfile.ZipFile(src) as z:
        zipwrite(dst, [(i.filename, z.read(i), how) for i in z.infolist()])


def tc(content, borders=True):
    b = ('<w:tcBorders><w:top w:val="single" w:sz="4" w:color="a/b>c"/><!-- a comment > there -->'
         '<w:left w:val=\'double\' w:sz="2" /><?pi in a border?><w:bottom w:val="nil"/>'
         '<w:right w:val="single" w:color="&quot;/&gt;"/></w:tcBorders>') if borders else ''
    return '<w:tc><w:tcPr><w:tcW w:w="2000" w:type="dxa"/>%s<w:vAlign w:val="center"/></w:tcPr>%s</w:tc>' % (b, content)


def p(t, extra=''):
    return '<w:p><w:pPr><w:pStyle w:val="Normal"/><w:tabs><w:tab w:val="left" w:pos="709"/></w:tabs>' \
           '<w:rPr><w:b/></w:rPr></w:pPr>%s<w:r><w:rPr></w:rPr><w:t xml:space="preserve">%s</w:t></w:r></w:p>' % (extra, t)


def stress_doc():
    long1 = esc('Zażółć gęślą jaźń. ' * 20) + '&#x2192;' + esc(' end of a long text, ') * 10
    long2 = ''.join('%c' % c for c in 'ĄĘÓŁŚŻŹĆŃ ąęółśżźćń → ✓ 😀 ' * 12)
    body = [
        p('References: A &amp; B &lt;tag&gt; &quot;q&quot; &apos;a&apos; &#x17C;&#378; &#169; &#8364; &#1F600; &unknown; done'),
        p(esc('Not in the set: → ✓ 😀 € ő ű ŧ ✗ and Polish ąę')),
        p(long1),
        p(esc(long2)),
        '<!-- a comment in the body, with <w:p> in it -- and dashes --><?pi in the body?>',
        '<w:p><w:r><w:t>Kept </w:t></w:r><w:del w:id="1" w:author="A"><w:r><w:delText>deleted → ✓ 😀 '
        '<![CDATA[ x ]]> &#x2603;</w:delText></w:r><!-- in a deletion --><?pi in a deletion?></w:del>'
        '<w:ins w:id="2" w:author="A"><w:r><w:t>inserted</w:t></w:r></w:ins></w:p>',
        '<w:p><w:r><w:rPr><w:b /><w:i w:val=\'1\' /></w:rPr><w:t>bold italic</w:t></w:r><w:r><w:tab/><w:t>tab</w:t>'
        '<w:br/><w:t>line</w:t><w:cr/><w:noBreakHyphen/><w:sym w:font="Symbol" w:char="F0B7"/></w:r></w:p>',
        '<w:p><w:r><w:fldChar w:fldCharType="begin"/></w:r><w:r><w:instrText> PAGE </w:instrText></w:r>'
        '<w:r><w:fldChar w:fldCharType="separate"/></w:r><w:r><w:t>7</w:t></w:r>'
        '<w:r><w:fldChar w:fldCharType="end"/></w:r></w:p>',
        '<w:p><mc:AlternateContent><mc:Choice Requires="wps"><w:r><w:t>choice</w:t></w:r></mc:Choice>'
        '<mc:Fallback><w:r><w:t>fallback → </w:t></w:r></mc:Fallback></mc:AlternateContent></w:p>',
        '<w:sdt><w:sdtPr><w:alias w:val="x/y>z"/><w:id w:val="5"/></w:sdtPr><w:sdtContent>%s</w:sdtContent></w:sdt>'
        % p('in a content control'),
        '<w:p><w:pPr><w:aVeryLongElementNameThatIsLongerThanTheBuffer w:val="1" w:anAttributeNameLongerThanTheNameBuffer="2"/>'
        '</w:pPr><w:r><w:t>long names</w:t></w:r></w:p>',
        '<w:tbl><w:tblPr><w:tblW w:w="0" w:type="auto"/><w:tblCellMar><w:top w:w="28" w:type="dxa"/>'
        '<w:left w:w="28" w:type="dxa"/></w:tblCellMar></w:tblPr><w:tblGrid><w:gridCol w:w="3000"/>'
        '<w:gridCol w:w="3000"/></w:tblGrid><w:tr><w:trPr><w:tblHeader/></w:trPr>'
        + tc(p('cell → one')) + tc(p('cell two'), borders=False) + '</w:tr><w:tr>'
        + tc('<w:tbl><w:tblPr/><w:tblGrid><w:gridCol w:w="1000"/></w:tblGrid><w:tr>' + tc(p('nested ✓'))
             + '</w:tr></w:tbl>' + p('after the nested'))
        + '<w:tc><w:tcPr><w:gridSpan w:val="1"/><w:tcBorders/></w:tcPr>' + p('spanned') + '</w:tc></w:tr></w:tbl>',
        p('list', '<w:pPr><w:numPr><w:ilvl w:val="3"/><w:numId w:val="1"/></w:numPr></w:pPr>'),
        p('numbered', '<w:pPr><w:numPr><w:ilvl w:val="0"/><w:numId w:val="2"/></w:numPr></w:pPr>'),
        p('the end'),
    ]
    return (XMLH + '<w:document %s><w:body>%s<w:sectPr><w:pgSz w:w="11906" w:h="16838"/>'
            '<w:pgMar w:top="1417" w:right="1417" w:bottom="1417" w:left="1417" w:header="708" w:footer="708"'
            ' w:gutter="0"/></w:sectPr></w:body></w:document>' % (W, ''.join(body))).encode()


def stress_num():
    lv = ''.join('<w:lvl w:ilvl="%d"><w:start w:val="1"/><w:numFmt w:val="%s"/><w:lvlText w:val="%%%d."/>'
                 '<w:lvlJc w:val="left"/><w:pPr><w:tabs><w:tab w:val="num" w:pos="%d"/></w:tabs>'
                 '<w:ind w:left="%d" w:hanging="360"/></w:pPr><w:rPr><w:rFonts w:ascii="Symbol" w:hAnsi="Symbol"/>'
                 '</w:rPr></w:lvl>' % (k, 'bullet' if k % 2 == 0 else 'decimal', k + 1, 720 * (k + 1), 720 * (k + 1))
                 for k in range(9))
    lv2 = lv.replace('"bullet"', '"x"').replace('"decimal"', '"bullet"').replace('"x"', '"decimal"')
    return (XMLH + '<w:numbering %s><w:abstractNum w:abstractNumId="0">%s</w:abstractNum>'
            '<w:abstractNum w:abstractNumId="1">%s</w:abstractNum><w:num w:numId="1"><w:abstractNumId w:val="0"/>'
            '<w:lvlOverride w:ilvl="0"><w:startOverride w:val="2"/></w:lvlOverride></w:num>'
            '<w:num w:numId="2"><w:abstractNumId w:val="1"/></w:num></w:numbering>' % (W, lv, lv2)).encode()


def stress(path):
    ct = (XMLH + '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
          '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
          '<Default Extension="xml" ContentType="application/xml"/>'
          '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.'
          'wordprocessingml.document.main+xml"/></Types>').encode()
    top = (XMLH + '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
           '<Relationship Id="rId1" Type="%s/officeDocument" Target="word/document.xml"/></Relationships>'
           % RELNS).encode()
    zipwrite(path, [('[Content_Types].xml', ct, 'dyn'), ('_rels/.rels', top, 'fixed'),
                    ('word/styles.xml', DOCX_STYLES.encode(), 'stored'), ('word/numbering.xml', stress_num(), 'fixed'),
                    ('word/document.xml', stress_doc(), 'dyn')])


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    d = sys.argv[1]
    os.makedirs(d, exist_ok=True)
    stress(os.path.join(d, 'Stress.docx'))
    rezip(GUIDE, os.path.join(d, 'Fixed codes.docx'), 'fixed')
    rezip(GUIDE, os.path.join(d, 'Kept in blocks.docx'), 'stored')


if __name__ == '__main__':
    main()
