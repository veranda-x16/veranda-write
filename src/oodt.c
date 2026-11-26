/* Veranda Write: an OpenDocument text (.odt) converted (DECISIONS W50,
   W51; office.h).

   The parts read, each once from its start: styles.xml (the fonts'
   faces: which are monospaced; the default paragraph style, the named
   styles and their parents: headings by their names and outline levels,
   quotations, alignment, margins, indents, line height, page breaks,
   bold, italic, underline, size; the list styles: bulleted or numbered;
   the page layouts: the paper, the margins, the header's and footer's
   room; the master pages: the header and the footer, one line each,
   page-number and page-count as {page} and {pages}), then content.xml
   (its automatic styles, the same way, then the text: paragraphs,
   headings, spans, lists, tabs, spaces, line breaks, tables as text).
   Pictures, drawings and frames, footnotes and endnotes, comments, the
   tracked changes' deleted text, indexes' templates are left out and
   counted. White space is the format's: a run of spaces, tabs and line
   ends one space (none at a paragraph's start); text:s the spaces it says.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "office.h"
#include "oimp.h"
#ifndef CW_HOST
#pragma code-name ("OVERLAY20")
#pragma rodata-name ("OVL20RO")
#pragma bss-name ("OVL20BSS")
#define od_load e_od_load
#endif
#define OXN_ODT
#include "oxnames.h"
#include "ohelp.c"

static const struct ntab te = NTAB(ode), ta = NTAB(oda);

#define OD_STY 1
#define OD_DOC 2
static unsigned char part, skipd, skipk, boxseen, e, ctx, inbody, tdepth, lastsp, firstp;
static unsigned char a_fam, a_ol, a_ls, a_mp, a_n, hfk, inmaster, gotm, hfirst, firstmp;
#define CX_P 1                          /* paragraph-properties */
#define CX_T 2                          /* text-properties */
#define CX_L 3                          /* page-layout-properties */
#define CX_H 4                          /* header-footer-properties */
#define CX_C 5                          /* (1.6) table-column-properties, table-cell-properties */
#define SK_PIC 1
#define SK_BOX 2
static unsigned char tp, cst;           /* (pr_t's while properties are read; a style being defined) */
static unsigned int a_h, a_par, a_lay;
static unsigned char a_k;               /* (the style's kind, by its name) */
/* the font faces: monospaced ones */
#define NFF 12
static unsigned char ffn;
static unsigned int ff_h[NFF];
static unsigned char ff_m[NFF];
/* the list styles: bulleted or numbered */
#define NLS 16
static unsigned char lsn, ls_cur;
static unsigned int ls_h[NLS];
static unsigned char ls_k[NLS];
/* (1.6) the table-column styles' widths (twips), the table-cell styles'
   borders (TR_NOGRID: none): their names' hashes */
#define NTS 16
static unsigned char tsn, a_r, cellc;
static unsigned int ts_h[NTS], ts_v[NTS], ts_cur;

static unsigned int FASTCALL ts_get(unsigned int h)
{
    static unsigned char i;
    for (i = 0; i < tsn; ++i)
        if (ts_h[i] == h)
            return ts_v[i];
    return 0;
}

static void FASTCALL ts_put(unsigned int v)
{
    if (tsn < NTS) {
        ts_h[tsn] = ts_cur;
        ts_v[tsn++] = v;
    }
}

/* the lists open (their kinds), the spans open (their styles) */
#define NDEP 6
static unsigned char ld, lst[NDEP], initem;
/* the page layouts, the master pages */
#define NPL 4
static unsigned char pln, hfs;
static unsigned int pl_h[NPL];
static unsigned int pl_v[NPL][8];       /* width, height, top, bottom, left, right, the header's and footer's room */
#define NMP 4
static unsigned char mpn, main_mp;
static unsigned int mp_h[NMP];
static unsigned char mp_pl[NMP], mp_hf[NMP];

/* the elements open: their numbers (an END needs no look-up) */
#define NEST 48
static unsigned char est[NEST], ed;

static void FASTCALL skip(unsigned char k)
{
    skipd = 1;
    skipk = k;
    boxseen = 0;
}

static void FASTCALL tgt(unsigned char t)
{
    tp = pr_t = t;
}

/* a length's text in twips: "12.5pt" "2.54cm" "1in" "25mm" "3pc" (none: points) */
static int FASTCALL olen(const char *s)
{
    static long v;
    static unsigned int d;
    static unsigned char neg;
    v = 0;
    neg = *s == '-';
    if (neg)
        ++s;
    for (; *s >= '0' && *s <= '9'; ++s)
        v = v * 10 + (*s - '0');
    v *= 10000;
    if (*s == '.')
        for (d = 1000, ++s; *s >= '0' && *s <= '9'; ++s, d /= 10)
            v += (*s - '0') * d;
    /* v: ten-thousandths */
    if (s[0] == 'c')
        v = (v * 36 + 317) / 635;       /* (cm: 1440 / 2.54 = 566.93) */
    else if (s[0] == 'm' && s[1] == 'm')
        v = (v * 18 + 1587) / 3175;     /* (mm: 56.693) */
    else if (s[0] == 'i')
        v = (v * 18 + 62) / 125;        /* (in: 1440) */
    else if (s[0] == 'p' && s[1] == 'c')
        v = (v * 3 + 62) / 125;         /* (pica: 240) */
    else
        v = (v + 250) / 500;            /* (pt: 20) */
    if (v > 32000)
        v = 32000;
    return neg ? -(int)v : (int)v;
}

/* a style's name ("Heading_20_1" or its display name "Heading 1"): its
   kind (P_NO: none) */
static unsigned char FASTCALL st_kind(const char *s)
{
    static unsigned char k;
    k = 0;
    if (starts(s, "heading_20_"))
        k = s[11];
    else if (starts(s, "heading "))
        k = s[8];
    if (k >= '1' && k <= '9')
        return k >= '3' ? PK_H3 : PK_H1 + (k - '1');
    if (starts(s, "title"))
        return PK_H1;
    if (starts(s, "subtitle"))
        return PK_H2;
    if (starts(s, "quotations") || starts(s, "quote"))
        return PK_QUOTE;
    return P_NO;
}

static unsigned char FASTCALL lskind(unsigned int h)
{
    unsigned char i;
    for (i = 0; i < lsn; ++i)
        if (ls_h[i] == h)
            return ls_k[i];
    return PK_BULLET;
}

/* a percentage's text ("130%": 130); 0 when it is not one */
static unsigned int FASTCALL pct(const char *s)
{
    unsigned int v = 0;
    for (; *s >= '0' && *s <= '9'; ++s)
        v = v * 10 + (*s - '0');
    if (*s == '.')
        while (*++s >= '0' && *s <= '9')
            ;
    return *s == '%' ? v : 0;
}

/* a paragraph (text:p, text:h) begins: in a list (an item's) or not
   (a style's list only for a list's paragraphs: the format's rule) */
static void FASTCALL para(unsigned char h)
{
    em_par();
    if (tdepth)
        em_tbl(tdepth == 1 ? TB_PARA : TB_NEST, 0);   /* (1.6: a cell's: oemit.c's em_tbl) */
    em_rs = 0xFF;
    pr_clr(PT_RUN);
    em_run();
    tgt(PT_PAR);
    SET(F_LIST, initem && ld && !h ? lst[ld - 1] : 0);
    lastsp = 1;
}

/*---------------------------------------------------------------
  the tokens
 ---------------------------------------------------------------*/
static void start(void)
{
    a_n = 1;
    a_h = a_par = a_lay = 0;
    a_fam = a_ol = a_ls = 0;
    a_r = 1;
    a_mp = a_k = P_NO;
    switch (e) {
    case ODE_office_text:
        inbody = part == OD_DOC;
        break;
    case ODE_text_p: case ODE_text_h:
        if (inbody || hfk)
            para(e == ODE_text_h);
        break;
    case ODE_style_paragraph_properties:
        ctx = CX_P;
        if (tp != PT_DEF)
            tgt(cst ? PT_STY : PT_NONE);
        break;
    case ODE_style_text_properties:
        ctx = CX_T;
        if (tp != PT_DEF)
            tgt(cst ? PT_STY : PT_NONE);
        break;
    case ODE_style_page_layout_properties:
        ctx = CX_L;
        break;
    case ODE_style_header_footer_properties:
        ctx = CX_H;
        break;
    case ODE_style_table_column_properties: case ODE_style_table_cell_properties:
        ctx = CX_C;                     /* (1.6: a column's width, a cell's borders) */
        break;
    case ODE_style_header_style:
        hfs = 6;
        break;
    case ODE_style_footer_style:
        hfs = 7;
        break;
    case ODE_style_default_style:
        cst = 0;
        tgt(PT_NONE);
        break;
    case ODE_text_tab:
        if (inbody || hfk) {
            em_char(CH_TAB);
            lastsp = 0;
        }
        break;
    case ODE_text_line_break:
        if (inbody || hfk) {
            if (hfk)
                em_char(' ');
            else
                em_split();
            lastsp = 1;
        }
        break;
    case ODE_text_note:
        ++oxc.notes;
        skip(0);
        break;
    case ODE_office_annotation:
        ++oxc.comments;
        skip(0);
        break;
    case ODE_text_tracked_changes:
        skip(0);
        break;
    case ODE_draw_frame:
        skip(SK_PIC);
        break;
    case ODE_draw_custom_shape: case ODE_draw_g: case ODE_draw_rect: case ODE_draw_ellipse: case ODE_draw_line:
    case ODE_draw_polygon: case ODE_draw_path: case ODE_draw_control:
        ++oxc.pics;
        skip(0);
        break;
    case ODE_text_table_of_content_source: case ODE_text_alphabetical_index_source:
    case ODE_text_illustration_index_source: case ODE_text_index_title_template: case ODE_office_forms:
    case ODE_text_sequence_decls: case ODE_text_variable_decls: case ODE_text_user_field_decls:
    case ODE_text_bibliography_source: case ODE_text_object_index_source: case ODE_text_user_index_source:
    case ODE_text_table_index_source:
        skip(0);
        break;
    case ODE_text_page_number: case ODE_text_page_count: case ODE_text_date: case ODE_text_file_name:
        if (hfk) {
            em_field(e == ODE_text_page_number ? 1 : e == ODE_text_page_count ? 2 : e == ODE_text_date ? 3 : 4);
            lastsp = 0;
            skip(0);
        }
        break;
    case ODE_table_table:
        /* (1.6) a table kept: its rows' paragraphs (a table in a cell: its
           text the cell's, counted) */
        if (tdepth++) {
            if (inbody)
                ++oxc.tables;
        } else
            em_tbl(TB_START, 0);
        break;
    case ODE_table_table_row:
        if (tdepth == 1)
            em_tbl(TB_ROW, cellc = 0);
        break;
    case ODE_table_covered_table_cell:
        skip(0);
        break;
    case ODE_text_list_item:
        initem = 1;
        break;
    case ODE_text_list_header:
        initem = 0;
        break;
    case ODE_style_header: case ODE_style_footer:
        if (inmaster && !gotm) {
            hfk = e == ODE_style_header ? 1 : 2;
            em_hfbegin(hfk);
            mp_hf[mpn - 1] |= hfk;
        } else
            skip(0);
        break;
    case ODE_style_header_first:
        hfirst = 1;
        skip(0);
        break;
    case ODE_style_footer_first: case ODE_style_header_left: case ODE_style_footer_left:
        skip(0);
        break;
    }
}

static void attr(void)
{
    static unsigned char a, i;
    static int v;
    static unsigned int u;
    if (!(ode_aw[e >> 3] & obit[e & 7]))
        return;                         /* (its attributes not read) */
    a = nlook(&ta);
    if (!a)
        return;
    switch (a) {
    case ODA_style_name:
        a_h = hash(XVAL);
        if (e == ODE_style_style && a_k == P_NO)
            a_k = st_kind(XVAL);
        return;
    case ODA_text_style_name:
        a_h = hash(XVAL);
        return;
    case ODA_style_family:
        a_fam = XVAL[0] == 'p' ? 1 : XVAL[0] == 't' && XVAL[1] == 'e' ? 2
              : !memcmp(XVAL, "table-c", 7) ? 3 : 0;   /* (1.6: table-column, table-cell) */
        return;
    case ODA_table_style_name:          /* (1.6) */
        a_h = hash(XVAL);
        return;
    case ODA_table_number_columns_repeated: case ODA_table_number_columns_spanned:
        v = onum(XVAL);
        a_r = v < 1 ? 1 : v > 16 ? 16 : (unsigned char)v;
        return;
    case ODA_style_parent_style_name:
        a_par = hash(XVAL);
        return;
    case ODA_style_display_name:
        if (e == ODE_style_style && a_k == P_NO)
            a_k = st_kind(XVAL);
        return;
    case ODA_style_default_outline_level: case ODA_text_outline_level:
        v = onum(XVAL);
        a_ol = v < 1 ? 0 : v > 3 ? 3 : (unsigned char)v;
        return;
    case ODA_style_list_style_name:
        a_ls = lskind(hash(XVAL));
        return;
    case ODA_style_master_page_name:
        u = hash(XVAL);
        for (i = 0; i < mpn; ++i)
            if (mp_h[i] == u)
                a_mp = i;
        return;
    case ODA_style_page_layout_name:
        a_lay = hash(XVAL);
        return;
    case ODA_text_c:
        v = onum(XVAL);
        a_n = v < 1 ? 1 : v > 200 ? 200 : (unsigned char)v;
        return;
    case ODA_svg_font_family:
        if (e == ODE_style_font_face && mono(XVAL))
            a_n = 0;                    /* (a monospaced face) */
        return;
    case ODA_style_font_pitch:
        if (e == ODE_style_font_face && XVAL[0] == 'f')
            a_n = 0;
        return;
    }
    if (ctx == CX_C) {
        /* (1.6) a column style's width; a cell style's borders */
        if (a == ODA_style_column_width)
            ts_put(olen(XVAL));
        else if (a == ODA_fo_border || a == ODA_fo_border_top)
            ts_put(XVAL[0] == 'n' ? TR_NOGRID : 0);
        return;
    }
    if (ctx == CX_L || ctx == CX_H) {
        /* a page layout's (the last one begun), its header's or footer's room */
        if (!pln)
            return;
        v = olen(XVAL);
        if (v < 0)
            v = 0;
        u = (unsigned int)v;
        if (ctx == CX_H) {
            if (a == ODA_fo_min_height)
                pl_v[pln - 1][hfs] += u;
            else if ((a == ODA_fo_margin_bottom && hfs == 6) || (a == ODA_fo_margin_top && hfs == 7))
                pl_v[pln - 1][hfs] += u;
            return;
        }
        i = a == ODA_fo_page_width ? 0 : a == ODA_fo_page_height ? 1 : a == ODA_fo_margin_top ? 2
            : a == ODA_fo_margin_bottom ? 3 : a == ODA_fo_margin_left ? 4 : a == ODA_fo_margin_right ? 5 : 8;
        if (i < 8)
            pl_v[pln - 1][i] = u;
        return;
    }
    if (ctx == CX_P) {
        switch (a) {
        case ODA_fo_text_align:
            i = XVAL[0];
            SET(F_ALIGN, i == 'c' ? PA_CENTRE : i == 'r' || i == 'e' ? PA_RIGHT : i == 'j' ? PA_JUSTIFY : PA_LEFT);
            return;
        case ODA_fo_margin_left:
            SET(F_LEFT, px(olen(XVAL), 240));
            return;
        case ODA_fo_margin_right:
            SET(F_RIGHT, px(olen(XVAL), 240));
            return;
        case ODA_fo_margin_top:
            SET(F_BEFORE, px(olen(XVAL), 254));
            return;
        case ODA_fo_margin_bottom:
            SET(F_AFTER, px(olen(XVAL), 254));
            return;
        case ODA_fo_margin:
            v = olen(XVAL);
            SET(F_LEFT, px(v, 240));
            SET(F_RIGHT, px(v, 240));
            SET(F_BEFORE, px(v, 254));
            SET(F_AFTER, px(v, 254));
            return;
        case ODA_fo_text_indent:
            v = olen(XVAL);
            SET(F_FIRST, v < 0 ? -(signed char)px(-v, 120) : (signed char)px(v, 120));
            return;
        case ODA_fo_line_height:
            u = pct(XVAL);
            SET(F_SPACING, u >= 175 ? 4 : u >= 135 ? 3 : 2);
            return;
        case ODA_fo_break_before:
            SET(F_PBB, XVAL[0] == 'p');
            return;
        case ODA_fo_break_after:
            SET(F_PBA, XVAL[0] == 'p');
            return;
        }
        return;
    }
    if (ctx == CX_T) {
        switch (a) {
        case ODA_fo_font_weight:
            FLAG(SF_BOLD, XVAL[0] == 'b' || (XVAL[0] >= '6' && XVAL[0] <= '9'));
            return;
        case ODA_fo_font_style:
            FLAG(SF_ITALIC, XVAL[0] == 'i' || XVAL[0] == 'o');
            return;
        case ODA_style_text_underline_style:
            FLAG(SF_ULINE, strcmp(XVAL, "none") != 0);
            return;
        case ODA_fo_font_size:
            u = pct(XVAL);
            if (u)
                pr_pct(u);
            else {
                v = (olen(XVAL) + 10) / 20;
                SET(F_SIZE, v < 1 ? 1 : v > 127 ? 127 : v);
            }
            return;
        case ODA_style_font_name:
            u = hash(XVAL);
            for (i = 0; i < ffn; ++i)
                if (ff_h[i] == u)
                    SET(F_MONO, ff_m[i]);
            return;
        case ODA_fo_font_family:
            SET(F_MONO, mono(XVAL));
            return;
        }
    }
}

/* a start tag's end */
static void open(void)
{
    static unsigned char i;
    static unsigned int w;
    switch (e) {
    case ODE_style_font_face:
        if (ffn < NFF) {
            ff_h[ffn] = a_h;
            ff_m[ffn++] = !a_n;
        }
        return;
    case ODE_style_style:
        cst = 0;
        tgt(PT_NONE);
        if (a_fam == 3) {
            ts_cur = a_h;               /* (1.6: a table column's or cell's style) */
            return;
        }
        if (!a_fam || st_new(a_h) == 0xFF) {
            skip(0);                    /* (a table's, a frame's ...) */
            return;
        }
        cst = 1;
        tgt(PT_STY);
        if (part == OD_DOC && a_fam == 1)
            st_auto(a_par ? st_find(a_par) : 0xFF);   /* (content.xml's: the paragraph's own) */
        else if (a_par)
            st_base(st_find(a_par));
        if (a_k != P_NO)
            SET(F_KIND, a_k);
        if (a_ol)
            SET(F_KIND, PK_H1 + a_ol - 1);
        if (a_ls)
            SET(F_LIST, a_ls);
        if (a_mp != P_NO)
            SET(F_MP, a_mp);
        return;
    case ODE_style_default_style:
        tgt(a_fam == 1 ? PT_DEF : PT_NONE);
        cst = 0;
        return;
    case ODE_text_list_style:
        ls_cur = lsn < NLS ? lsn : 0xFF;
        if (lsn < NLS) {
            ls_h[lsn] = a_h;
            ls_k[lsn++] = 0;
        }
        return;
    case ODE_text_list_level_style_number: case ODE_text_list_level_style_bullet:
    case ODE_text_list_level_style_image:
        if (ls_cur != 0xFF && !ls_k[ls_cur])
            ls_k[ls_cur] = e == ODE_text_list_level_style_number ? PK_NUMBER : PK_BULLET;
        return;
    case ODE_text_list:
        if (ld < NDEP) {
            lst[ld] = a_h ? lskind(a_h) : ld ? lst[ld - 1] : PK_BULLET;
            ++ld;
        }
        return;
    case ODE_style_page_layout:
        if (pln < NPL) {
            pl_h[pln] = a_h;
            memset(pl_v[pln], 0, sizeof pl_v[0]);
            ++pln;
        }
        return;
    case ODE_style_master_page:
        inmaster = 1;
        if (mpn < NMP) {
            mp_h[mpn] = a_h;
            mp_hf[mpn] = 0;
            mp_pl[mpn] = 0xFF;
            for (i = 0; i < pln; ++i)
                if (pl_h[i] == a_lay)
                    mp_pl[mpn] = i;
            ++mpn;
        }
        return;
    case ODE_table_table_column:        /* (1.6) its width, a_r times */
        if (tdepth == 1)
            for (w = ts_get(a_h); a_r--;)
                em_tbl(TB_COL, w);
        return;
    case ODE_table_table_cell:
        if (tdepth == 1) {
            em_tbl(TB_CELL, 0);
            em_tbl(TB_SPAN, a_r);
            if (!cellc++)
                em_tbl(TB_BORD, ts_get(a_h));   /* (the row's first cell's borders: the row's) */
        }
        return;
    case ODE_text_p: case ODE_text_h:
        if (!inbody && !hfk)
            return;
        if (a_h)
            em_pstyle(st_find(a_h));
        if (e == ODE_text_h)
            SET(F_KIND, a_ol ? PK_H1 + a_ol - 1 : PK_H1);
        if (firstp && !hfk) {
            firstp = 0;
            firstmp = em_mp();
        }
        return;
    case ODE_text_span:
        em_span(a_h ? st_find(a_h) : 0xFF);
        return;
    case ODE_text_s:
        if (inbody || hfk) {
            em_spaces(a_n);
            lastsp = 0;
        }
        return;
    }
}

static void end(void)
{
    e = ed ? est[--ed] : 0;
    switch (e) {
    case ODE_office_text:
        inbody = 0;
        break;
    case ODE_text_p: case ODE_text_h:
        if (inbody || hfk)
            em_endpar();
        break;
    case ODE_table_table_cell:
        if (tdepth == 1 && inbody)
            em_tbl(TB_ENDC, 0);         /* (1.6: an empty cell: an empty paragraph) */
        break;
    case ODE_style_table_column_properties: case ODE_style_table_cell_properties:
        ctx = 0;
        break;
    case ODE_style_paragraph_properties: case ODE_style_text_properties: case ODE_style_page_layout_properties:
    case ODE_style_header_footer_properties:
        ctx = 0;
        break;
    case ODE_style_header_style: case ODE_style_footer_style:
        hfs = 0;
        break;
    case ODE_style_style: case ODE_style_default_style:
        cst = 0;
        tgt(PT_NONE);
        break;
    case ODE_text_list_style:
        ls_cur = 0xFF;
        break;
    case ODE_text_list:
        if (ld)
            --ld;
        break;
    case ODE_text_list_item:
        initem = 0;
        break;
    case ODE_text_span:
        em_unspan();
        break;
    case ODE_table_table:
        if (tdepth && !--tdepth)
            em_tbl(TB_END, 0);          /* (1.6: the table's end) */
        break;
    case ODE_style_header: case ODE_style_footer:
        if (hfk) {
            em_hfend();
            hfk = 0;
            main_mp = mpn - 1;
        }
        break;
    case ODE_style_master_page:
        inmaster = 0;
        if (mp_hf[mpn - 1])
            gotm = 1;                   /* (the first master page with a header or a footer: the document's) */
        break;
    }
}

/* text: the format's white space (a run of it one space) */
static void text(void)
{
    static unsigned char i, n, c;
    if (!inbody && !hfk)
        return;
    for (i = n = 0; i < xm_vlen; ++i) {
        c = (unsigned char)XVAL[i];
        if (c < 0x20 || c == ' ') {
            if (lastsp)
                continue;
            c = ' ';
            lastsp = 1;
        } else
            lastsp = 0;
        XVAL[n++] = c;
    }
    em_chars(XVAL, n);
}

static unsigned char FASTCALL rdpart(unsigned char p, const char *s)
{
    static unsigned char t;
    strcpy(XNAME, s);                   /* (main RAM, for oz_part) */
    if (oz_part(XNAME))
        return 0;
    part = p;
    skipd = ctx = inbody = tdepth = 0;
    ld = initem = 0;
    hfk = inmaster = 0;
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
                t = nlook(&te);
                if (t == ODE_text_changed_region)
                    ++oxc.changes;
                else if (t == ODE_text_p && skipk == SK_PIC)
                    boxseen = 1;        /* (a frame with text: a text box) */
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
            xm_skipa = !(ode_aw[e >> 3] & obit[e & 7]);   /* (its attributes passed over) */
            break;
        case XT_ATTR: attr(); break;
        case XT_OPEN: open(); break;
        case XT_TEXT: text(); break;
        case XT_END: end(); break;
        }
    }
    return 1;
}

unsigned char od_load(void)
{
    static unsigned char r, i;
    static unsigned int *v;
    ffn = lsn = pln = mpn = 0;
    main_mp = gotm = hfirst = 0;
    firstp = 1;
    firstmp = P_NO;
    ls_cur = 0xFF;
    hfs = 0;
    pgs.head[0] = pgs.foot[0] = 0;
    pgs.flags = 0;
    if (em_init())
        return FE_MEM;
    rdpart(OD_STY, "styles.xml");
    r = rdpart(OD_DOC, "content.xml");
    if (!r || oz_err)
        return oz_err == OZ_MEM ? FE_MEM : oz_err == OZ_IO ? FE_IO : FE_BAD;
    r = em_finish();
    if (r)
        return r;
    /* the page: the document's master page's layout; the header's and
       footer's room in the top and bottom margins */
    if (mpn && (i = mp_pl[main_mp]) != 0xFF) {
        v = pl_v[i];
        if (v[0] && v[1]) {
            pgs.w = v[0];
            pgs.h = v[1];
        }
        pgs.top = v[2] + (pgs.head[0] ? v[6] : 0);
        pgs.bottom = v[3] + (pgs.foot[0] ? v[7] : 0);
        pgs.left = v[4];
        pgs.right = v[5];
    }
    /* not on the first page: its master page has none, or an empty first one */
    if ((pgs.head[0] || pgs.foot[0])
        && (hfirst || (firstmp != P_NO && firstmp < mpn && firstmp != main_mp && !mp_hf[firstmp])))
        pgs.flags |= PS_NOFIRST;
    em_page();
    return 0;
}
