#!/usr/bin/env python3
"""Veranda Write: the element and attribute names the .docx and .odt
readers know (src/odocx.c, src/oodt.c), as a header of hashes:
src/oxnames.h (the Makefile makes it).

usage: oxnames.py <out.h>

(#define OXN_DOCX or OXN_ODT first: a reader's tables only.)
A name is looked up by its hash (src/ohelp.c's hash: h = 5381, then for
each byte h = (h * 33) ^ c, 16 bits) and its length in an open-addressed
table (its slot the hash's low bits, then the next ones), not by
comparing strings; this script refuses a
collision among the known names, and with the other names the two
formats use that the readers pass over (a list below: an unknown name
with a known one's hash and length would be taken for it).

Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE)."""
import sys

DOCX_E = '''w:p w:r w:t w:pPr w:rPr w:pStyle w:jc w:ind w:spacing w:numPr w:numId w:pageBreakBefore
w:outlineLvl w:sectPr w:type w:pgSz w:pgMar w:headerReference w:footerReference w:titlePg w:rStyle
w:b w:i w:u w:sz w:rFonts w:tab w:tabs w:br w:cr w:noBreakHyphen w:sym w:drawing w:pict w:object
w:txbxContent mc:Fallback m:oMath m:oMathPara w:footnoteReference w:endnoteReference
w:commentReference w:del w:moveFrom w:ins w:moveTo w:pPrChange w:rPrChange w:sectPrChange
w:tblPrChange w:fldChar w:instrText w:fldSimple w:tbl w:tr w:tc w:tblPr w:tblGrid w:trPr w:tcPr
w:sdtPr w:sdtEndPr w:docDefaults w:latentStyles w:style w:name w:basedOn w:abstractNum w:lvl
w:numFmt w:num w:abstractNumId w:lvlOverride Relationship w:hdr w:ftr w:vanish w:delInstrText
w:rPrDefault w:pPrDefault w:background w:fldData w:delText'''.split()
DOCX_A = '''w:val w:type w:left w:start w:right w:end w:firstLine w:hanging w:before w:after w:line
w:lineRule w:w w:h w:top w:bottom r:id w:styleId w:default w:ascii w:hAnsi w:fldCharType w:instr
w:abstractNumId w:numId w:ilvl Id Type Target'''.split()
ODT_E = '''office:automatic-styles office:master-styles office:styles office:body office:text
style:style style:default-style style:paragraph-properties style:text-properties style:page-layout
style:page-layout-properties style:master-page style:header style:footer style:header-first
style:footer-first style:header-left style:footer-left style:font-face text:list-style
text:list-level-style-bullet text:list-level-style-number text:list-level-style-image text:p text:h
text:span text:list text:list-item text:list-header text:tab text:s text:line-break
text:soft-page-break text:note text:page-number text:page-count text:date text:file-name text:tracked-changes table:table
table:table-row table:table-cell table:covered-table-cell draw:frame draw:custom-shape draw:g
draw:rect draw:ellipse draw:line draw:polygon draw:path draw:control office:annotation
text:table-of-content-source text:alphabetical-index-source text:illustration-index-source
text:index-title-template office:forms text:sequence-decls text:variable-decls
text:user-field-decls style:header-footer-properties text:changed-region office:font-face-decls
style:header-style style:footer-style text:bibliography-source text:object-index-source
text:user-index-source text:table-index-source'''.split()
ODT_A = '''style:name style:family style:parent-style-name style:display-name
style:default-outline-level style:list-style-name style:master-page-name style:page-layout-name
fo:text-align fo:margin-left fo:margin-right fo:margin-top fo:margin-bottom fo:text-indent
fo:line-height fo:break-before fo:break-after fo:font-weight fo:font-style fo:font-size
style:font-name fo:font-family style:text-underline-style fo:page-width fo:page-height
text:style-name text:outline-level text:c fo:min-height svg:font-family style:font-pitch
style:font-family-generic fo:margin style:text-underline-type style:font-size-asian
style:font-name-complex'''.split()
# the elements whose attributes a reader reads (the others' are passed
# over without a look: a bitmap by the element's number)
DOCX_AW = '''w:pStyle w:jc w:ind w:spacing w:numId w:pageBreakBefore w:outlineLvl w:type w:pgSz w:pgMar
w:headerReference w:footerReference w:titlePg w:rStyle w:b w:i w:u w:sz w:rFonts w:br w:fldChar w:fldSimple
w:style w:name w:basedOn w:abstractNum w:lvl w:numFmt w:num w:abstractNumId Relationship w:vanish'''.split()
ODT_AW = '''style:style style:default-style style:paragraph-properties style:text-properties style:page-layout
style:page-layout-properties style:master-page style:header-footer-properties style:font-face text:list-style
text:list text:p text:h text:span text:s'''.split()
# names the readers do not know, which must not be taken for one
OTHER = '''w:document w:body w:lang w:szCs w:bCs w:iCs w:color w:kern w:noProof w:proofErr w:bookmarkStart
w:bookmarkEnd w:lastRenderedPageBreak w:rtl w:keepNext w:keepLines w:widowControl w:contextualSpacing
w:highlight w:shd w:vertAlign w:strike w:dstrike w:caps w:smallCaps w:spacingCs w:w w:position
w:eastAsianLayout w:specVanish w:oMath w:snapToGrid w:adjustRightInd w:autoSpaceDE w:autoSpaceDN
w:textAlignment w:suppressAutoHyphens w:hyperlink w:smartTag w:customXml w:sdt w:sdtContent
w:tblW w:tblInd w:tblBorders w:tblLayout w:tblCellMar w:tblLook w:gridCol w:tcW w:vAlign w:gridSpan
w:vMerge w:tblHeader w:trHeight w:cols w:docGrid w:pgNumType w:formProt w:textDirection w:bidi
w:rsid w:rsidR w:rsidRPr w:rsidRDefault w:rsidP w:paraId w:textId w14:paraId w14:textId
w:uiPriority w:qFormat w:semiHidden w:unhideWhenUsed w:link w:next w:autoRedefine w:locked
w:lsdException w:multiLevelType w:nsid w:tmpl w:lvlText w:lvlJc w:start w:suff w:lvlRestart
w:isLgl w:legacy w:pStyle w:lvlPicBulletId w:numIdMacAtCleanup w:abstractNum w:startOverride
w:framePr w:pBdr w:top w:left w:bottom w:right w:between w:bar w:outline w:shadow w:emboss
w:imprint w:webHidden w:fitText w:em w:effect w:specVanish w:fonts w:font w:panose1 w:charset
w:family w:pitch w:sig w:altName w:settings w:zoom w:proofState w:defaultTabStop w:characterSpacingControl
w:compat w:compatSetting w:rsids w:themeFontLang w:clrSchemeMapping w:decimalSymbol w:listSeparator
w:footnotePr w:endnotePr w:footnote w:endnote w:separator w:continuationSeparator w:footnoteRef
w:endnoteRef w:annotationRef w:comments w:comment w:headerReference w:permStart w:permEnd
w:moveFromRangeStart w:moveFromRangeEnd w:moveToRangeStart w:moveToRangeEnd w:commentRangeStart
w:commentRangeEnd w:ruby w:rubyPr w:rt w:rubyBase w:dir w:bdo w:subDoc w:altChunk w:fldSimple
w:ptab w:dayShort w:monthShort w:yearShort w:dayLong w:monthLong w:yearLong w:pgNum w:annotationRef
w:contentPart w:docPartObj w:docPartGallery w:docPartUnique w:alias w:tag w:id w:showingPlcHdr
w:placeholder w:docPart w:text w:date w:dateFormat w:lid w:storeMappedDataAs w:calendar w:dropDownList
w:listItem w:comboBox w:richText w:picture w:citation w:group w:bibliography w:equation
mc:AlternateContent mc:Choice wp:inline wp:anchor wp:extent wp:effectExtent wp:docPr wp:cNvGraphicFramePr
a:graphic a:graphicData pic:pic pic:nvPicPr pic:blipFill pic:spPr a:blip a:stretch a:fillRect a:xfrm
a:off a:ext a:prstGeom a:avLst wps:wsp wps:txbx wps:bodyPr v:shape v:textbox v:imagedata v:rect
o:lock v:shapetype v:stroke v:formulas v:f v:path w10:wrap wp:wrapSquare wp:positionH wp:positionV
wp:posOffset wp:simplePos wp14:sizeRelH wp14:sizeRelV wp14:pctWidth wp14:pctHeight
office:document-content office:document-styles office:scripts office:settings office:meta office:version
office:value-type office:value text:a text:bookmark text:bookmark-start text:bookmark-end
text:reference-mark text:reference-mark-start text:reference-mark-end text:sequence text:sequence-decl
text:date text:time text:author-name text:title text:subject text:file-name text:chapter text:sheet-name
text:note-citation text:note-body text:note-ref text:bookmark-ref text:reference-ref text:sequence-ref
text:change text:change-start text:change-end text:insertion text:deletion text:format-change
text:section text:section-source text:table-of-content text:index-body text:index-title
text:alphabetical-index text:illustration-index text:user-defined text:placeholder text:drop-down
text:hidden-text text:hidden-paragraph text:conditional-text text:ruby text:ruby-base text:ruby-text
text:meta text:meta-field text:linenumbering-configuration text:notes-configuration
text:outline-style text:outline-level-style text:list-level-properties text:list-level-label-alignment
text:numbered-paragraph text:soft-page-break text:toc-mark text:toc-mark-start text:toc-mark-end
text:alphabetical-index-mark text:variable-set text:variable-get text:user-field-get text:page-variable-get
text:style-name text:cond-style-name text:class-names text:id text:list-id text:continue-numbering
text:continue-list text:start-value text:restart-numbering text:display-levels text:level
text:bullet-char text:num-format text:num-suffix text:num-prefix text:label-followed-by
text:list-tab-stop-position text:min-label-width text:space-before text:min-label-distance
text:is-list-header text:note-class text:name text:formula text:display text:protected
table:table-column table:table-columns table:table-header-rows table:table-rows table:name
table:style-name table:number-columns-repeated table:number-columns-spanned table:number-rows-spanned
table:table-header-columns table:default-cell-style-name table:align table:border-model
draw:image draw:text-box draw:name draw:z-index draw:style-name svg:width svg:height svg:x svg:y
text:anchor-type text:anchor-page-number draw:transform xlink:href xlink:type xlink:show xlink:actuate
office:binary-data style:map style:tab-stops style:tab-stop style:drop-cap style:background-image
style:columns style:column style:column-sep style:footnote-sep style:graphic-properties
style:table-properties style:table-column-properties style:table-row-properties
style:table-cell-properties style:section-properties style:ruby-properties style:list-level-properties
style:position style:type style:char style:leader-style style:leader-text style:num-format
style:print-orientation style:writing-mode style:footnote-max-height style:font-name-asian
style:font-weight-asian style:font-style-asian style:font-weight-complex style:font-style-complex
style:font-size-complex style:language-asian style:country-asian style:letter-kerning
style:text-underline-width style:text-underline-color style:text-line-through-style
style:text-position style:text-outline style:text-shadow style:use-window-font-color
style:font-relief style:text-emphasize style:text-scale style:text-rotation-angle
style:auto-text-indent style:tab-stop-distance style:writing-mode-automatic style:justify-single-word
style:punctuation-wrap style:line-break style:snap-to-layout-grid style:vertical-align
style:page-number style:shadow style:register-true style:class style:auto-update style:contextual-spacing
style:line-height-at-least style:line-spacing style:font-independent-line-spacing style:text-autospace
style:font-family-asian style:font-family-complex style:font-pitch-asian style:font-charset
style:font-adornments style:num-letter-sync style:volatile style:data-style-name style:percentage-data-style-name
style:list-level-position-and-space-mode style:dynamic-spacing style:layout-grid-mode
fo:color fo:background-color fo:font-variant fo:text-transform fo:letter-spacing fo:language fo:country
fo:hyphenate fo:hyphenation-remain-char-count fo:hyphenation-push-char-count fo:keep-with-next
fo:keep-together fo:widows fo:orphans fo:padding fo:border fo:border-top fo:border-bottom fo:border-left
fo:border-right fo:text-align-last fo:text-shadow fo:font-name fo:min-width fo:max-height fo:max-width
fo:wrap-option fo:clip fo:padding-top fo:padding-bottom fo:padding-left fo:padding-right
fo:column-count fo:column-gap fo:start-indent fo:end-indent fo:space-before fo:space-after
style:font-weight-complex fo:hyphenation-ladder-count fo:line-height-at-least
loext:graphic-properties loext:text-properties loext:paragraph-properties loext:contextual-spacing
loext:marker-style-name loext:tab-stop-distance loext:opacity loext:hyphenation-no-caps
officeooo:rsid officeooo:paragraph-rsid'''.split()


def h16(s):
    h = 5381
    for c in s.encode('latin-1'):
        h = ((h * 33) ^ c) & 0xFFFF
    return h


def table(name, names, out, other):
    seen = {}
    for i, n in enumerate(names):
        k = (h16(n), len(n))
        if k in seen and seen[k] != n:
            sys.exit('oxnames: %s and %s: one hash' % (seen[k], n))
        seen[k] = n
    for n in other:
        k = (h16(n), len(n))
        if k in seen and seen[k] != n:
            sys.exit('oxnames: %s (unknown) has %s\'s hash' % (n, seen[k]))
    ident = lambda n: n.replace(':', '_').replace('-', '_')
    for i, n in enumerate(names):
        out.append('#define %s_%s %d' % (name.upper(), ident(n), i + 1))
    out.append('#define %s_N %d' % (name.upper(), len(names)))
    # an open-addressed table: slot hash & m, then the next ones
    size = 16
    while size < len(names) * 1.34:
        size *= 2
    th, tl, ti = [0] * size, [0] * size, [0] * size
    for i, n in enumerate(names):
        k = h16(n)
        j = k & (size - 1)
        while ti[j]:
            j = (j + 1) & (size - 1)
        th[j], tl[j], ti[j] = k, len(n), i + 1
    out.append('#define %s_m %d' % (name, size - 1))
    out.append('static const unsigned int %s_h[%d] = {' % (name, size))
    out.append('    ' + ', '.join('0x%04X' % v for v in th) + ' };')
    out.append('static const unsigned char %s_l[%d] = {' % (name, size))
    out.append('    ' + ', '.join('%d' % v for v in tl) + ' };')
    out.append('static const unsigned char %s_i[%d] = {' % (name, size))
    out.append('    ' + ', '.join('%d' % v for v in ti) + ' };')


def bitmap(name, names, want, out):
    bits = [0] * ((len(names) + 1 + 7) // 8)
    for w in want:
        i = names.index(w) + 1
        bits[i >> 3] |= 1 << (i & 7)
    out.append('static const unsigned char %s_aw[%d] = { %s };' % (name, len(bits), ', '.join('%d' % b for b in bits)))


def main():
    out = ['/* made by tools/oxnames.py: do not edit */']
    other = set(OTHER) | set(DOCX_E) | set(DOCX_A) | set(ODT_E) | set(ODT_A)
    for name, names in (('dxe', DOCX_E), ('dxa', DOCX_A), ('ode', ODT_E), ('oda', ODT_A)):
        if name in ('dxe', 'ode'):
            out.append('#ifdef OXN_%s' % ('DOCX' if name == 'dxe' else 'ODT'))
        table(name, names, out, other)
        if name == 'dxe':
            bitmap('dxe', names, DOCX_AW, out)
        if name == 'ode':
            bitmap('ode', names, ODT_AW, out)
        if name in ('dxa', 'oda'):
            out.append('#endif')
    open(sys.argv[1], 'w').write('\n'.join(out) + '\n')


main()
