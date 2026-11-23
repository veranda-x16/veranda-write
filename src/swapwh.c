/* Veranda Write: the pager's card side: where each paged chunk's bytes
   are on the card (wh: the document's file at an offset, a slot of the
   swap, or WH_NONE: nowhere, the bytes only in RAM), the swap's slots
   (one file a slot: the platform's plat_sw_write and plat_sw_read; the
   text's slots 0 .. TX_MAX - 1, the line table's after them, so that
   clearing one frees only its own). Overlay 12 on the X16 (swap.c and
   cbufx.c, in overlay 11, call it through glue.s).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CW_HOST
#include "v/ovl12.h"
#endif
#include <string.h>
#include "cbpriv.h"

static unsigned long tx_wh[TX_MAX], lt_wh[LT_MAX];
static unsigned char slot[(TX_MAX + LT_MAX + 7) / 8];
static const unsigned char bit[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
#define T(pg) ((pg) == PG_TX ? tx_wh : lt_wh)

static void sfree(unsigned long w)
{
    static unsigned int n;
    if (!WH_DIRTY(w) && (w & WH_SWAP)) {
        n = (unsigned int)(w & 0xFFFF);
        slot[n >> 3] &= ~bit[n & 7];
    }
}

void FASTCALL wh_dirty(unsigned char pg, unsigned int i)
{
    unsigned long *t = T(pg) + i;
    sfree(*t);
    *t = WH_NONE;
}

void wh_ins(unsigned char pg, unsigned int i, unsigned int n)
{
    unsigned long *t = T(pg) + i;
    memmove(t + 1, t, n * sizeof *t);
    *t = WH_NONE;
}

void wh_del(unsigned char pg, unsigned int i, unsigned int n)
{
    unsigned long *t = T(pg) + i;
    sfree(*t);
    memmove(t, t + 1, n * sizeof *t);
}

unsigned char wh_app(unsigned char pg, unsigned int i, unsigned int l)
{
    unsigned long *t = T(pg) + i;
    if (cb_src == WH_NONE)
        return 0;
    if (!l) {
        sfree(*t);
        *t = cb_src;
        return 1;
    }
    return !(*t & WH_SWAP) && *t + l == cb_src;
}

void FASTCALL wh_clear(unsigned char pg)
{
    if (pg == PG_TX) {
        memset(tx_wh, 0xFF, sizeof tx_wh);
        memset(slot, 0, TX_MAX / 8);
    } else {
        memset(lt_wh, 0xFF, sizeof lt_wh);
        memset(slot + TX_MAX / 8, 0, sizeof slot - TX_MAX / 8);
    }
}

void wh_set(unsigned char pg, unsigned int i, pos_t v)
{
    unsigned long *t = T(pg) + i;
    sfree(*t);
    *t = v;
}

unsigned char sw_out(unsigned char pg, unsigned int i, unsigned char b, unsigned int n)
{
    static unsigned int s, e;
    unsigned long *t = T(pg) + i;
    sfree(*t);
    *t = WH_NONE;
    s = pg == PG_TX ? 0 : TX_MAX;
    e = pg == PG_TX ? TX_MAX : TX_MAX + LT_MAX;
    while (slot[s >> 3] & bit[s & 7])
        if (++s == e)
            return 1;                   /* (cannot be: a slot a chunk) */
    if (plat_sw_write(s, b, n))
        return 1;
    slot[s >> 3] |= bit[s & 7];
    *t = WH_SWAP | s;
    return 0;
}

/* the undo log's banks to the swap's slots after the chunks' (put aside,
   when the chunks out were not enough: W35), and back (sw_ulin: the main
   loop's first pass, or a key, after coming back); the log's place in its
   cbuf kept, its banks 0 meanwhile */
#define UL_SLOT (TX_MAX + LT_MAX)
static unsigned char ul_out;

void sw_ulout(void)
{
    static unsigned int i;
    static unsigned char b;
    for (i = 0; i < ul.n; ++i) {
        b = ul.bank[i];
        if (!b)
            continue;
        if (plat_sw_write(UL_SLOT + i, b, ul.len[i]))
            return;                     /* (the card took nothing: the rest stay) */
        bk_put(b);
        ul.bank[i] = 0;
        ul_out = 1;
    }
    bk_release();
}

void sw_ulin(void)
{
    static unsigned int i;
    static unsigned char b;
    if (!ul_out)
        return;
    ul_out = 0;
    for (i = 0; i < ul.n; ++i) {
        if (ul.bank[i])
            continue;
        b = bk_get();
        while (!b || plat_sw_read(UL_SLOT + i, b, ul.len[i])) {
            if (b)
                plat_sw_fail(SE_READ);
            else {
                ul_reset();             /* (no bank at all: the log lost) */
                return;
            }
        }
        ul.bank[i] = b;
    }
}

unsigned char sw_rd(unsigned char pg, unsigned int i, unsigned char b, unsigned int n)
{
    unsigned long w = T(pg)[i];
    if (WH_DIRTY(w))
        return 1;                       /* (cannot be: a chunk out is on the card) */
    if (w & WH_SWAP)
        return plat_sw_read((unsigned int)(w & 0xFFFF), b, n);
    return plat_doc_read(w, b, n);
}
#ifdef DBG
#include <stdio.h>
#include <stdlib.h>
void sw_check(const char *w)
{
    unsigned int i;
    for (i = 0; i < tx.n; ++i) {
        unsigned char b = tx.bank[i];
        if ((!b && WH_DIRTY(tx_wh[i])) || (b && sw_bd[b] != WH_DIRTY(tx_wh[i]))) {
            printf("CHECK %s: chunk %u bank %u bd %u wh %08lX\n", w, i, b, b ? sw_bd[b] : 9, tx_wh[i]);
            abort();
        }
    }
}
#endif
