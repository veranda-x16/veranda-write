#!/usr/bin/env python3
"""Veranda Write's tests: .docx and .odt documents to open (stage 4,
W49-W54), written as Word and LibreOffice write them (the XML by hand,
zipped with deflate), and what Veranda Write must make of each.

usage: office_fix.py <folder>          the fixtures and expect.json there

Each fixture's expectation (expect.json): its paragraphs, each (kind,
align, text) with a run of a style where it matters ('b', 'i', 'u',
'mono', size), the page breaks, the page setup (paper, margins, the
header and the footer) and the counts the conversion's note says
(tables, pictures, notes, comments, changes, characters the set has
not). tools/check_office.py converts them (build/host/test_cw ox) and
compares.

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import json
import os
import sys
import zipfile

W = 'xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" ' \
    'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" ' \
    'xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006" ' \
    'xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing" ' \
    'xmlns:wps="http://schemas.microsoft.com/office/word/2010/wordprocessingShape" ' \
    'xmlns:m="http://schemas.openxmlformats.org/officeDocument/2006/math" ' \
    'xmlns:w14="http://schemas.microsoft.com/office/word/2010/wordml" ' \
    'xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" ' \
    'xmlns:pic="http://schemas.openxmlformats.org/drawingml/2006/picture" ' \
    'xmlns:v="urn:schemas-microsoft-com:vml" xmlns:o="urn:schemas-microsoft-com:office:office"'
XMLH = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\r\n'
POL = 'Zażółć gęślą jaźń'
POL2 = 'ZAŻÓŁĆ GĘŚLĄ JAŹŃ'
RELNS = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'


def zipit(path, parts, stored_first=None):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        if stored_first:
            z.writestr(zipfile.ZipInfo(stored_first[0]), stored_first[1], compress_type=zipfile.ZIP_STORED)
        for name, data in parts:
            z.writestr(name, data)


def esc(t):
    return t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


# ---------------------------------------------------------------- .docx
def run(t, b=False, i=False, u=False, sz=None, font=None):
    rp = ''
    if font:
        rp += '<w:rFonts w:ascii="%s" w:hAnsi="%s" w:cs="%s"/>' % (font, font, font)
    if b:
        rp += '<w:b/><w:bCs/>'
    if i:
        rp += '<w:i/><w:iCs/>'
    if u:
        rp += '<w:u w:val="single"/>'
    if sz:
        rp += '<w:sz w:val="%d"/><w:szCs w:val="%d"/>' % (sz * 2, sz * 2)
    rp = '<w:rPr>%s<w:lang w:val="pl-PL"/></w:rPr>' % rp
    return '<w:r w:rsidR="00A1B2C3">%s<w:t xml:space="preserve">%s</w:t></w:r>' % (rp, esc(t))


def para(content, style=None, jc=None, extra=''):
    pp = ''
    if style:
        pp += '<w:pStyle w:val="%s"/>' % style
    pp += extra
    if jc:
        pp += '<w:jc w:val="%s"/>' % jc
    return '<w:p w:rsidR="00A1B2C3" w:rsidRDefault="00A1B2C3">%s%s</w:p>' % (
        '<w:pPr>%s<w:rPr><w:b/></w:rPr></w:pPr>' % pp if pp else '', content)


DOCX_STYLES = XMLH + '''<w:styles %s><w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:asciiTheme="minorHAnsi" w:eastAsiaTheme="minorHAnsi" w:hAnsiTheme="minorHAnsi" w:cstheme="minorBidi"/><w:sz w:val="22"/><w:szCs w:val="22"/><w:lang w:val="en-US" w:eastAsia="en-US" w:bidi="ar-SA"/></w:rPr></w:rPrDefault><w:pPrDefault><w:pPr><w:spacing w:after="160" w:line="259" w:lineRule="auto"/></w:pPr></w:pPrDefault></w:docDefaults><w:latentStyles w:defLockedState="0" w:defUIPriority="99" w:defSemiHidden="0" w:defUnhideWhenUsed="0" w:defQFormat="0" w:count="376"><w:lsdException w:name="Normal" w:uiPriority="0" w:qFormat="1"/><w:lsdException w:name="heading 1" w:uiPriority="9" w:qFormat="1"/></w:latentStyles>
<w:style w:type="paragraph" w:default="1" w:styleId="Normal"><w:name w:val="Normal"/><w:qFormat/></w:style>
<w:style w:type="paragraph" w:styleId="Nagwek1"><w:name w:val="heading 1"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:link w:val="Nagwek1Znak"/><w:uiPriority w:val="9"/><w:qFormat/><w:pPr><w:keepNext/><w:keepLines/><w:spacing w:before="240" w:after="0"/><w:outlineLvl w:val="0"/></w:pPr><w:rPr><w:rFonts w:asciiTheme="majorHAnsi" w:eastAsiaTheme="majorEastAsia" w:hAnsiTheme="majorHAnsi" w:cstheme="majorBidi"/><w:color w:val="2F5496" w:themeColor="accent1" w:themeShade="BF"/><w:sz w:val="32"/><w:szCs w:val="32"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading2"><w:name w:val="heading 2"/><w:basedOn w:val="Normal"/><w:pPr><w:outlineLvl w:val="1"/></w:pPr><w:rPr><w:sz w:val="26"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading3"><w:name w:val="heading 3"/><w:basedOn w:val="Normal"/><w:pPr><w:outlineLvl w:val="2"/></w:pPr><w:rPr><w:sz w:val="24"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Quote"><w:name w:val="Quote"/><w:basedOn w:val="Normal"/><w:pPr><w:ind w:left="864" w:right="864"/><w:jc w:val="center"/></w:pPr><w:rPr><w:i/><w:iCs/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="ListParagraph"><w:name w:val="List Paragraph"/><w:basedOn w:val="Normal"/><w:pPr><w:ind w:left="720"/><w:contextualSpacing/></w:pPr></w:style>
<w:style w:type="paragraph" w:styleId="Code"><w:name w:val="Code"/><w:basedOn w:val="Normal"/><w:rPr><w:rFonts w:ascii="Courier New" w:hAnsi="Courier New"/><w:sz w:val="20"/></w:rPr></w:style>
<w:style w:type="character" w:styleId="Strong"><w:name w:val="Strong"/><w:rPr><w:b/><w:bCs/></w:rPr></w:style>
<w:style w:type="table" w:default="1" w:styleId="TableNormal"><w:name w:val="Normal Table"/><w:tblPr><w:tblInd w:w="0" w:type="dxa"/></w:tblPr></w:style>
<w:style w:type="paragraph" w:styleId="Header"><w:name w:val="header"/><w:basedOn w:val="Normal"/><w:pPr><w:tabs><w:tab w:val="center" w:pos="4536"/><w:tab w:val="right" w:pos="9072"/></w:tabs><w:spacing w:after="0"/></w:pPr></w:style>
</w:styles>''' % W

DOCX_NUM = XMLH + '''<w:numbering %s><w:abstractNum w:abstractNumId="0" w15:restartNumberingAfterBreak="0"><w:nsid w:val="12345678"/><w:multiLevelType w:val="hybridMultilevel"/><w:lvl w:ilvl="0"><w:start w:val="1"/><w:numFmt w:val="bullet"/><w:lvlText w:val=""/><w:lvlJc w:val="left"/><w:pPr><w:ind w:left="720" w:hanging="360"/></w:pPr><w:rPr><w:rFonts w:ascii="Symbol" w:hAnsi="Symbol"/></w:rPr></w:lvl><w:lvl w:ilvl="1"><w:numFmt w:val="decimal"/></w:lvl></w:abstractNum><w:abstractNum w:abstractNumId="1"><w:lvl w:ilvl="0"><w:start w:val="1"/><w:numFmt w:val="decimal"/><w:lvlText w:val="%%1."/></w:lvl></w:abstractNum><w:num w:numId="1"><w:abstractNumId w:val="0"/></w:num><w:num w:numId="2"><w:abstractNumId w:val="1"/></w:num></w:numbering>''' % W.replace(
    'xmlns:w=', 'xmlns:w15="http://schemas.microsoft.com/office/word/2012/wordml" xmlns:w=')


def numpr(n):
    return '<w:numPr><w:ilvl w:val="0"/><w:numId w:val="%d"/></w:numPr>' % n


def docx_features(path):
    """every feature: what it must give"""
    body = []
    exp = []
    body.append(para(run('Title of the test'), 'Nagwek1'))
    exp.append({'kind': 1, 'text': 'Title of the test', 'runs': [['Title', 'b', 27]]})
    body.append(para(run('Plain, ') + run('bold, ', b=True) + run('italic, ', i=True) + run('underlined', u=True)
                     + run(' and ') + run('big', sz=20) + run(' and ') + run('small', sz=8) + run(' and ')
                     + run('mono', font='Courier New') + run(' ') + '<w:r><w:rPr><w:rStyle w:val="Strong"/></w:rPr><w:t>strong</w:t></w:r>'))
    exp.append({'kind': 0, 'after': 9, 'text': 'Plain, bold, italic, underlined and big and small and mono strong',
                'runs': [['Plain', '', 12], ['bold', 'b', 12], ['italic', 'i', 12], ['underlined', 'u', 12],
                         ['big', '', 18], ['small', '', 9], ['mono', 'mono', 10], ['strong', 'b', 12]]})
    body.append(para(run(POL + ' ') + run(POL2, b=True), jc='center'))
    exp.append({'kind': 0, 'align': 1, 'text': POL + ' ' + POL2})
    body.append(para(run('Right'), jc='right'))
    exp.append({'kind': 0, 'align': 2, 'text': 'Right'})
    body.append(para(run('Justified text, a little of it.'), jc='both'))
    exp.append({'kind': 0, 'align': 3, 'text': 'Justified text, a little of it.'})
    body.append(para(run('Second heading'), 'Heading2'))
    exp.append({'kind': 2, 'text': 'Second heading', 'runs': [['Second', 'b', 18]]})
    body.append(para(run('Third heading'), 'Heading3'))
    exp.append({'kind': 3, 'text': 'Third heading', 'runs': [['Third', 'b', 12]]})
    body.append(para(run('A bullet'), 'ListParagraph', extra=numpr(1)))
    exp.append({'kind': 5, 'text': 'A bullet'})
    body.append(para(run('Another bullet'), 'ListParagraph', extra=numpr(1)))
    exp.append({'kind': 5, 'text': 'Another bullet'})
    body.append(para(run('Number one'), 'ListParagraph', extra=numpr(2)))
    exp.append({'kind': 6, 'text': 'Number one'})
    body.append(para(run('A quotation.'), 'Quote'))
    exp.append({'kind': 4, 'align': 1, 'text': 'A quotation.', 'runs': [['quotation', 'i', 12]]})
    body.append(para('<w:r><w:t>Tab</w:t></w:r><w:r><w:tab/><w:t>after a tab</w:t></w:r><w:r><w:br/>'
                     '<w:t>after a line break</w:t></w:r>'))
    exp.append({'kind': 0, 'text': 'Tab\tafter a tab'})
    exp.append({'kind': 0, 'text': 'after a line break'})
    body.append(para(run('Before the break') + '<w:r><w:br w:type="page"/></w:r>' + run('after the break')))
    exp.append({'kind': 0, 'text': 'Before the break'})
    exp.append({'pb': 1})
    exp.append({'kind': 0, 'text': 'after the break'})
    body.append(para(run('A new page by its paragraph'), extra='<w:pageBreakBefore/>'))
    exp.append({'pb': 1})
    exp.append({'kind': 0, 'text': 'A new page by its paragraph'})
    body.append(para('<w:r><w:br w:type="page"/></w:r>'))
    exp.append({'pb': 1})
    body.append(para(run('Code in its style'), 'Code'))
    exp.append({'kind': 0, 'text': 'Code in its style', 'runs': [['Code', 'mono', 10]]})
    # a table, a picture, a text box, a footnote, a comment, tracked changes, a field, a formula
    rows = ''.join('<w:tr><w:trPr><w:trHeight w:val="300"/></w:trPr>%s</w:tr>' % ''.join(
        '<w:tc><w:tcPr><w:tcW w:w="3000" w:type="dxa"/></w:tcPr>%s</w:tc>' % para(run('r%dc%d' % (r, c)))
        for c in range(3)) for r in range(2))
    body.append('<w:tbl><w:tblPr><w:tblStyle w:val="TableGrid"/><w:tblW w:w="0" w:type="auto"/></w:tblPr>'
                '<w:tblGrid><w:gridCol w:w="3000"/></w:tblGrid>%s</w:tbl>' % rows)
    exp.append({'kind': 0, 'text': 'r0c0\tr0c1\tr0c2'})
    exp.append({'kind': 0, 'text': 'r1c0\tr1c1\tr1c2'})
    pic = ('<w:r><w:drawing><wp:inline distT="0"><wp:extent cx="100" cy="100"/><wp:docPr id="1" name="Picture 1"/>'
           '<a:graphic><a:graphicData uri="http://schemas.openxmlformats.org/drawingml/2006/picture">'
           '<pic:pic><pic:nvPicPr><pic:cNvPr id="1" name="p.png"/><pic:cNvPicPr/></pic:nvPicPr><pic:blipFill><a:blip r:embed="rId20"/></pic:blipFill>'
           '<pic:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="100" cy="100"/></a:xfrm><a:prstGeom prst="rect"/></pic:spPr></pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r>')
    box = ('<w:r><mc:AlternateContent><mc:Choice Requires="wps"><w:drawing><wp:anchor><wps:wsp><wps:txbx>'
           '<w:txbxContent>%s</w:txbxContent></wps:txbx></wps:wsp></wp:anchor></w:drawing></mc:Choice>'
           '<mc:Fallback><w:pict><v:shape><v:textbox><w:txbxContent>%s</w:txbxContent></v:textbox>'
           '</v:shape></w:pict></mc:Fallback></mc:AlternateContent></w:r>') % (para(run('in a box')), para(run('in a box')))
    body.append(para(run('Picture:') + pic + run(' box:') + box + run(' note') +
                     '<w:r><w:rPr><w:rStyle w:val="FootnoteReference"/></w:rPr><w:footnoteReference w:id="1"/></w:r>'
                     + '<w:commentRangeStart w:id="0"/>' + run(' comment') + '<w:commentRangeEnd w:id="0"/>'
                     '<w:r><w:commentReference w:id="0"/></w:r>'
                     + '<w:ins w:id="5" w:author="A" w:date="2026-11-20T10:00:00Z">' + run(' inserted') + '</w:ins>'
                     + '<w:del w:id="6" w:author="A"><w:r><w:delText> deleted</w:delText></w:r></w:del>'
                     + '<w:r><w:fldChar w:fldCharType="begin"/></w:r><w:r><w:instrText xml:space="preserve"> DATE \\@ "d MMMM yyyy" </w:instrText></w:r>'
                     '<w:r><w:fldChar w:fldCharType="separate"/></w:r>' + run(' 20 November 2026') +
                     '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
                     + '<m:oMath><m:r><m:t>x=1</m:t></m:r></m:oMath>' + run(' end → \U0001F600 é')))
    exp.append({'kind': 0, 'text': 'Picture: box: note comment inserted 20 November 2026 end ? ? é', 'lo': 0})  # (LibreOffice makes the date today's)
    body.append(para(run('Last paragraph.')))
    exp.append({'kind': 0, 'text': 'Last paragraph.'})
    sect = ('<w:sectPr w:rsidR="00A1B2C3"><w:headerReference w:type="default" r:id="rId8"/>'
            '<w:footerReference w:type="default" r:id="rId9"/><w:pgSz w:w="12240" w:h="15840"/>'
            '<w:pgMar w:top="1440" w:right="1800" w:bottom="1440" w:left="1800" w:header="708" w:footer="708" w:gutter="0"/>'
            '<w:cols w:space="708"/><w:titlePg/><w:docGrid w:linePitch="360"/></w:sectPr>')
    doc = XMLH + '<w:document %s mc:Ignorable="w14"><w:body>%s%s</w:body></w:document>' % (W, ''.join(body), sect)
    hdr = XMLH + '<w:hdr %s>%s</w:hdr>' % (W, para(run('Veranda') + '<w:r><w:tab/></w:r>' + run('Test') +
                                                   '<w:r><w:tab/></w:r>' + run(POL[:6]), 'Header'))
    ftr = XMLH + '<w:ftr %s>%s</w:ftr>' % (W, para(run('Page ') + '<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
                                                   '<w:r><w:instrText xml:space="preserve"> PAGE </w:instrText></w:r>'
                                                   '<w:r><w:fldChar w:fldCharType="separate"/></w:r>' + run('1') +
                                                   '<w:r><w:fldChar w:fldCharType="end"/></w:r>' + run(' of ') +
                                                   '<w:fldSimple w:instr=" NUMPAGES \\* MERGEFORMAT ">' + run('3') +
                                                   '</w:fldSimple>', jc='center'))
    rels = XMLH + ('<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
                   '<Relationship Id="rId1" Type="%s/styles" Target="styles.xml"/>'
                   '<Relationship Id="rId2" Type="%s/numbering" Target="numbering.xml"/>'
                   '<Relationship Id="rId8" Type="%s/header" Target="header1.xml"/>'
                   '<Relationship Id="rId9" Type="%s/footer" Target="footer1.xml"/>'
                   '<Relationship Id="rId10" Type="%s/footnotes" Target="footnotes.xml"/>'
                   '<Relationship Id="rId11" Type="%s/comments" Target="comments.xml"/>'
                   '</Relationships>') % ((RELNS,) * 6)
    notes = XMLH + ('<w:footnotes %s><w:footnote w:type="separator" w:id="-1"><w:p><w:r><w:separator/></w:r></w:p></w:footnote>'
                    '<w:footnote w:type="continuationSeparator" w:id="0"><w:p><w:r><w:continuationSeparator/></w:r></w:p></w:footnote>'
                    '<w:footnote w:id="1"><w:p><w:r><w:footnoteRef/></w:r><w:r><w:t>The note.</w:t></w:r></w:p></w:footnote></w:footnotes>') % W
    comments = XMLH + ('<w:comments %s><w:comment w:id="0" w:author="A" w:date="2026-11-20T10:00:00Z" w:initials="A">'
                       '<w:p><w:r><w:t>The comment.</w:t></w:r></w:p></w:comment></w:comments>') % W
    write_docx(path, doc, styles=DOCX_STYLES, num=DOCX_NUM, rels=rels, extra=[('word/header1.xml', hdr),
                                                                              ('word/footer1.xml', ftr),
                                                                              ('word/footnotes.xml', notes),
                                                                              ('word/comments.xml', comments)])
    return {'paras': exp, 'page': {'paper': 1, 'top': 1440, 'bottom': 1440, 'left': 1800, 'right': 1800,
                                   'head': 'Veranda|Test|Zażółć', 'foot': 'Page {page} of {pages}', 'flags': 1},
            'counts': {'tables': 1, 'pics': 2, 'boxes': 1, 'notes': 1, 'comments': 1, 'changes': 2, 'lost': 2,
                       'fields': 0}, 'pbreaks': 3}


def write_docx(path, doc, styles=None, num=None, rels=None, extra=()):
    ct = XMLH + ('<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
                 '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
                 '<Default Extension="xml" ContentType="application/xml"/>'
                 '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
                 + ''.join('<Override PartName="/word/%s.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.%s+xml"/>'
                           % (n, t) for n, t in (('styles', 'styles'), ('numbering', 'numbering'), ('header1', 'header'),
                                                 ('footer1', 'footer'), ('footnotes', 'footnotes'), ('comments', 'comments'))
                           if any(e[0] == 'word/%s.xml' % n for e in extra) or (n == 'styles' and styles) or (n == 'numbering' and num))
                 + '</Types>')
    top = XMLH + ('<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
                  '<Relationship Id="rId1" Type="%s/officeDocument" Target="word/document.xml"/></Relationships>' % RELNS)
    parts = [('[Content_Types].xml', ct), ('_rels/.rels', top), ('word/document.xml', doc)]
    if styles:
        parts.append(('word/styles.xml', styles))
    if num:
        parts.append(('word/numbering.xml', num))
    if rels:
        parts.append(('word/_rels/document.xml.rels', rels))
    parts += list(extra)
    zipit(path, parts)


def docx_big(path, n):
    """a document.xml of about n bytes: paragraphs of styled runs,
    headings, page breaks; the text it must give (its length)"""
    body, chars, k = [], 0, 0
    size = 0
    words = ('alpha', 'beta', 'gamma', 'delta', POL.split()[0], 'epsilon', 'zeta')
    while size < n:
        if k % 60 == 59:
            p = para(run('x') + '<w:r><w:br w:type="page"/></w:r>')
            chars += 3
        elif k % 25 == 0:
            p = para(run('Heading %d' % k), 'Nagwek1')
            chars += len('Heading %d' % k) + 1
        else:
            rs = ''
            for j in range(8):
                t = ' '.join(words[(k + j + q) % 7] for q in range(4)) + ' '
                rs += run(t, b=j % 3 == 0, i=j % 4 == 1)
                chars += len(t)
            p = para(rs, jc='both' if k % 3 == 0 else None)
            chars += 1
        body.append(p)
        size += len(p.encode('utf-8'))
        k += 1
    doc = XMLH + '<w:document %s><w:body>%s<w:sectPr><w:pgSz w:w="11906" w:h="16838"/><w:pgMar w:top="1417" w:right="1417" w:bottom="1417" w:left="1417"/></w:sectPr></w:body></w:document>' % (W, ''.join(body))
    write_docx(path, doc, styles=DOCX_STYLES)
    return {'chars': chars - 1, 'xml': len(doc.encode('utf-8')), 'paras_n': k}


# ---------------------------------------------------------------- .odt
ODT_NS = ('xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" '
          'xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" '
          'xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" '
          'xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0" '
          'xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" '
          'xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" '
          'xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:dc="http://purl.org/dc/elements/1.1/" '
          'xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0" '
          'xmlns:loext="urn:org:documentfoundation:names:experimental:office:xmlns:loext:1.0" office:version="1.3"')
FACES = ('<office:font-face-decls><style:font-face style:name="Liberation Serif" svg:font-family="&apos;Liberation Serif&apos;" style:font-family-generic="roman" style:font-pitch="variable"/>'
         '<style:font-face style:name="Liberation Mono" svg:font-family="&apos;Liberation Mono&apos;" style:font-family-generic="modern" style:font-pitch="fixed"/>'
         '<style:font-face style:name="DejaVu Sans" svg:font-family="&apos;DejaVu Sans&apos;" style:font-pitch="variable"/></office:font-face-decls>')
ODT_STYLES = XMLH.replace(' standalone="yes"', '') + ('<office:document-styles %s>%s<office:styles>'
    '<style:default-style style:family="paragraph"><style:paragraph-properties fo:hyphenation-ladder-count="no-limit" style:writing-mode="page"/><style:text-properties style:font-name="Liberation Serif" fo:font-size="12pt" fo:language="pl" fo:country="PL"/></style:default-style>'
    '<style:style style:name="Standard" style:family="paragraph" style:class="text"/>'
    '<style:style style:name="Heading" style:family="paragraph" style:parent-style-name="Standard" style:next-style-name="Text_20_body" style:class="text"><style:paragraph-properties fo:margin-top="0.423cm" fo:margin-bottom="0.212cm" loext:contextual-spacing="false" fo:keep-with-next="always"/><style:text-properties style:font-name="DejaVu Sans" fo:font-size="14pt"/></style:style>'
    '<style:style style:name="Text_20_body" style:display-name="Text body" style:family="paragraph" style:parent-style-name="Standard" style:class="text"><style:paragraph-properties fo:margin-top="0cm" fo:margin-bottom="0.247cm" style:contextual-spacing="false" fo:line-height="115%%"/></style:style>'
    '<style:style style:name="Heading_20_1" style:display-name="Heading 1" style:family="paragraph" style:parent-style-name="Heading" style:next-style-name="Text_20_body" style:default-outline-level="1" style:class="text"><style:text-properties fo:font-size="130%%" fo:font-weight="bold"/></style:style>'
    '<style:style style:name="Heading_20_2" style:display-name="Heading 2" style:family="paragraph" style:parent-style-name="Heading" style:default-outline-level="2" style:class="text"><style:text-properties fo:font-size="115%%" fo:font-weight="bold"/></style:style>'
    '<style:style style:name="Quotations" style:family="paragraph" style:parent-style-name="Standard" style:class="html"><style:paragraph-properties fo:margin-left="1cm" fo:margin-right="1cm" fo:margin-top="0cm" fo:margin-bottom="0.283cm"/></style:style>'
    '<style:style style:name="Preformatted_20_Text" style:display-name="Preformatted Text" style:family="paragraph" style:parent-style-name="Standard" style:class="html"><style:text-properties style:font-name="Liberation Mono" fo:font-size="10pt"/></style:style>'
    '<style:style style:name="Emphasis" style:family="text"><style:text-properties fo:font-style="italic"/></style:style>'
    '<text:list-style style:name="Numbering_20_123" style:display-name="Numbering 123"><text:list-level-style-number text:level="1" style:num-suffix="." style:num-format="1"><style:list-level-properties text:list-level-position-and-space-mode="label-alignment"/></text:list-level-style-number></text:list-style>'
    '</office:styles><office:automatic-styles>'
    '<style:style style:name="MP1" style:family="paragraph" style:parent-style-name="Header"><style:paragraph-properties fo:text-align="center"/></style:style>'
    '<style:page-layout style:name="pm1"><style:page-layout-properties fo:page-width="21.001cm" fo:page-height="29.7cm" style:num-format="1" style:print-orientation="portrait" fo:margin-top="2cm" fo:margin-bottom="1.5cm" fo:margin-left="2.5cm" fo:margin-right="2.5cm" style:writing-mode="lr-tb"/>'
    '<style:header-style><style:header-footer-properties fo:min-height="0cm" fo:margin-bottom="0.499cm"/></style:header-style><style:footer-style><style:header-footer-properties fo:min-height="0cm" fo:margin-top="0.499cm"/></style:footer-style></style:page-layout>'
    '<style:page-layout style:name="pm2"><style:page-layout-properties fo:page-width="21.001cm" fo:page-height="29.7cm" fo:margin-top="2cm" fo:margin-bottom="2cm" fo:margin-left="2cm" fo:margin-right="2cm"/><style:header-style/><style:footer-style/></style:page-layout>'
    '</office:automatic-styles><office:master-styles>'
    '<style:master-page style:name="Standard" style:page-layout-name="pm1"><style:header><text:p text:style-name="MP1">Header <text:span>of</text:span> ' + POL[:6] + '</text:p></style:header>'
    '<style:footer><text:p text:style-name="Footer">Page <text:page-number text:select-page="current">1</text:page-number>/<text:page-count>3</text:page-count><text:tab/>right</text:p></style:footer></style:master-page>'
    '<style:master-page style:name="First_20_Page" style:display-name="First Page" style:page-layout-name="pm2" style:next-style-name="Standard"/>'
    '</office:master-styles></office:document-styles>') % (ODT_NS, FACES)


def odt_auto():
    return ('<office:automatic-styles>'
            '<style:style style:name="P1" style:family="paragraph" style:parent-style-name="Heading_20_1" style:master-page-name="First_20_Page"><style:paragraph-properties style:page-number="auto"/></style:style>'
            '<style:style style:name="P2" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:text-align="center" style:justify-single-word="false"/></style:style>'
            '<style:style style:name="P3" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:text-align="end"/></style:style>'
            '<style:style style:name="P4" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:text-align="justify"/></style:style>'
            '<style:style style:name="P5" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:break-before="page"/></style:style>'
            '<style:style style:name="P6" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:margin-left="1.27cm" fo:text-indent="-0.635cm"/></style:style>'
            '<style:style style:name="T1" style:family="text"><style:text-properties fo:font-weight="bold" style:font-weight-asian="bold"/></style:style>'
            '<style:style style:name="T2" style:family="text"><style:text-properties fo:font-style="italic"/></style:style>'
            '<style:style style:name="T3" style:family="text"><style:text-properties style:text-underline-style="solid" style:text-underline-width="auto" style:text-underline-color="font-color"/></style:style>'
            '<style:style style:name="T4" style:family="text"><style:text-properties fo:font-size="20pt"/></style:style>'
            '<style:style style:name="T5" style:family="text"><style:text-properties style:font-name="Liberation Mono"/></style:style>'
            '<style:style style:name="T6" style:family="text"><style:text-properties fo:font-size="8pt"/></style:style>'
            '<style:style style:name="Table1" style:family="table"><style:table-properties style:width="17cm"/></style:style>'
            '<style:style style:name="fr1" style:family="graphic" style:parent-style-name="Graphics"/>'
            '<text:list-style style:name="L1"><text:list-level-style-bullet text:level="1" text:bullet-char="•"><style:list-level-properties/></text:list-level-style-bullet><text:list-level-style-number text:level="2" style:num-format="1"/></text:list-style>'
            '</office:automatic-styles>')


def tp(t, style='Text_20_body'):
    return '<text:p text:style-name="%s">%s</text:p>' % (style, t)


def odt_features(path):
    b, exp = [], []
    b.append('<text:h text:style-name="P1" text:outline-level="1">Title of the test</text:h>')
    exp.append({'kind': 1, 'text': 'Title of the test', 'runs': [['Title', 'b', 27]]})
    b.append(tp('Plain, <text:span text:style-name="T1">bold</text:span>, <text:span text:style-name="T2">italic</text:span>, '
                '<text:span text:style-name="T3">underlined</text:span> and <text:span text:style-name="T4">big</text:span> and '
                '<text:span text:style-name="T6">small</text:span> and <text:span text:style-name="T5">mono</text:span> '
                '<text:span text:style-name="Emphasis">emph<text:span text:style-name="T1">both</text:span></text:span>.'))
    exp.append({'kind': 0, 'after': 8, 'text': 'Plain, bold, italic, underlined and big and small and mono emphboth.',
                'runs': [['Plain', '', 12], ['bold', 'b', 12], ['italic', 'i', 12], ['underlined', 'u', 12],
                         ['big', '', 18], ['small', '', 9], ['mono', 'mono', 12], ['emph', 'i', 12], ['both', 'bi', 12]]})
    b.append(tp(esc(POL) + ' <text:span text:style-name="T1">' + POL2 + '</text:span>', 'P2'))
    exp.append({'kind': 0, 'align': 1, 'text': POL + ' ' + POL2})
    b.append(tp('Right', 'P3'))
    exp.append({'kind': 0, 'align': 2, 'text': 'Right'})
    b.append(tp('Justified   text,\n a <text:s text:c="2"/>little of it.', 'P4'))
    exp.append({'kind': 0, 'align': 3, 'text': 'Justified text, a   little of it.'})
    b.append('<text:h text:style-name="Heading_20_2" text:outline-level="2">Second heading</text:h>')
    exp.append({'kind': 2, 'text': 'Second heading', 'runs': [['Second', 'b', 18]]})
    b.append('<text:list xml:id="list1" text:style-name="L1"><text:list-item>%s</text:list-item><text:list-item>%s</text:list-item></text:list>'
             % (tp('A bullet', 'P6'), tp('Another bullet', 'P6')))
    exp.append({'kind': 5, 'text': 'A bullet'})
    exp.append({'kind': 5, 'text': 'Another bullet'})
    b.append('<text:list text:style-name="Numbering_20_123"><text:list-item>%s</text:list-item></text:list>' % tp('Number one'))
    exp.append({'kind': 6, 'text': 'Number one'})
    b.append(tp('A quotation.', 'Quotations'))
    exp.append({'kind': 4, 'text': 'A quotation.', 'left': 32})
    b.append(tp('Tab<text:tab/>after a tab<text:line-break/>after a line break'))
    exp.append({'kind': 0, 'text': 'Tab\tafter a tab'})
    exp.append({'kind': 0, 'text': 'after a line break'})
    b.append(tp('Before the break'))
    exp.append({'kind': 0, 'text': 'Before the break'})
    b.append(tp('A new page by its style', 'P5'))
    exp.append({'pb': 1})
    exp.append({'kind': 0, 'text': 'A new page by its style'})
    b.append(tp('Code in its style', 'Preformatted_20_Text'))
    exp.append({'kind': 0, 'text': 'Code in its style', 'runs': [['Code', 'mono', 10]]})
    rows = ''.join('<table:table-row>%s</table:table-row>' % ''.join(
        '<table:table-cell office:value-type="string">%s</table:table-cell>' % tp('r%dc%d' % (r, c), 'Table_20_Contents')
        for c in range(3)) for r in range(2))
    b.append('<table:table table:name="Table1" table:style-name="Table1"><table:table-column table:number-columns-repeated="3"/>%s</table:table>' % rows)
    exp.append({'kind': 0, 'text': 'r0c0\tr0c1\tr0c2'})
    exp.append({'kind': 0, 'text': 'r1c0\tr1c1\tr1c2'})
    b.append(tp('Picture:<draw:frame draw:name="Image1" text:anchor-type="as-char" svg:width="1cm" svg:height="1cm"><draw:image xlink:href="Pictures/1.png"/></draw:frame>'
                ' box:<draw:frame draw:name="Frame1"><draw:text-box>%s</draw:text-box></draw:frame>'
                ' note<text:note text:id="ftn1" text:note-class="footnote"><text:note-citation>1</text:note-citation><text:note-body>%s</text:note-body></text:note>'
                ' comment<office:annotation><dc:creator>A</dc:creator>%s</office:annotation>'
                ' inserted<text:change text:change-id="ct2"/> end → \U0001F600 é' % (tp('in a box'), tp('the note'), tp('the comment'))))
    exp.append({'kind': 0, 'text': 'Picture: box: note comment inserted end ? ? é'})
    b.append(tp('Last paragraph.'))
    exp.append({'kind': 0, 'text': 'Last paragraph.'})
    tracked = ('<text:tracked-changes><text:changed-region xml:id="ct1" text:id="ct1"><text:insertion><office:change-info><dc:creator>A</dc:creator></office:change-info></text:insertion></text:changed-region>'
               '<text:changed-region text:id="ct2"><text:deletion><office:change-info/>%s</text:deletion></text:changed-region></text:tracked-changes>' % tp('deleted'))
    content = XMLH.replace(' standalone="yes"', '') + ('<office:document-content %s><office:scripts/>%s%s<office:body><office:text>'
               '%s<text:sequence-decls><text:sequence-decl text:display-outline-level="0" text:name="Table"/></text:sequence-decls>%s'
               '</office:text></office:body></office:document-content>') % (ODT_NS, FACES, odt_auto(), tracked, ''.join(b))
    write_odt(path, content, ODT_STYLES)
    return {'paras': exp, 'page': {'paper': 0, 'top': 1417, 'bottom': 850 + 283, 'left': 1417, 'right': 1417,
                                   'head': 'Header of Zażółć', 'foot': 'Page {page}/{pages}|right', 'flags': 1},
            'counts': {'tables': 1, 'pics': 1, 'boxes': 1, 'notes': 1, 'comments': 1, 'changes': 2, 'lost': 2},
            'pbreaks': 1}


def write_odt(path, content, styles):
    man = XMLH.replace(' standalone="yes"', '') + (
        '<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0" manifest:version="1.3">'
        '<manifest:file-entry manifest:full-path="/" manifest:version="1.3" manifest:media-type="application/vnd.oasis.opendocument.text"/>'
        '<manifest:file-entry manifest:full-path="content.xml" manifest:media-type="text/xml"/>'
        '<manifest:file-entry manifest:full-path="styles.xml" manifest:media-type="text/xml"/>'
        '</manifest:manifest>')
    zipit(path, [('META-INF/manifest.xml', man), ('styles.xml', styles), ('content.xml', content)],
          stored_first=('mimetype', 'application/vnd.oasis.opendocument.text'))


def odt_big(path, n):
    body, chars, k, size = [], 0, 0, 0
    words = ('alpha', 'beta', 'gamma', 'delta', POL.split()[0], 'epsilon', 'zeta')
    while size < n:
        if k % 25 == 0:
            p = '<text:h text:style-name="Heading_20_1" text:outline-level="1">Heading %d</text:h>' % k
            chars += len('Heading %d' % k) + 1
        else:
            t = ''
            for j in range(8):
                w = ' '.join(words[(k + j + q) % 7] for q in range(4)) + ' '
                t += '<text:span text:style-name="T%d">%s</text:span>' % (1 + j % 3, w) if j % 2 else w
                chars += len(w)
            p = tp(t[:-1] if t.endswith(' ') else t, 'P4' if k % 3 == 0 else 'Text_20_body')
            chars += 1 - (1 if t.endswith(' ') else 0)
        body.append(p)
        size += len(p.encode('utf-8'))
        k += 1
    content = XMLH.replace(' standalone="yes"', '') + ('<office:document-content %s>%s%s<office:body><office:text>%s'
                                                       '</office:text></office:body></office:document-content>') % (
        ODT_NS, FACES, odt_auto(), ''.join(body))
    write_odt(path, content, ODT_STYLES)
    return {'chars': chars - 1, 'xml': len(content.encode('utf-8')), 'paras_n': k}


def polish_docx(path):
    doc = XMLH + '<w:document %s><w:body>%s%s</w:body></w:document>' % (
        W, para(run(POL)), para(run(POL2, b=True)))
    write_docx(path, doc)
    return {'paras': [{'kind': 0, 'text': POL}, {'kind': 0, 'text': POL2, 'runs': [['ZA', 'b', 12]]}],
            'counts': {'lost': 0}}


def plain_docx(path):
    """the least a .docx may be: no styles, no relationships of its own"""
    doc = XMLH + '<w:document %s><w:body><w:p><w:r><w:t>Just text.</w:t></w:r></w:p></w:body></w:document>' % W
    write_docx(path, doc)
    return {'paras': [{'kind': 0, 'text': 'Just text.'}]}


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    exp = {
        'features.docx': docx_features(os.path.join(out, 'features.docx')),
        'features.odt': odt_features(os.path.join(out, 'features.odt')),
        'polish.docx': polish_docx(os.path.join(out, 'polish.docx')),
        'plain.docx': plain_docx(os.path.join(out, 'plain.docx')),
        'big.docx': docx_big(os.path.join(out, 'big.docx'), int(os.environ.get('OX_BIG', '300000'))),
        'big.odt': odt_big(os.path.join(out, 'big.odt'), int(os.environ.get('OX_BIG', '300000'))),
    }
    json.dump(exp, open(os.path.join(out, 'expect.json'), 'w'), indent=1, ensure_ascii=False)


if __name__ == '__main__':
    main()
