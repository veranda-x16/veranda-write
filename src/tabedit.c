/* Veranda Write: a table's rows and cells found, and the table edits
   (overlay 28 on the X16, with v/tbui.c; DECISIONS W62-W64).

   The table is paragraphs with codes at their starts (cw.h, FORMAT.md
   1.6), so every edit here is the text's: codes and CRs put in, taken
   out or changed (tx_ins, tx_del, tx_set), and undo, redo, the file,
   the clipboard and the layout see them as any other edit. A row is
   found through the line table (its line's width is TB_W), its cells by
   the codes its paragraphs start with.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#ifndef CW_HOST
#include "v/ovl28.h"                    /* (its entry points e_..., through glue.s) */
#endif
#include "cw.h"
#ifndef CW_HOST
#pragma code-name ("OVERLAY28")
#pragma rodata-name ("OVL28RO")
#pragma bss-name ("OVL28BSS")
extern unsigned char lb[128];           /* (view.c's: main RAM, cb_read's) */
#define RB lb
#else
static unsigned char rbuf[128];
#define RB rbuf
#endif
#define RBN 128
#include "tbhead.h"

pos_t tr_lo, tr_hi, tr_c[TB_MAXC + 1], tt_lo, tt_hi;
unsigned int tt_n;
/* (tr_n, tr_f, tr_w, tb_at: layout.c's, main RAM: v/tbcmd.c reads them) */

/* the place after the CR that ends q's paragraph (hi at most) */
static pos_t FASTCALL par_after(pos_t q, pos_t hi)
{
    static unsigned int n, i;
    static unsigned char c;
    while (q < hi) {
        n = cb_read(&tx, q, RB, hi - q > RBN ? RBN : (unsigned int)(hi - q));
        for (i = 0; i < n;) {
            c = RB[i];
            if (IS_KIND(c)) {
                if (i + 3 > n)
                    break;
                i += 3;
                continue;
            }
            ++i;
            if (c == CH_CR)
                return q + i;
        }
        if (!i)
            break;
        q += i;
    }
    return hi;
}

static unsigned char FASTCALL is_row(unsigned int i)
{
    static struct line l;
    if (i >= lt_n)
        return 0;
    lt_get(i, &l);
    return IS_TR(&l);
}

unsigned char FASTCALL tb_row(pos_t p)
{
    static struct line l;
    static pos_t q;
    if (p > tx.total)
        return 0;
    lt_find(p);
    lt_get(lf_i, &l);
    if (!IS_TR(&l))
        return 0;
    tr_lo = lf_pos;
    tr_hi = lf_pos + l.len;
    tr_n = 0;
    for (q = tr_lo; q < tr_hi; q = par_after(q, tr_hi)) {
        tb_head(q);
        if (q == tr_lo)
            tr_f = th_a;
        if ((q == tr_lo || th_k == K_TCELL) && tr_n < TB_MAXC) {
            tr_c[tr_n] = q;
            tr_w[tr_n++] = q == tr_lo ? th_w : th_a;
        }
    }
    tr_c[tr_n] = tr_hi;
    return 1;
}

unsigned char FASTCALL tb_table(pos_t p)
{
    static unsigned int i, k;
    if (!tb_row(p))
        return 0;
    lt_find(tr_lo);
    for (i = lf_i; i && is_row(i - 1); --i)
        ;
    tt_lo = lt_start(i);
    for (k = i; is_row(k); ++k)
        ;
    tt_n = k - i;
    tt_hi = k < lt_n ? lt_start(k) : tx.total;
    tb_row(p);                          /* (tr_ for p's row again) */
    return 1;
}

unsigned char FASTCALL tb_cell(pos_t p)
{
    static unsigned char k;
    for (k = tr_n; k > 1 && p < tr_c[k - 1]; --k)
        ;
    return k - 1;
}

pos_t FASTCALL tb_cstart(unsigned char k)
{
    return pos_norm(tr_c[k]);
}

/*---------------------------------------------------------------
  the edits
 ---------------------------------------------------------------*/
/* (iobuf: main RAM, tx_ins's) */
static unsigned char bn;

static void FASTCALL put3(unsigned char k, unsigned char a)
{
    iobuf[bn++] = k;
    iobuf[bn++] = a;
    iobuf[bn++] = k;
}

/* a cell's paragraph: its codes and its CR (first: the row's first) */
static void FASTCALL cellp(unsigned char first, unsigned char f, unsigned char w)
{
    if (first)
        put3(K_TROW, f);
    put3(K_TCELL, w);
    iobuf[bn++] = CH_CR;
}

static unsigned char FASTCALL ins(pos_t q)
{
    if (tx_ins(q, iobuf, bn))
        return DE_MEM;
    return 0;
}

unsigned char tb_insert(pos_t p, unsigned char rows, unsigned char cols)
{
    static pos_t q;
    static unsigned char r, k;
    static unsigned int w;
    doc_err = 0;
    if (tb_row(p))
        return DE_TB;                   /* (no table in a table) */
    if (cols < 1)
        cols = 1;
    if (cols > TB_MAXC)
        cols = TB_MAXC;
    q = par_start(p);
    if (pos_norm(q) != p) {
        iobuf[0] = CH_CR;               /* (inside a paragraph: split by a Return first) */
        p = doc_insert(p, iobuf, 1, style_typing(p));
        if (doc_err)
            return doc_err;
        q = par_start(p);
    }
    tb_at = q;
    for (r = 0; r < rows; ++r) {
        bn = 0;
        for (k = 0; k < cols; ++k) {
            w = 256 / cols + (k < 256 % cols);
            cellp(!k, 0, w > 255 ? 255 : (unsigned char)w);
        }
        if (ins(q))
            break;
        q += bn;
    }
    lay_fix();
    tb_at = pos_norm(tb_at);
    return doc_err;
}

unsigned char FASTCALL tb_addrow(pos_t p, unsigned char below)
{
    static unsigned char k;
    static pos_t q;
    if (!tb_row(p))
        return DE_TB;
    bn = 0;
    for (k = 0; k < tr_n; ++k)
        cellp(!k, tr_f, tr_w[k]);
    q = below ? tr_hi : tr_lo;
    k = ins(q);
    lay_fix();
    tb_at = pos_norm(q);
    return k;
}

/* the table's rows from its first: tb_row for each, the line table laid
   out again after each one's edits (lay_fix: a row stays one line); 0
   when there are no more */
static unsigned int rw_i, rw_n;

static void rows_from(void)
{
    lt_find(tt_lo);
    rw_i = lf_i;
    rw_n = tt_n;
}

static unsigned char row_next(void)
{
    lay_fix();
    if (!rw_n)
        return 0;
    --rw_n;
    return tb_row(lt_start(rw_i++));
}

/* the code at cell k's first paragraph (the K_TCELL's arg: 1, its paragraph code's end: 0) */
static pos_t FASTCALL at(unsigned char k, unsigned char arg)
{
    tb_head(tr_c[k]);
    return tr_c[k] + (arg ? th_co + 1 : th_po);
}

/* the cursor's row (its line: ri) after edits in its table: cell k's start */
static unsigned int ri;

static void FASTCALL at_cell(unsigned char k)
{
    if (tb_row(lt_start(ri)))
        tb_at = tb_cstart(k < tr_n ? k : tr_n - 1);
    else
        tb_at = pos_norm(tt_lo);
}

unsigned char FASTCALL tb_addcol(pos_t p, unsigned char right)
{
    static unsigned char kc, k, w;
    static pos_t q;
    if (!tb_table(p))
        return DE_TB;
    kc = tb_cell(p);
    ri = lf_i;
    rows_from();
    while (row_next())
        if (tr_n >= TB_MAXC)
            return DE_TB;               /* (8 cells a row at most) */
    rows_from();
    while (row_next()) {
        k = kc < tr_n ? kc : tr_n - 1;
        w = tr_w[k] >> 1;
        if (!w)
            w = 1;
        tb_head(tr_c[k]);
        if (th_co != 0xFF)
            tx_set(tr_c[k] + th_co + 1, tr_w[k] - w > 0 ? tr_w[k] - w : 1);
        bn = 0;
        if (!right && !k) {
            /* a new first cell: the row's code goes to it */
            if (th_ro != 0xFF)
                tx_del(tr_c[0] + th_ro, 3);
            cellp(1, tr_f, w);
            q = tr_c[0];
        } else {
            cellp(0, 0, w);
            q = tr_c[k + right];
        }
        if (ins(q))
            break;
    }
    lay_fix();
    at_cell(right ? kc + 1 : kc);
    return doc_err;
}

/* n bytes at a deleted, the character style after them kept: when the
   style in effect there was set inside them (a bold cell's text going
   on into the next row's), a code for it after the head codes of the
   paragraph now at a (unless one is there) */
static void FASTCALL del_keep(pos_t a, pos_t n)
{
    static unsigned char s0, s1, c;
    s0 = style_at(a);
    s1 = style_at(a + n);
    tx_del(a, n);
    if (s0 == s1)
        return;
    while (a + 2 < tx.total && IS_KIND(c = cb_byte(&tx, a)) && c != K_CS)
        a += 3;
    if (a < tx.total && cb_byte(&tx, a) == K_CS)
        return;
    bn = 0;
    put3(K_CS, s1);
    ins(a);
}

unsigned char FASTCALL tb_delrow(pos_t p)
{
    if (!tb_table(p))
        return DE_TB;
    if (tt_n == 1)
        return tb_deltable(p);
    del_keep(tr_lo, tr_hi - tr_lo);
    lay_fix();
    tb_at = pos_norm(tr_lo);
    return 0;
}

unsigned char FASTCALL tb_delcol(pos_t p)
{
    static unsigned char kc, k, n, w;
    if (!tb_table(p))
        return DE_TB;
    kc = tb_cell(p);
    ri = lf_i;
    n = 0;
    rows_from();
    while (row_next())
        if (tr_n > n)
            n = tr_n;
    if (n < 2)
        return tb_deltable(p);
    rows_from();
    while (row_next()) {
        if (kc >= tr_n || tr_n < 2)
            continue;                   /* (a row without that cell, or with one cell: kept) */
        k = kc ? kc - 1 : 1;            /* (its width to the cell before it, or after) */
        w = tr_w[k] + tr_w[kc] > 255 ? 255 : tr_w[k] + tr_w[kc];
        tx_set(at(k, 1), w);
        if (!kc) {
            bn = 0;                     /* (the row's code to its new first cell) */
            put3(K_TROW, tr_f);
            ins(at(1, 0));
        }
        del_keep(tr_c[kc], tr_c[kc + 1] - tr_c[kc]);
    }
    lay_fix();
    at_cell(kc);
    return doc_err;
}

unsigned char FASTCALL tb_deltable(pos_t p)
{
    if (!tb_table(p))
        return DE_TB;
    del_keep(tt_lo, tt_hi - tt_lo);
    lay_fix();
    tb_at = pos_norm(tt_lo);
    return 0;
}

unsigned char tb_widths(pos_t p, const unsigned char *w, unsigned char n)
{
    static unsigned char k;
    if (!tb_table(p))
        return DE_TB;
    rows_from();
    while (row_next())
        for (k = 0; k < tr_n && k < n; ++k) {
            tb_head(tr_c[k]);
            if (th_co != 0xFF)
                tx_set(tr_c[k] + th_co + 1, w[k] ? w[k] : 1);
        }
    lay_fix();
    tb_at = p;
    return 0;
}

unsigned char FASTCALL tb_grid(pos_t p, unsigned char on)
{
    if (!tb_table(p))
        return DE_TB;
    rows_from();
    while (row_next()) {
        tb_head(tr_lo);
        if (th_ro != 0xFF)
            tx_set(tr_lo + th_ro + 1, on ? th_a & ~TR_NOGRID : th_a | TR_NOGRID);
    }
    lay_fix();
    tb_at = p;
    return 0;
}

unsigned char tb_return(pos_t p, unsigned char t)
{
    static pos_t q;
    iobuf[0] = CH_CR;
    q = doc_insert(p, iobuf, 1, t);
    if (doc_err)
        return doc_err;
    q = par_start(q);                   /* (the new paragraph: the cell's further one) */
    tb_head(q);
    bn = 0;
    put3(K_TPARA, 0);
    ins(q + th_po);
    lay_fix();
    tb_at = pos_norm(q);
    return doc_err;
}

/*---------------------------------------------------------------
  what may be deleted, Backspace and Delete at a cell's edge, Tab
 ---------------------------------------------------------------*/
static unsigned char FASTCALL in_row(pos_t p)
{
    static struct line l;
    lt_find(p);
    lt_get(lf_i, &l);
    return IS_TR(&l);
}

unsigned char tb_delok(pos_t lo, pos_t hi)
{
    static pos_t r;
    static unsigned char k;
    if (!in_row(lo) && (hi >= tx.total || !in_row(hi)))
        return 1;
    if (!tb_row(lo))
        return 0;
    r = tr_lo;
    k = tb_cell(lo);
    return tb_row(hi) && tr_lo == r && tb_cell(hi) == k;
}

/* Backspace at p (no selection): 1 as always, 0 nothing (a cell's
   start), 2 the cursor to tb_at (after a table: into its last cell) */
unsigned char tb_bs(pos_t p)
{
    static pos_t q;
    q = pos_prev(p);
    if (q == p || cb_byte(&tx, q) != CH_CR)
        return 1;
    tb_head(q + 1);
    if (th_k == K_TPARA)
        return 1;                       /* (a cell's further paragraph: joined to the one before) */
    if (in_row(p))
        return 0;
    if (in_row(q)) {
        tb_at = q;
        return 2;
    }
    return 1;
}

/* Delete at p: 1 as always, 0 nothing (a cell's end, a table after) */
unsigned char tb_del(pos_t p)
{
    if (p >= tx.total || cb_byte(&tx, p) != CH_CR)
        return 1;
    tb_head(p + 1);
    if (in_row(p))
        return th_k == K_TPARA;
    return th_k != K_TROW;
}

/* the cell after p's (fwd) or before it: tb_at its start; 0 none */
unsigned char tb_next(pos_t p, unsigned char fwd)
{
    static unsigned char k;
    if (!tb_row(p))
        return 0;
    k = tb_cell(p);
    if (fwd) {
        if (k + 1 < tr_n) {
            tb_at = tb_cstart(k + 1);
            return 1;
        }
        if (tr_hi < tx.total && tb_row(tr_hi)) {
            tb_at = tb_cstart(0);
            return 1;
        }
        return 0;
    }
    if (k) {
        tb_at = tb_cstart(k - 1);
        return 1;
    }
    if (tr_lo && tb_row(tr_lo - 1)) {
        tb_at = tb_cstart(tr_n - 1);
        return 1;
    }
    return 0;
}

/* (oexp.c's, an .odt's column styles) the cells' widths the text has: a
   bit each in b (32 bytes); only the rows read (the line table says
   which lines they are: the whole text read took seconds) */
void FASTCALL tb_used(unsigned char *b)
{
    static pos_t p;
    static unsigned int k;
    static unsigned char c;
    static struct line l;
    memset(b, 0, 32);
    for (k = 0, p = 0; k < lt_n; ++k, p += l.len) {
        lt_get(k, &l);
        if (IS_TR(&l) && tb_row(p))
            for (c = 0; c < tr_n; ++c)
                b[tr_w[c] >> 3] |= 1 << (tr_w[c] & 7);
    }
}
