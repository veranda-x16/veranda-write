/* Veranda Write: AI help's text (DECISIONS W68; Veranda's docs/AIREQ.md).
   The document's side of it, portable (the host's unit tests build it;
   on the X16 overlay 31, read from SPELL.OVL when AI help is used: the
   files and the boxes are v/aiui.c's).

   What goes to Veranda Ask (AI.IN): the characters of [ai_lo, ai_hi) as
   they are (the text is in Veranda's set, ISO-8859-16, as AI.IN wants
   it), CR at each paragraph's end, no codes: a character style, a
   paragraph style, a cell's further paragraph, a page break are left
   out. A range that is not inside one cell of a table, or that holds a
   table (a row's or a cell's code), is refused: its answer could not be
   put back without breaking the table. 8,192 bytes at most.

   What comes back (AI.OUT) is put in by ai_begin, ai_add, ai_end (the
   caller makes it one undo step): AM_REPLACE puts the answer in place of
   [ai_lo, ai_hi) (inserted at ai_lo first, then the old text deleted:
   the answer's paragraphs take the paragraph style of ai_lo's paragraph,
   as Return in the middle of it gives, never Body after a heading's
   end), its characters in the style of the range's first character;
   AM_AFTER inserts it as new paragraphs after the paragraph that holds
   the range's end (a CR first: Return at that paragraph's end, Body
   after a heading; its characters in the first character's style, a
   heading's made Body's: 12 points, not bold). In a table's cell the new
   paragraphs are the cell's further paragraphs ([K_TPARA 0 K_TPARA]
   after each CR, as tb_return makes them). The answer's CRs (and LFs)
   end paragraphs, a run of them one paragraph's end, those at its start
   and end left out; characters the text cannot hold are left out. After
   ai_end, [ai_lo, ai_hi) is the answer.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cw.h"
#ifndef CW_HOST
#pragma code-name ("OVERLAY31")
#pragma rodata-name ("OVL31RO")
#pragma bss-name ("OVL31BSS")
#pragma data-name ("OVL31DATA")
#endif

unsigned int ai_n;                      /* (ai_check) AI.IN's bytes */
static unsigned char a_cell;            /* the range is in a table's cell */

static unsigned char FASTCALL in_row(pos_t p)
{
    static struct line l;
    lt_find(p);
    lt_get(lf_i, &l);
    return IS_TR(&l);
}

/* the end of the paragraph holding p: its CR's place (DOC_LEN: none) */
static pos_t FASTCALL par_cr(pos_t p)
{
    static unsigned char c;
    while (p < DOC_LEN) {
        c = cb_byte(&tx, p);
        if (c == CH_CR)
            break;
        p += IS_KIND(c) ? 3 : 1;
    }
    return p;
}

void FASTCALL ai_para(pos_t p)
{
    ai_lo = pos_norm(par_start(p));
    ai_hi = par_cr(ai_lo);
}

unsigned char ai_check(void)
{
    static pos_t p;
    static unsigned char c, any;
    ai_n = 0;
    any = 0;
    for (p = ai_lo; p < ai_hi; ++p) {
        c = cb_byte(&tx, p);
        if (IS_KIND(c)) {
            if (c == K_TROW || c == K_TCELL)
                return AE_CELLS;        /* (a table, or a part of one, in it) */
            p += 2;
            continue;
        }
        if (c != CH_CR && !IS_CHAR(c))
            continue;
        if (++ai_n > AI_MAX)
            return AE_LONG;
        if (c != CH_CR && c != ' ' && c != CH_TAB)
            any = 1;
    }
    if (!tb_delok(ai_lo, ai_hi))
        return AE_CELLS;                /* (from a cell out of it, or into one) */
    return any ? 0 : AE_EMPTY;
}

void ai_text(void)
{
    static pos_t p;
    static unsigned int n;
    static unsigned char c;
    n = 0;
    for (p = ai_lo; p < ai_hi; ++p) {
        c = cb_byte(&tx, p);
        if (IS_KIND(c)) {
            p += 2;
            continue;
        }
        if (c != CH_CR && !IS_CHAR(c))
            continue;
        iobuf[n++] = c;
        if (n == IOBUF) {
            plat_ai_put(iobuf, n);
            n = 0;
        }
    }
    if (n)
        plat_ai_put(iobuf, n);
}

/*---------------------------------------------------------------
  the answer put in
 ---------------------------------------------------------------*/
static pos_t a_p, a_start, a_q;         /* where the next run goes; the answer's start; brk's paragraph */
static unsigned char a_t, a_mode, a_brk, a_any, a_err;
static unsigned int a_n;                /* the run in iobuf */
static const unsigned char tpara[3] = { K_TPARA, 0, K_TPARA };

static void flush(void)
{
    if (!a_n || a_err)
        return;
    a_p = doc_insert(a_p, iobuf, a_n, a_t);
    if (doc_err)
        a_err = doc_err;
    a_n = 0;
}

/* a paragraph's end at a_p (in a cell: the new paragraph the cell's) */
static void brk(void)
{
    static pos_t q;
    if (a_err)
        return;
    iobuf[0] = CH_CR;
    a_p = doc_insert(a_p, iobuf, 1, a_t);
    if (doc_err) {
        a_err = doc_err;
        return;
    }
    for (q = a_p; q >= 3 && IS_KIND(cb_byte(&tx, q - 1)); q -= 3)
        ;                               /* (the new paragraph's start: after the CR) */
    a_q = q;
    if (!a_cell)
        return;
    if (cb_byte(&tx, q) == K_PS)
        q += 3;                         /* (its paragraph code first, as the format says) */
    memcpy(iobuf, tpara, 3);
    if (tx_ins(q, iobuf, 3)) {
        a_err = DE_MEM;
        return;
    }
    lay_fix();
    a_p = pos_norm(a_p + 3);
}

void FASTCALL ai_begin(unsigned char mode)
{
    static unsigned char ps;
    static struct cstyle s;
    a_mode = mode;
    a_n = a_any = a_err = a_brk = 0;
    a_cell = in_row(ai_lo);
    a_t = style_at(ai_lo);
    if (mode == AM_REPLACE) {
        a_p = a_start = ai_lo;
        tk_a = ai_lo;                   /* (the old text: moved by the insertions) */
        tk_b = ai_hi;
        return;
    }
    for (a_p = ai_hi; a_p > ai_lo && IS_KIND(cb_byte(&tx, a_p - 1)); a_p -= 3)
        ;                               /* (the range's last character: not a code's byte) */
    a_p = par_cr(a_p > ai_lo ? a_p - 1 : ai_lo);
    ps = par_at(par_start(a_p));        /* (the paragraph the answer comes after) */
    if (IS_HEAD(par[ps].kind)) {
        s = sty[a_t];                   /* (Body's characters after a heading: Return's) */
        ps = sty_find(s.font, DEF_SIZE, s.flags & ~SF_BOLD, s.colour);
        if (ps != 0xFF)
            a_t = ps;
    }
    a_brk = 1;                          /* (a paragraph's end first) */
}

void ai_add(const unsigned char *b, unsigned int n)
{
    static unsigned char c;
    for (; n && !a_err; --n) {
        c = *b++;
        if (c == CH_CR || c == 10) {
            if (a_any)
                a_brk = 1;
            continue;
        }
        if (!IS_CHAR(c))
            continue;
        if (a_brk) {
            flush();
            brk();
            a_brk = 0;
            if (!a_any && a_mode == AM_AFTER)
                a_start = a_q;          /* (the new paragraph's start: its codes before the text) */
        }
        a_any = 1;
        iobuf[a_n++] = c;
        if (a_n == AI_RUN)
            flush();
    }
}

unsigned char ai_end(void)
{
    flush();
    if (a_err)
        return a_err;
    if (!a_any)
        return DE_AI;                   /* (nothing in it: nothing done) */
    if (a_mode == AM_REPLACE) {
        ai_hi = doc_delete(tk_a, tk_b);
        if (doc_err)
            return doc_err;
    } else
        ai_hi = a_p;
    ai_lo = pos_norm(a_start);
    return 0;
}
