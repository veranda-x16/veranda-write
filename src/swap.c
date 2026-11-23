/* Veranda Write: the pager. The text and the line table are paged
   cbufs (cbuf.c): their chunks are in RAM banks while there are banks,
   and on the card when there are not (DECISIONS W29).

   The cache is every bank the pool can get (bank.c: runs from Veranda
   while it has them). When none is left, sw_take puts out the chunk used
   least recently (each use stamps its bank: res_i), the oldest one with a
   copy on the card first in a pass through the whole document (sw_scan:
   save, find, counting), and hands its bank over. A chunk with no copy
   on the card (sw_bd: new, or changed since it was read) is written to a
   slot of the swap first (swapwh.c, the platform's files); one whose bytes
   are the document file's as they are (opened, not changed since, or
   saved) is just dropped. sw_in reads a chunk back (from the swap, or
   from the document's file at its place) into a bank sw_take may have
   made. Banks an edit is using are pinned (sw_pin1, sw_pin2).

   Put aside (Alt+Tab), sw_aside puts chunks out until whole runs of the
   pool can go back to Veranda and the switcher has the banks it needs
   (W15, W21). After a save, sw_saved makes every chunk of the text a
   clean copy of the new file's.

   The tables of the paged cbufs are here (overlay 11's bank on the X16:
   cbuf.c's reads map it), the cards' side (where each chunk is on the
   card, the slots) is swapwh.c's (overlay 12).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CW_HOST
#include "v/ovl11.h"
#endif
#include <string.h>
#include "cbpriv.h"

unsigned char tx_bank[TX_MAX];
unsigned int tx_len[TX_MAX];
unsigned char lt_bank[LT_MAX];
unsigned int lt_len[LT_MAX];
unsigned int sw_stamp[256];
unsigned char sw_bd[256];
unsigned char sw_pin1, sw_pin2;

void sw_init(void)
{
    cb_initp(&tx, tx_bank, tx_len, PG_TX, TX_MAX);
    cb_initp(&lt, lt_bank, lt_len, PG_LT, LT_MAX);
}

void sw_age(void)
{
    static unsigned char b;
    b = 0;
    do
        sw_stamp[b] >>= 1;
    while (++b);
    sw_clk = 0x8000;
}

/* the paged cbuf pg as it is now (W while an edit works on it) */
static struct cbuf *live(unsigned char pg)
{
    struct cbuf *c = pg == PG_TX ? &tx : &lt;
    return cb_cur == c ? &W : c;
}

/* the victim: the chunk to put out (vc, vi, vb); 0 none */
static unsigned char take_lo;           /* (sw_take_lo: a bank of the first runs) */
static struct cbuf *vc;
static unsigned int vi;
static unsigned char vb;

static unsigned char choose(unsigned char clean_only)
{
    static struct cbuf *c, *cc, *dc;
    static unsigned int i, n, ci_, di, cst, dst, s;
    static unsigned char pg, b, cb_, db;
    cb_ = db = 0;
    cst = dst = 0xFFFF;
    for (pg = PG_TX; pg <= PG_LT; ++pg) {
        c = live(pg);
        n = c->n;
        for (i = 0; i < n; ++i) {
            b = c->bank[i];
            if (!b || b == sw_pin1 || b == sw_pin2 || (take_lo && !bk_inlo(b)))
                continue;
            s = sw_stamp[b];
            if (sw_bd[b]) {
                if (s < dst || !db) {
                    dst = s;
                    db = b;
                    dc = c;
                    di = i;
                }
            } else if (s < cst || !cb_) {
                cst = s;
                cb_ = b;
                cc = c;
                ci_ = i;
            }
        }
    }
    if (cb_ && (clean_only || sw_scan || !db || cst <= dst)) {
        vc = cc;
        vi = ci_;
        vb = cb_;
        return 1;
    }
    if (!db || clean_only)
        return 0;
    vc = dc;
    vi = di;
    vb = db;
    return 1;
}

/* the victim out: its bytes to the card if they are not there; 0 done */
static unsigned char put_out(void)
{
    if (sw_bd[vb]) {
        if (sw_out(vc->pg, vi, vb, vc->len[vi])) {
            sw_err = SE_WRITE;
            return 1;
        }
        sw_bd[vb] = 0;
        ++sw_writes;
    }
    vc->bank[vi] = 0;
    if (vb == sw_last)
        sw_last = 0;
    return 0;
}

unsigned char sw_take_lo(void)
{
    unsigned char b;
    take_lo = 1;
    b = sw_take();
    take_lo = 0;
    return b;
}

unsigned char sw_take(void)
{
    static unsigned char co;
    co = 0;
    for (;;) {
        if (!choose(co)) {
            /* (none clean: the card would not take one; asked whether to
               try again: the card made room, put back) */
            if (co && plat_sw_fail(SE_WRITE)) {
                co = 0;
                continue;
            }
            return 0;
        }
        if (!put_out())
            return vb;
        co = 1;                         /* (the card would not take it: only clean ones now) */
    }
}

#ifdef DBG
void sw_check(const char *w);
#endif
void sw_in(void)
{
    static unsigned int i;
    static unsigned char b;
    i = sw_ci;
#ifdef DBG
    if (W.bank == tx_bank) sw_check("in<");
#endif
    b = bk_get();
    if (!b) {
        ul_reset();                     /* (the last resort: the undo log's banks) */
        bk_pg = 1;
        b = bk_get();
        bk_pg = 0;
    }
    while (!b) {
        plat_sw_fail(SE_WRITE);         /* (no bank at all: the card must take a chunk) */
        bk_pg = 1;
        b = bk_get();
        bk_pg = 0;
    }
    while (sw_rd(W.pg, i, b, L[i])) {
        sw_err = SE_READ;
        plat_sw_fail(SE_READ);
    }
    ++sw_reads;
    B[i] = b;
    sw_bd[b] = 0;
    sw_stamp[b] = sw_clk;
}

void sw_saved(pos_t off)
{
    static unsigned int i;
    static unsigned char b;
    for (i = 0; i < tx.n; ++i) {
        wh_set(PG_TX, i, off);
        off += tx_len[i];
        b = tx_bank[i];
        if (b)
            sw_bd[b] = 0;
    }
}

/* put aside: whole runs of the pool out to the card (the cheapest first:
   the fewest chunks without a copy there) until Veranda has sw_want banks
   free; iobuf: which banks are paged chunks' */
void sw_aside(void)
{
    static unsigned char r, k, b, best, cost, bc, ok, any, pg;
    static unsigned int i;
    static struct cbuf *c;
    bk_release();
    while (plat_free() < sw_want) {
        memset(iobuf, 0, 256);
        for (pg = PG_TX; pg <= PG_LT; ++pg) {
            c = live(pg);
            for (i = 0; i < c->n; ++i)
                iobuf[c->bank[i]] = 1;
        }
        iobuf[0] = 0;
        best = 0xFF;
        bc = 0xFF;
        for (r = 0; r < NRUN; ++r) {
            if (!run_n[r])
                continue;
            ok = 1;
            any = cost = 0;
            for (k = 0; k < run_n[r]; ++k) {
                b = run_b[r] + k;
                if (bk_isfree(b))
                    continue;
                if (!iobuf[b]) {
                    ok = 0;
                    break;
                }
                any = 1;
                cost += sw_bd[b];
            }
            if (ok && any && cost < bc) {
                bc = cost;
                best = r;
            }
        }
        if (best == 0xFF)
            break;
        for (k = 0; k < run_n[best]; ++k) {
            b = run_b[best] + k;
            if (!iobuf[b])
                continue;
            for (pg = PG_TX; pg <= PG_LT; ++pg) {
                c = live(pg);
                for (i = 0; i < c->n; ++i)
                    if (c->bank[i] == b) {
                        vc = c;
                        vi = i;
                        vb = b;
                        if (put_out())
                            return;
                        bk_put(b);
                        goto next;
                    }
            }
next:       ;
        }
        bk_release();
    }
    if (plat_free() < sw_want)
        sw_ulout();                     /* (not enough yet: the undo log's banks too) */
}
