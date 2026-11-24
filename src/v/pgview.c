/* Veranda Write on Veranda: the pages' side of the view (overlay 14,
   with page.c and hf.c; DECISIONS W44-W46): the rows with the bands
   between the pages, a page break's line, the header and footer made for
   the view and for the PDF, Insert > Page break, the default page setup
   read from the card. The panel is pgpanel.c's (overlay 16).

   The page setup panel (File > Page setup, Insert > Header and footer):
   under the text, as the find panel is (the view's bottom ta_bot moves
   up to PP_Y - 1): the paper (A4, Letter), the four margins (in
   centimetres on A4, inches on Letter; the arrows or the - and + boxes
   step them by a millimetre or a twentieth of an inch), the header and
   the footer (one line each: "centre", "left|right", "left|centre|right";
   the fields {page} {pages} {date} {name}), "Not on the first page",
   "For new documents" (the setup becomes the default, kept on the card
   in /VERANDA/.SYSTEM/CWPAGE.DEF), OK and Cancel. While it is open every
   key and click is its own: Tab and the up and down arrows move between
   its items, left and right (and Space) change one, typing edits a
   field, Return is OK, Esc Cancel. OK keeps the setup in the document
   (not an undo step: it marks the document modified) and wraps the text
   again when the paper or the margins changed.

   The bands (W45): a row that starts a page has the band above it (the
   page before's footer, a gap, its header: view2.c's rows_build sizes
   it), drawn here when draw_row draws the row; the last page's footer
   under the document's last line.

   Insert > Page break (Ctrl+Return): a page break's paragraph, [$12 0
   $12] and its CR, put before the cursor's paragraph (one split by a
   Return first when the cursor is inside it): one undo step.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrpath.h>
#include "cwv.h"
#include "view.h"
#pragma code-name ("OVERLAY14")
#pragma rodata-name ("OVL14RO")
#pragma bss-name ("OVL14BSS")

#define pgs_default e_pgs_default       /* (page.c: this overlay's, called straight) */

extern char run[100];                   /* (view.c: main RAM, what the kernel draws) */

static char nm[64];                     /* {name} */
static void t_band(void);

static void __fastcall__ text(const char *s, unsigned int x, unsigned int y)
{
    strncpy(run, s, sizeof run - 1);
    run[sizeof run - 1] = 0;
    WPutString(run, x, y);
}

static unsigned int __fastcall__ width(const char *s)
{
    unsigned int w = 0;
    while (*s)
        w += GetCharWidth(*s++);
    return w;
}

/* {name}: the document's name, its extension left out (pdfhf.c's too) */
void pg_name(void)
{
    char *p;
    strcpy(nm, docname[0] ? docname : docname[1] ? docname + 1 : "Untitled");
    p = strrchr(nm, '.');
    if (p && strlen(p) <= 4)
        *p = 0;
    hf_name = nm;
}

/* row i is a page break's paragraph (LF_PB: draw_row, through glue.s):
   a dotted line across the column a little above its baseline, "Page
   break" in its middle (W45) */
void __fastcall__ e_v_pbreak(unsigned char i)
{
    static unsigned int y, m, w;
    static const char *s;
    y = vr[i].y + vr[i].l.a - 3;
    if (y < TA_TOP || y > TA_BOT)
        return;
    strcpy(run, "Page break");
    UseFont(FONT_UI);
    currentMode = 0;
    for (w = 0, s = run; *s; ++s)
        w += GetCharWidth(*s);
    m = TX0 + (wrap_w >> 1);
    WHorizontalLine(0xAA, TX0, m - (w >> 1) - 6, y);
    WHorizontalLine(0xAA, m + (w >> 1) + 6, TX0 + wrap_w - 1, y);
    if (y >= TA_TOP + 7 && y + 3 <= TA_BOT)
        WPutString(run, m - (w >> 1), y + 3);
    UseFont(0);
}

/*---------------------------------------------------------------
  the rows: the line table's from top_line, their places and y; a row
  that starts a page has the band above it (W45: the page before's
  footer, a gap, its header; the document's first line its header)
 ---------------------------------------------------------------*/
static unsigned char __fastcall__ band_h(unsigned int pg)
{
    unsigned char h = 0;
    if (pg > 1) {
        h = BAND_GAP;
        if (HF_ON(pgs.foot, pg - 1))
            h += BAND_HF;
    }
    if (HF_ON(pgs.head, pg))
        h += BAND_HF;
    return h;
}

void e_rows_build(void)
{
    static unsigned int i, y, n, pg;
    static pos_t p;
    static struct vrow *r;
    static unsigned char m, *b;
    i = top_line;
    if (i >= lt_n)
        top_line = i = lt_n - 1;
    p = lt_start(i);
    n = par_number(i);
    pg_arg = i;
    pg_view();
    pg = pv_pg;
    y = TY0;
    vn = 0;
    r = vr;
    b = pv_st;                          /* (row vn's bit: *b & m) */
    m = 1;
    while (vn < VMAX && i < lt_n && y <= TA_BOT) {
        lt_get(i, &r->l);
        if (vn && (r->l.f & LF_FIRST))  /* (a numbered list's next: one more) */
            n = par[r->l.p].kind != PK_NUMBER ? 0 : n + 1;
        r->n = n;
        r->pos = p;
        r->g = 0;
        if (*b & m) {
            if (vn)
                ++pg;
            y += r->g = band_h(pg);
        } else if (!i)
            y += r->g = band_h(1);
        if (!(m <<= 1)) {
            m = 1;
            ++b;
        }
        r->pg = pg;
        r->y = y;
        p += r->l.len;
        y += r->l.h;
        ++i;
        ++vn;
        ++r;
    }
}

/* below the last row: the page's paper (the last page's footer under
   the document's last line, W45). clr_y: the screens are the paper from
   there down, as this left them (0xFFFF: not known; v_all and every
   ScrollRect forget it): a key typed at a short document's end cleared
   the half screen under it again (W43: 0.055 s) */
static unsigned int clr_pg;

void e_clear_below(void)
{
    static unsigned int y, pg;
    y = vn ? vr[vn - 1].y + vr[vn - 1].l.h : TY0;
    pg = vn && top_line + vn >= lt_n ? vr[vn - 1].pg : 0;
    if (y > TA_BOT || (y >= clr_y && pg == clr_pg))
        return;
    clr_y = y;
    clr_pg = pg;
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    SetPattern(0);
    WRectangle(PG_L, y, PG_R, TA_BOT);
    if (cur_on && cy >= y)
        cur_on = 0;
    if (pg && HF_ON(pgs.foot, pg)) {
        r11 = y;                        /* (the footer alone, under y) */
        r12 = pg;
        t_band();
    }
}


/*---------------------------------------------------------------
  the bands between the pages
 ---------------------------------------------------------------*/
/* the header or footer s of page pg, its baseline at y + 10, inside the
   text's width (only when all of its line is on the screen) */
static void __fastcall__ hf_line(const char *s, unsigned int pg, unsigned int y)
{
    static unsigned char k;
    static unsigned int w, x;
    if (y < TA_TOP || y + BAND_HF - 1 > TA_BOT)
        return;
    hf_make(s, pg, pg_tot > pg ? pg_tot : pg);
    UseFont(FONT_UI);
    currentMode = 0;
    for (k = 0; k < 3; ++k) {
        if (!hf_part[k][0])
            continue;
        w = width(hf_part[k]);
        x = TX0;
        if (k == 1)
            x += (wrap_w - w) >> 1;
        else if (k == 2)
            x += wrap_w - w;
        if (w > wrap_w)
            x = TX0;
        text(hf_part[k], x, y + 10);
    }
    UseFont(0);
}

/* the band of row i: y - g .. y - 1 */
static void __fastcall__ band(unsigned int top, unsigned int bot, unsigned int pg)
{
    static unsigned int y, a, b;
    a = top < TA_TOP ? TA_TOP : top;
    b = bot > TA_BOT ? TA_BOT : bot;
    if (a > b)
        return;
    pg_name();
    hf_today();
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    SetPattern(0);
    WRectangle(PG_L, a, PG_R, b);
    y = top;
    if (pg > 1) {
        if (HF_ON(pgs.foot, pg - 1)) {
            hf_line(pgs.foot, pg - 1, y);
            y += BAND_HF;
        }
        /* the gap: a grey band across the page, edged */
        a = y + 2 < TA_TOP ? TA_TOP : y + 2;
        b = y + 7 > TA_BOT ? TA_BOT : y + 7;
        if (a <= b) {
            SetPattern(2);
            WRectangle(PG_L, a, PG_R, b);
            SetPattern(0);
        }
        if (y + 2 >= TA_TOP && y + 2 <= TA_BOT)
            WHorizontalLine(0xFF, PG_L, PG_R, y + 2);
        if (y + 7 >= TA_TOP && y + 7 <= TA_BOT)
            WHorizontalLine(0xFF, PG_L, PG_R, y + 7);
        y += BAND_GAP;
    }
    if (HF_ON(pgs.head, pg))
        hf_line(pgs.head, pg, y);
}

void __fastcall__ e_v_band(unsigned char i)
{
    struct vrow *r = &vr[i];
    band(r->y - r->g, r->y - 1, r->pg);
}

/* the last page's footer (page r12) under y r11 */
static void t_band(void)
{
    pg_name();
    hf_today();
    hf_line(pgs.foot, r12, r11 + 2);
}



/*---------------------------------------------------------------
  a page break
 ---------------------------------------------------------------*/
void t_pbreak(void)
{
    static pos_t q;
    if (pp_on)
        return;
    ul_step(U_CMD, cur, anc);
    if (HAS_SEL()) {
        ty_style = style_typing(SEL_LO());
        cur = anc = doc_delete(SEL_LO(), SEL_HI());
    }
    q = par_start(cur);
    if (pos_norm(q) != cur) {
        run[0] = CH_CR;                 /* (inside a paragraph: split by a Return first) */
        cur = doc_insert(cur, (unsigned char *)run, 1, style_typing(cur));
        q = par_start(cur);
    }
    run[0] = K_PB;
    run[1] = 0;
    run[2] = K_PB;
    run[3] = CH_CR;
    if (tx_ins(q, (unsigned char *)run, 4))
        message("There is not enough memory", "for that.");
    lay_fix();
    cur = anc = pos_norm(q + 4);
    doc_err = 0;
    ul_break();
    ui_moved();
}

/*---------------------------------------------------------------
  the default for new documents: /VERANDA/.SYSTEM/CWPAGE.DEF (the PAGE
  chunk's 142 bytes)
 ---------------------------------------------------------------*/
static char defname[] = "CWPAGE.DEF";

void pg_loaddef(void)
{
    unsigned char h;
    vr_cd(VR_DATA);
    h = FOpen(defname, 'r');
    if (h >= 2) {
        if (FRead(h, iobuf, PGS_N) == PGS_N)
            memcpy(&pgs_def, iobuf, PGS_N);
        FClose(h);
    }
    vr_cdback();
}

