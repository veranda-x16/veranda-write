/* Veranda Write: a paragraph's start and the next one's in a table's
   row (styles.c's par_start and par_next hand them here: a row is one
   line of many paragraphs; overlay 29 on the X16, with v/tbcmd.c).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CW_HOST
#define par_start e_par_start           /* (through glue.s) */
#define par_next e_par_next
#endif
#include "cw.h"
#ifndef CW_HOST
#pragma code-name ("OVERLAY29")
#pragma rodata-name ("OVL29RO")
#pragma bss-name ("OVL29BSS")
#endif

/* (par_start, par_next for a place tb_p in a row: its paragraph's
   start, the next one's, by the CRs; walking back, a code is passed
   over whole by its last byte) */
void tb_pstart(void)
{
    static pos_t q;
    static unsigned char c;
    lt_find(tb_p);
    q = tb_p;
    while (q > lf_pos) {
        c = cb_byte(&tx, q - 1);
        if (IS_KIND(c))
            q -= 3;
        else if (c == CH_CR)
            break;
        else
            --q;
    }
    tb_p = q;
}

void tb_pnext(void)
{
    static pos_t q, e;
    static unsigned char c;
    static struct line l;
    lt_find(tb_p);
    lt_get(lf_i, &l);
    e = lf_pos + l.len;
    for (q = tb_p; q < e;) {
        c = cb_byte(&tx, q);
        if (IS_KIND(c))
            q += 3;
        else if (++q, c == CH_CR)
            break;
    }
    tb_p = q;
}

/* the paragraphs, through the line table: a paragraph's first line has
   LF_FIRST */
pos_t FASTCALL par_start(pos_t p)
{
    struct line l;
    unsigned int i;
    lt_find(p);
    i = lf_i;
    lt_get(i, &l);
    if (IS_TR(&l)) {
        tb_p = p;                       /* (1.6: a table's row: its cells' paragraphs) */
        tb_pstart();
        return tb_p;
    }
    for (;;) {
        lt_get(i, &l);
        if ((l.f & LF_FIRST) || !i)
            break;
        --i;
    }
    return lt_start(i);
}

pos_t FASTCALL par_next(pos_t p)
{
    struct line l;
    unsigned int i;
    lt_find(p);
    for (i = lf_i; i < lt_n; ++i) {
        lt_get(i, &l);
        if (IS_TR(&l) && i == lf_i) {
            tb_p = p;
            tb_pnext();
            return tb_p;
        }
        if (l.f & LF_PARA)
            return i + 1 < lt_n ? lt_start(i + 1) : 0xFFFFFFFFUL;
    }
    return 0xFFFFFFFFUL;
}

