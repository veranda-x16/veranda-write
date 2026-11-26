/* Veranda Write: a table's row laid out (overlay 27 on the X16, with
   v/tbview.c, which draws it; DECISIONS W62, W63).

   A row is one line of the line table: word wrap (layfix.c) meets a
   paragraph that starts with a row's code and hands it here (tb_wrap):
   the row's paragraphs are taken, cell by cell, each wrapped by word
   wrap itself inside its cell (wrap_w the cell's text's width, tb_in
   set so that it does not hand them here again), the cells side by side,
   the row as tall as its tallest cell; its line's width is TB_W (no
   other line's is), its paragraph style 0. The lines of its cells (its
   sub-lines) are kept for the row laid out last (tb_lay: their offsets,
   heights, styles, cells, y), so drawing it, its cursor and a click in
   it need not wrap it again (tb_ok: the text not changed since,
   lay_dirty clears it).

   A cell's box: its left border at tc_x[k] (from the text's left edge),
   its text TB_PADL in and TB_PADX narrower (a border, 3 pixels each
   side), TB_PADT down from the row's top (its border and 2), TB_PADB
   above its bottom; the table's last row has its bottom border too.
   Widths are 256ths of the text's: tc_x[k] = wrap_w x (the widths before
   it) / 256, the last border inside the text.

   What a row can hold: 2,047 bytes (a line's length, 11 bits; typing
   stops at TB_ROWMAX), 255 pixels (a line's height: what is below is
   not drawn), TB_SL sub-lines kept (the rest measured, not drawn).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cw.h"
#ifndef CW_HOST
#pragma code-name ("OVERLAY27")
#pragma rodata-name ("OVL27RO")
#pragma bss-name ("OVL27BSS")
#endif
#include "tbhead.h"

unsigned char tc_n, tc_f, tc_h, tc_last, tc_se, tc_s;
unsigned int tc_x[TB_MAXC + 1];
pos_t tc_p, tc_end;
static unsigned int tc_w;               /* (the text's width it was laid out in) */
unsigned char sl_n;
struct sub tsl[TB_SL];

/* a border: the widths before it (256ths of the text's) */
static unsigned int FASTCALL bx(unsigned int cum)
{
    static unsigned int x;
    x = (unsigned int)(((unsigned long)tc_w * cum) >> 8);
    return x > tc_w - 1 ? tc_w - 1 : x;
}

static unsigned char ch[TB_MAXC];       /* each cell's height so far */

void FASTCALL tb_lay(pos_t p, unsigned char s)
{
    static unsigned int o, o0, cum, rem, cw, h;
    static unsigned char c, num, numk, m, cut, k;
    static struct sub ns;
    if (tb_ok && p == tc_p && s == tc_s && wrap_w == tc_w)
        return;                         /* (laid out last, the text as it was) */
    tc_p = p;
    tc_s = s;
    tc_w = wrap_w;
    rem = tx.total - p > 0xFFFF ? 0xFFFF : (unsigned int)(tx.total - p);
    tb_head(p);
    tc_f = th_a;
    tc_n = sl_n = c = num = numk = cut = 0;
    cum = cw = 0;
    o = 0;
    for (;;) {
        if (o) {
            tb_head(p + o);
            if (th_k != K_TCELL && th_k != K_TPARA)
                break;                  /* (the next row, or the table's end) */
        }
        if ((!o || th_k == K_TCELL) && tc_n < TB_MAXC) {
            /* a cell: its borders, its text's width */
            tc_x[tc_n] = bx(cum);
            cum += o ? th_a : th_w;
            c = tc_n++;
            tc_x[tc_n] = bx(cum);
            cw = TB_CW(c);
            ch[c] = num = numk = 0;
        }
        /* its paragraph, line by line, inside the cell */
        o0 = o;
        wrap_w = cw;
        tb_in = lc_first = 1;
        k = 1;
        for (;;) {
            wrap(p + o, s);
            if (k) {
                m = par[lc_ps].kind == PK_NUMBER;   /* (a numbered list in the cell: */
                num = m && numk ? num + 1 : m;      /*  1 + the numbered paragraphs before it) */
                numk = m;
            }
            if (o + wr_len > 2047) {
                cut = 1;                /* (a row longer than a line can be) */
                break;
            }
            if (sl_n < TB_SL) {
                ns.l.len = wr_len;
                ns.l.h = wr_h;
                ns.l.a = wr_a;
                ns.l.s = s;
                ns.l.f = wr_f;
                ns.l.p = lc_ps;
                ns.l.w = wr_w;
                ns.off = o;
                h = TB_PADT + ch[c];
                ns.y = h > 255 ? 255 : (unsigned char)h;
                ns.c = c;
                ns.num = k ? num : 0;
                memcpy(tsl + sl_n++, &ns, sizeof ns);
            }
            k = 0;
            h = ch[c] + wr_h;
            ch[c] = h > 255 ? 255 : (unsigned char)h;
            o += wr_len;
            s = wr_s;
            if ((wr_f & LF_PARA) || !wr_len || o >= rem)
                break;
        }
        tb_in = 0;
        wrap_w = tc_w;
        if (cut) {
            if (o0)
                o = o0;                 /* (the row ends before that paragraph) */
            else {
                /* (a first paragraph longer than a line can be: cut at its last whole sub-line) */
                while (sl_n && tsl[sl_n - 1].off + tsl[sl_n - 1].l.len > 2047)
                    --sl_n;
                o = sl_n ? tsl[sl_n - 1].off + tsl[sl_n - 1].l.len : 1;
            }
            break;
        }
        if (!wr_len || o >= rem || !(wr_f & LF_PARA))
            break;
    }
    tb_in = 0;
    wrap_w = tc_w;
    tc_end = p + o;
    tc_se = s;
    tb_head(tc_end);
    tc_last = o >= rem || th_k != K_TROW;
    for (c = k = 0; c < tc_n; ++c)
        if (ch[c] > k)
            k = ch[c];
    h = TB_PADT + k + TB_PADB + tc_last;
    tc_h = h > 255 ? 255 : (unsigned char)h;
    tb_ok = 1;
}

/* the row at tb_p (its first paragraph's first line) as one line, if it is one */
void tb_wrap(void)
{
    static pos_t p;
    p = tb_p;
    tb_head(p);
    tb_is = th_k == K_TROW;
    if (!tb_is)
        return;
    tb_ok = 0;                          /* (laid out again: word wrap's word) */
    tb_lay(p, tb_s);
    wr_len = (unsigned int)(tc_end - p);
    wr_h = tc_h;
    wr_a = sl_n ? TB_PADT + tsl[0].l.a : TB_PADT;
    wr_s = tc_se;
    wr_w = TB_W;
    /* (it ends its last paragraph: the next line starts one; a row cut
       inside a paragraph does not) */
    wr_f = LF_FIRST;
    if (cb_byte(&tx, tc_end - 1) == CH_CR)
        wr_f |= LF_PARA;
    lc_ps = 0;
    lc_first = wr_f & LF_PARA;
}

unsigned char FASTCALL tb_sub(unsigned int off)
{
    static unsigned char j;
    static unsigned int a;
    for (j = 0; j < sl_n; ++j) {
        a = tsl[j].off;
        if (off >= a && off < a + tsl[j].l.len)
            return j;
    }
    return 0xFF;
}


/* (pdf.c's, W63: the PDF's rows) tb_is 0xFF: the row at tb_p (style
   tb_s) laid out, in tb_xb its sub-lines' number, its cells', flags,
   last-row flag, then tc_x; else sub-line tb_is (struct sub) there */
#ifdef CW_HOST
unsigned char tb_xb[64];
#define TXB tb_xb
#else
extern unsigned char lb[128];           /* (view.c's: main RAM) */
#define TXB lb
#endif

void tb_pdf(void)
{
    if (tb_is != 0xFF) {
        memcpy(TXB, tsl + tb_is, sizeof(struct sub));
        return;
    }
    tb_lay(tb_p, tb_s);
    TXB[0] = sl_n;
    TXB[1] = tc_n;
    TXB[2] = tc_f;
    TXB[3] = tc_last;
    memcpy(TXB + 4, tc_x, sizeof tc_x);
}
