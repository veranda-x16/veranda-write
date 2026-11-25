/* Veranda Write: a Word document (.docx, Office Open XML) converted
   (DECISIONS W50, W51; office.h).

   The parts read, in this order, each once from its start (ozip.c's
   oz_part, oxml.c's tokens): word/_rels/document.xml.rels (which parts
   are the header and the footer), word/numbering.xml (which lists are
   numbered), word/styles.xml (the defaults, the paragraph and character
   styles: headings by their names or outline levels, quotations, lists,
   alignment, indents, spacing, bold, italic, underline, size, a
   monospaced font), word/document.xml (the body: paragraphs, runs,
   tabs, line and page breaks, tables as text, the page's size and
   margins), then the default header and footer (one line each, the PAGE
   and NUMPAGES fields as {page} and {pages}). Pictures, drawings, text
   boxes, formulas, footnotes and endnotes (their marks), comments and
   deleted text (tracked changes: the insertions kept) are left out and
   counted; the instructions of fields are left out, their results kept.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "office.h"
#include "oimp.h"
#ifndef CW_HOST
#pragma code-name ("OVERLAY19")
#pragma rodata-name ("OVL19RO")
#pragma bss-name ("OVL19BSS")
#define dx_load e_dx_load
#endif
#define OXN_DOCX
#include "oxnames.h"
#include "ohelp.c"

static const struct ntab te = NTAB(dxe), ta = NTAB(dxa);

#define DP_RELS 1
#define DP_NUM  2
#define DP_STY  3
#define DP_DOC  4
#define DP_HF   5
static unsigned char part, skipd, skipk, boxseen, e, ctx, indef, inpar, intext, ininstr, insect, cst;
static unsigned char tdepth, cellc, cellp, fld, fk, fsup, sect_br, a_on, a_t, a_id, nofirst;
#define CX_P 1                          /* properties: a paragraph's (pPr) */
#define CX_R 2                          /*   a run's (rPr) */
#define SK_PIC 1
static unsigned char tp;                /* (pr_t's while properties are read) */
static unsigned char st_type;
static int lval[4];                     /* an element's numbers (w:ind, w:spacing, w:pgMar ...) */
static unsigned char lset;
/* numbering.xml: the lists */
#define NABS 16
#define NNUM 48
static unsigned char absn, numn, cur_abs, cur_lvl;
static unsigned char abs_id[NABS], abs_num[NABS];
static unsigned int num_id[NNUM];
static unsigned char num_kind[NNUM];
/* the relationships: the headers' and footers' parts */
#define NREL 10
#define RELN 20
static unsigned char reln;
static unsigned int rel_h[NREL];
static char rel_t[NREL][RELN];
static unsigned int a_h;
static char a_tgt[RELN];
static char hdr_t[RELN], ftr_t[RELN];

/* the elements open: their numbers (an END needs no look-up) */
#define NEST 48
static unsigned char est[NEST], ed;

static void FASTCALL skip(unsigned char k)
{
    skipd = 1;
    skipk = k;
    boxseen = 0;
}

/* a field's kind: 1 PAGE, 2 NUMPAGES, 3 DATE, 4 FILENAME ({page}, {pages}, {date}, {name}) */
static unsigned char FASTCALL fkind(const char *s)
{
    if (strstr(s, "NUMPAGES") || strstr(s, "SECTIONPAGES"))
        return 2;
    if (strstr(s, "FILENAME"))
        return 4;
    if (strstr(s, "DATE"))
        return 3;
    return strstr(s, "PAGE") ? 1 : 0;
}

static unsigned char FASTCALL numkind(unsigned int id)
{
    unsigned char i;
    if (!id)
        return 0;
    for (i = 0; i < numn; ++i)
        if (num_id[i] == id)
            return num_kind[i];
    return PK_BULLET;
}

/* a style's name: a heading, a quotation, a list */
static void FASTCALL st_name(const char *s)
{
    if (starts(s, "heading ") && s[8] >= '1' && s[8] <= '9')
        SET(F_KIND, s[8] >= '3' ? PK_H3 : PK_H1 + (s[8] - '1'));
    else if (starts(s, "title"))
        SET(F_KIND, PK_H1);
    else if (starts(s, "subtitle"))
        SET(F_KIND, PK_H2);
    else if (starts(s, "quote") || starts(s, "intense quote"))
        SET(F_KIND, PK_QUOTE);
    else if (starts(s, "list bullet"))
        SET(F_LIST, PK_BULLET);
    else if (starts(s, "list number"))
        SET(F_LIST, PK_NUMBER);
}

static void FASTCALL tgt(unsigned char t)
{
    tp = pr_t = t;
}

/*---------------------------------------------------------------
  the tokens
 ---------------------------------------------------------------*/
static void start(void)
{
    static unsigned char doc;
    a_on = 1;
    a_t = 0;
    lset = 0;
    doc = part >= DP_DOC;
    switch (e) {
    case DXE_w_p:
        if (!doc)
            break;
        inpar = 1;
        if (tdepth) {
            if (cellp++)
                em_char(' ');
            tgt(PT_NONE);
        } else {
            em_par();
            tgt(PT_PAR);
        }
        break;
    case DXE_w_pPr:
        ctx = CX_P;
        tgt(cst ? PT_STY : indef ? PT_DEF : inpar ? tp : PT_NONE);
        break;
    case DXE_w_rPr:
        if (doc && inpar && ctx == CX_P) {
            skip(0);                    /* (the paragraph mark's) */
            break;
        }
        ctx = CX_R;
        tgt(cst ? PT_STY : indef ? PT_DEF : doc && !tdepth ? PT_RUN : doc ? PT_RUN : PT_NONE);
        break;
    case DXE_w_r:
        pr_clr(PT_RUN);
        em_rs = 0xFF;
        em_run();
        break;
    case DXE_w_t:
        intext = doc;
        break;
    case DXE_w_instrText:
        ininstr = 1;
        break;
    case DXE_w_tab:
        if (doc && ctx != CX_P)
            em_char(CH_TAB);
        break;
    case DXE_w_tabs: case DXE_mc_Fallback: case DXE_w_delInstrText: case DXE_w_delText:
    case DXE_w_tblPr: case DXE_w_tblGrid: case DXE_w_trPr: case DXE_w_tcPr: case DXE_w_sdtPr:
    case DXE_w_sdtEndPr: case DXE_w_latentStyles: case DXE_w_lvlOverride: case DXE_w_background:
    case DXE_w_fldData:
        skip(0);
        break;
    case DXE_w_cr:
        if (doc) {
            if (tdepth)
                em_char(' ');
            else
                em_split();
        }
        break;
    case DXE_w_noBreakHyphen:
        if (doc)
            em_char('-');
        break;
    case DXE_w_sym:
        if (doc) {
            em_char('?');
            ++oxc.lost;
        }
        break;
    case DXE_w_drawing: case DXE_w_pict: case DXE_w_object: case DXE_m_oMath: case DXE_m_oMathPara:
        skip(SK_PIC);
        break;
    case DXE_w_footnoteReference: case DXE_w_endnoteReference:
        ++oxc.notes;
        break;
    case DXE_w_commentReference:
        ++oxc.comments;
        break;
    case DXE_w_del: case DXE_w_moveFrom: case DXE_w_pPrChange: case DXE_w_rPrChange:
    case DXE_w_sectPrChange: case DXE_w_tblPrChange:
        ++oxc.changes;
        skip(0);
        break;
    case DXE_w_ins: case DXE_w_moveTo:
        ++oxc.changes;
        break;
    case DXE_w_tbl:
        if (!tdepth++ && doc && part == DP_DOC)
            ++oxc.tables;
        break;
    case DXE_w_tr:
        if (tdepth == 1) {
            em_par();
            cellc = 0;
        } else if (tdepth)
            em_char(' ');
        break;
    case DXE_w_tc:
        if (tdepth == 1 && cellc++)
            em_char(CH_TAB);
        else if (tdepth > 1)
            em_char(' ');
        cellp = 0;
        break;
    case DXE_w_sectPr:
        insect = 1;
        a_t = 0;
        tgt(PT_NONE);
        break;
    case DXE_w_docDefaults:
        indef = 1;
        break;
    case DXE_w_style:
        st_type = 0;
        a_h = 0;
        a_id = 0;
        break;
    }
}

static void attr(void)
{
    static unsigned char a;
    static int v;
    if (!(dxe_aw[e >> 3] & (1 << (e & 7))))
        return;                         /* (its attributes not read) */
    a = nlook(&ta);
    if (!a)
        return;
    if (a == DXA_w_val) {
        switch (e) {
        case DXE_w_b: case DXE_w_i: case DXE_w_pageBreakBefore: case DXE_w_titlePg: case DXE_w_vanish:
            a_on = oon(XVAL);
            return;
        case DXE_w_u:
            a_on = strcmp(XVAL, "none") != 0;
            return;
        case DXE_w_pStyle:
            if (tp == PT_PAR) {
                a = st_find(hash(XVAL));
                if (a != 0xFF)
                    em_ps = a;
            }
            return;
        case DXE_w_rStyle:
            if (tp == PT_RUN)
                em_rs = st_find(hash(XVAL));
            return;
        case DXE_w_jc:
            a = XVAL[0];
            SET(F_ALIGN, a == 'c' ? PA_CENTRE : a == 'r' || (a == 'e' && XVAL[1] == 'n') ? PA_RIGHT
                         : a == 'b' || a == 'd' ? PA_JUSTIFY : PA_LEFT);
            return;
        case DXE_w_numId:
            if (ctx == CX_P)
                SET(F_LIST, numkind((unsigned int)onum(XVAL)));
            return;
        case DXE_w_outlineLvl:
            v = onum(XVAL);
            if (v >= 0 && v < 9)
                SET(F_KIND1, v >= 2 ? PK_H3 : PK_H1 + v);
            return;
        case DXE_w_sz:
            if (ctx == CX_R) {
                v = (onum(XVAL) + 1) / 2;
                SET(F_SIZE, v < 1 ? 1 : v > 127 ? 127 : v);
            }
            return;
        case DXE_w_type:
            if (insect && XVAL[0] == 'c')
                a_t = 1;                /* (continuous: no page break) */
            return;
        case DXE_w_name:
            if (cst)
                st_name(XVAL);
            return;
        case DXE_w_basedOn:
            if (cst)
                st_base(st_find(hash(XVAL)));
            return;
        case DXE_w_numFmt:
            if (part == DP_NUM && !cur_lvl && absn && abs_id[absn - 1] == cur_abs)
                abs_num[absn - 1] = !strcmp(XVAL, "none") ? 2 : strcmp(XVAL, "bullet") != 0;
            return;
        case DXE_w_abstractNumId:
            if (part == DP_NUM && numn) {
                v = onum(XVAL);
                for (a = 0; a < absn; ++a)
                    if (abs_id[a] == v)
                        num_kind[numn - 1] = abs_num[a] == 2 ? 0 : abs_num[a] ? PK_NUMBER : PK_BULLET;
            }
            return;
        }
        return;
    }
    switch (e) {
    case DXE_w_ind:
        v = onum(XVAL);
        if (a == DXA_w_left || a == DXA_w_start)
            SET(F_LEFT, px(v, 240));
        else if (a == DXA_w_right || a == DXA_w_end)
            SET(F_RIGHT, px(v, 240));
        else if (a == DXA_w_firstLine)
            SET(F_FIRST, px(v, 120));
        else if (a == DXA_w_hanging)
            SET(F_FIRST, -(signed char)px(v, 120));
        return;
    case DXE_w_spacing:
        if (ctx != CX_P)
            return;
        v = onum(XVAL);
        if (a == DXA_w_before)
            SET(F_BEFORE, px(v, 254));
        else if (a == DXA_w_after)
            SET(F_AFTER, px(v, 254));
        else if (a == DXA_w_line)
            lval[0] = v;
        else if (a == DXA_w_lineRule && XVAL[0] != 'a')
            lset = 1;                   /* (exact, at least: twips, not lines) */
        return;
    case DXE_w_rFonts:
        if (ctx == CX_R && (a == DXA_w_ascii || a == DXA_w_hAnsi))
            SET(F_MONO, mono(XVAL));
        return;
    case DXE_w_pgSz:
        v = onum(XVAL);
        if (a == DXA_w_w)
            pgs.w = v;
        else if (a == DXA_w_h)
            pgs.h = v;
        return;
    case DXE_w_pgMar:
        v = onum(XVAL);
        if (v < 0)
            v = -v;
        if (a == DXA_w_top)
            pgs.top = v;
        else if (a == DXA_w_bottom)
            pgs.bottom = v;
        else if (a == DXA_w_left || a == DXA_w_start)
            pgs.left = v;
        else if (a == DXA_w_right || a == DXA_w_end)
            pgs.right = v;
        return;
    case DXE_w_headerReference: case DXE_w_footerReference:
        if (a == DXA_w_type && XVAL[0] != 'd')
            a_t = 1;                    /* (first, even: not the default) */
        else if (a == DXA_r_id)
            a_h = hash(XVAL);
        return;
    case DXE_w_style:
        if (a == DXA_w_type)
            st_type = XVAL[0] == 'p' ? 1 : XVAL[0] == 'c' ? 2 : 0;
        else if (a == DXA_w_styleId)
            a_h = hash(XVAL);
        else if (a == DXA_w_default)
            a_id = oon(XVAL);
        return;
    case DXE_w_fldChar:
        if (a == DXA_w_fldCharType) {
            if (XVAL[0] == 'b') {
                fld = 1;
                fk = fsup = 0;
            } else if (XVAL[0] == 's') {
                fld = 2;
                if (fk && part == DP_HF) {
                    em_field(fk);
                    fsup = 1;
                }
            } else
                fld = fsup = 0;
        }
        return;
    case DXE_w_fldSimple:
        if (a == DXA_w_instr)
            a_t = fkind(XVAL);
        return;
    case DXE_w_br:
        if (a == DXA_w_type)
            a_t = XVAL[0] == 'p' ? 1 : XVAL[0] == 'c' && XVAL[1] == 'o' ? 2 : 0;
        return;
    case DXE_w_abstractNum:
        if (a == DXA_w_abstractNumId && absn < NABS) {
            abs_id[absn] = cur_abs = (unsigned char)onum(XVAL);
            abs_num[absn++] = 0;
        }
        return;
    case DXE_w_lvl:
        if (a == DXA_w_ilvl)
            cur_lvl = (unsigned char)onum(XVAL);
        return;
    case DXE_w_num:
        if (a == DXA_w_numId && numn < NNUM) {
            num_id[numn] = (unsigned int)onum(XVAL);
            num_kind[numn++] = PK_BULLET;
        }
        return;
    case DXE_Relationship:
        if (a == DXA_Id)
            a_h = hash(XVAL);
        else if (a == DXA_Type) {
            v = (int)strlen(XVAL);
            a_t = v > 7 && !strcmp(XVAL + v - 7, "/header") ? 1 : v > 7 && !strcmp(XVAL + v - 7, "/footer") ? 2 : 0;
        } else if (a == DXA_Target)
            strncpy(a_tgt, XVAL[0] == '/' ? XVAL + 1 : XVAL, RELN - 1);
        return;
    }
}

/* a start tag's end: what its attributes said */
static void open(void)
{
    static unsigned char i, k;
    static int v;
    switch (e) {
    case DXE_w_b:
        if (ctx == CX_R)
            FLAG(SF_BOLD, a_on);
        return;
    case DXE_w_i:
        if (ctx == CX_R)
            FLAG(SF_ITALIC, a_on);
        return;
    case DXE_w_u:
        if (ctx == CX_R)
            FLAG(SF_ULINE, a_on);
        return;
    case DXE_w_pageBreakBefore:
        SET(F_PBB, a_on);
        return;
    case DXE_w_titlePg:
        nofirst = a_on;
        return;
    case DXE_w_spacing:
        if (ctx == CX_P && lval[0] && !lset) {
            v = lval[0];
            SET(F_SPACING, v >= 420 ? 4 : v >= 300 ? 3 : 2);
        }
        lval[0] = 0;
        return;
    case DXE_w_br:
        if (part < DP_DOC)
            return;
        if (tdepth || part == DP_HF)
            em_char(' ');
        else if (a_t == 1)
            em_pbreak();
        else if (!a_t)
            em_split();
        return;
    case DXE_w_fldSimple:
        if (part == DP_HF && a_t) {
            em_field(a_t);
            skip(0);
        } else if (part == DP_DOC)
            ++oxc.fields;
        return;
    case DXE_w_headerReference: case DXE_w_footerReference:
        if (a_t)
            return;
        for (i = 0; i < reln; ++i)
            if (rel_h[i] == a_h)
                strcpy(e == DXE_w_headerReference ? hdr_t : ftr_t, rel_t[i]);
        return;
    case DXE_w_style:
        cst = 0;
        if (!st_type || (i = st_new(a_h)) == 0xFF) {
            skip(0);                    /* (a table's or a list's style) */
            return;
        }
        cst = 1;
        tgt(PT_STY);
        if (a_id && st_type == 1)
            def_ps = i;
        return;
    case DXE_Relationship:
        if (a_t && reln < NREL) {
            rel_h[reln] = a_h;
            a_tgt[RELN - 1] = 0;
            strcpy(rel_t[reln++], a_tgt);
        }
        a_t = 0;
        return;
    }
    (void)k;
}

static void end(void)
{
    e = ed ? est[--ed] : 0;
    switch (e) {
    case DXE_w_p:
        if (part < DP_DOC)
            break;
        inpar = 0;
        ctx = 0;
        if (!tdepth) {
            em_endpar();
            if (sect_br) {
                em_pbreak();
                sect_br = 0;
            }
        }
        break;
    case DXE_w_pPr: case DXE_w_rPr:
        ctx = 0;
        if (inpar && !cst)
            tgt(tdepth ? PT_NONE : PT_PAR);
        break;
    case DXE_w_t:
        intext = 0;
        break;
    case DXE_w_instrText:
        ininstr = 0;
        break;
    case DXE_w_tbl:
        if (tdepth)
            --tdepth;
        break;
    case DXE_w_tr:
        if (tdepth == 1)
            em_endpar();
        break;
    case DXE_w_style:
        cst = 0;
        break;
    case DXE_w_docDefaults:
        indef = 0;
        break;
    case DXE_w_sectPr:
        insect = 0;
        if (inpar && !a_t)
            sect_br = 1;                /* (a section's end: a page break after its paragraph) */
        break;
    }
}

static void text(void)
{
    static unsigned char i, c;
    if (ininstr && fld == 1) {
        if (!fk)
            fk = fkind(XVAL);
        return;
    }
    if (!intext || fsup)
        return;
    for (i = 0; i < xm_vlen; ++i) {
        c = (unsigned char)XVAL[i];
        if (c < 0x20 && c != CH_TAB)
            XVAL[i] = ' ';
    }
    em_chars(XVAL, xm_vlen);
}

/* part p (its name: s, main RAM) read; 0: there is none */
static unsigned char FASTCALL rdpart(unsigned char p, const char *s)
{
    static unsigned char t;
    if (oz_part(s))
        return 0;
    part = p;
    skipd = ctx = indef = inpar = intext = ininstr = insect = 0;
    tdepth = fld = fsup = 0;
    cst = 0;
    tgt(PT_NONE);
    ed = 0;
    xm_skipa = 0;
    xm_init();
    for (;;) {
        t = xm_next();
        if (t == XT_EOF || t == XT_ERR)
            break;
        if (skipd) {
            xm_skipa = 1;
            if (t == XT_START) {
                ++skipd;
                if (nlook(&te) == DXE_w_txbxContent)
                    boxseen = 1;
            } else if (t == XT_END && !--skipd) {
                if (ed)
                    --ed;               /* (the element skipped: closed) */
                if (skipk == SK_PIC) {
                    if (boxseen)
                        ++oxc.boxes;
                    else
                        ++oxc.pics;
                }
            }
            continue;
        }
        switch (t) {
        case XT_START:
            e = nlook(&te);
            if (ed < NEST)
                est[ed++] = e;
            start();
            xm_skipa = !(dxe_aw[e >> 3] & (1 << (e & 7)));   /* (its attributes passed over) */
            break;
        case XT_ATTR: attr(); break;
        case XT_OPEN: open(); break;
        case XT_TEXT: text(); break;
        case XT_END: end(); break;
        }
    }
    return 1;
}

static void FASTCALL hfpart(unsigned char k, const char *t)
{
    if (!t[0])
        return;
    strcpy(XNAME, "word/");
    strncat(XNAME, t, XNAMEN - 6);
    em_hfbegin(k);
    rdpart(DP_HF, XNAME);
    em_hfend();
}

/* the names in main RAM for oz_part (XNAME: no token meanwhile) */
static unsigned char FASTCALL rdname(unsigned char p, const char *s)
{
    strcpy(XNAME, s);
    return rdpart(p, XNAME);
}

unsigned char dx_load(void)
{
    static unsigned char r;
    absn = numn = reln = 0;
    hdr_t[0] = ftr_t[0] = 0;
    nofirst = 0;
    pgs.head[0] = pgs.foot[0] = 0;
    pgs.flags = 0;
    if (em_init())
        return FE_MEM;
    MARK("ox<");
    rdname(DP_RELS, "word/_rels/document.xml.rels");
    rdname(DP_NUM, "word/numbering.xml");
    MARK("num");
    rdname(DP_STY, "word/styles.xml");
    MARK("sty");
#ifdef CW_PROF
    strcpy(XNAME, "word/document.xml");     /* (the profile: inflating alone) */
    if (!oz_part(XNAME))
        while (oz_read(XIN, XINN))
            ;
    MARK("inf");
#endif
    r = rdname(DP_DOC, "word/document.xml");
    MARK("doc");
    if (!r || oz_err)
        return oz_err == OZ_MEM ? FE_MEM : oz_err == OZ_IO ? FE_IO : FE_BAD;
    r = em_finish();
    if (r)
        return r;
    hfpart(1, hdr_t);
    hfpart(2, ftr_t);
    if (nofirst)
        pgs.flags |= PS_NOFIRST;
    em_page();
    return 0;
}
