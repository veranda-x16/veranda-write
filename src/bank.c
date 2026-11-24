/* Veranda Write: the pool of RAM banks.

   Veranda's BankAlloc gives runs of banks and keeps 24 runs for every
   program together (Veranda's DECISIONS V57), so banks are taken in runs
   (the first BK_LO of 1 bank, then of 2 to 8 banks, of 4, from 32 of 8,
   from 64 of 16: else half as many) and handed out one at a time, from the run with the
   fewest free (so that runs empty whole); a bank given back stays in the
   pool for the next chunk. bk_keep banks are left to Veranda (desk
   accessories, the kernel). When the pool has none and Veranda none to
   give, a chunk of the text or the line table is put out to the card
   (swap.c's sw_take) and its bank is the one handed out. bk_release gives
   back to the system every run whose banks are all in the pool (before
   the switcher puts the program aside: it counts the pool's banks as the
   program's, W15). (Veranda frees them all when the program quits.)

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include "cw.h"

static unsigned char pool[32];          /* bit b: bank b free in the pool */
static unsigned char nfree;
unsigned char bk_owned;
unsigned char run_b[NRUN], run_n[NRUN];   /* the runs taken: first bank, banks */
unsigned char bk_keep;
static const unsigned char bit[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

unsigned char bk_free_n(void)
{
    return nfree;
}

/* bk_get would put nothing out to the card for a paged chunk: a bank in
   the pool, or a run to take one from (W43: cbufx.c's split) */
unsigned char bk_cheap(void)
{
    unsigned char i;
    if (nfree)
        return 1;
    for (i = 0; i < NRUN && run_n[i]; ++i)
        ;
    return i < NRUN && (!bk_keep || plat_free() > bk_keep);
}

void FASTCALL bk_put(unsigned char b)
{
    if (!b)
        return;
    pool[b >> 3] |= bit[b & 7];
    ++nfree;
}

unsigned char FASTCALL bk_isfree(unsigned char b)
{
    return (pool[b >> 3] & bit[b & 7]) != 0;
}

/* run i's banks free in the pool */
static unsigned char free_in(unsigned char i)
{
    unsigned char k, b, f = 0;
    for (k = 0; k < run_n[i]; ++k) {
        b = run_b[i] + k;
        if (pool[b >> 3] & bit[b & 7])
            ++f;
    }
    return f;
}

/* the free bank of the run with the fewest free among runs lo .. hi - 1
   (so that runs empty whole, for bk_release); 0 none */
static unsigned char pick(unsigned char lo, unsigned char hi)
{
    unsigned char i, j = 0xFF, n = 0, k, b;
    for (i = lo; i < hi; ++i) {
        if (!run_n[i])
            continue;
        k = free_in(i);
        if (k && k < j) {
            j = k;
            n = i;
        }
    }
    if (j == 0xFF)
        return 0;
    for (k = 0; k < run_n[n]; ++k) {
        b = run_b[n] + k;
        if (pool[b >> 3] & bit[b & 7]) {
            pool[b >> 3] &= ~bit[b & 7];
            --nfree;
            return b;
        }
    }
    return 0;
}

unsigned char FASTCALL bk_inlo(unsigned char b)
{
    unsigned char i;
    for (i = 0; i < BK_LO; ++i)
        if (run_n[i] && b >= run_b[i] && b < run_b[i] + run_n[i])
            return 1;
    return 0;
}

/* a bank for a paged chunk (bk_pg) from the later runs, one for anything
   else (the undo log, the clipboard, the fonts: never put out) from the
   first BK_LO runs (a paged chunk put out of one of them if need be), so
   that the later runs hold only chunks the pager can put out: whole runs
   go back to Veranda when the program is put aside (swap.c, W35) */
unsigned char bk_pg;

unsigned char bk_get(void)
{
    unsigned char i, b, n, k;
    if (bk_pg) {
        if ((b = pick(BK_LO, NRUN)) != 0)
            return b;
    } else {
        if ((b = pick(0, BK_LO)) != 0)
            return b;
        for (i = 0; i < BK_LO && run_n[i]; ++i)
            ;
        if (i < BK_LO && (b = plat_bank_run(1)) != 0) {
            run_b[i] = b;               /* (a first run given back: one more) */
            run_n[i] = 1;
            ++bk_owned;
            return b;
        }
        if (run_n[BK_LO - 1] && (b = sw_take_lo()) != 0)
            return b;
    }
    if (!nfree) {
        for (i = 0; i < NRUN && run_n[i]; ++i)
            ;
        if (i == NRUN)
            return sw_take();
        n = i < BK_LO ? 1 : bk_owned < 8 ? 2 : bk_owned < 32 ? 4 : bk_owned < 64 ? 8 : 16;
        k = bk_keep ? plat_free() : 0xFF;
        k = k > bk_keep ? k - bk_keep : 0;
        for (; n > k; n >>= 1)
            ;
        for (; n; n >>= 1) {
            b = plat_bank_run(n);
            if (b)
                break;
        }
        if (!n)
            return sw_take();
        run_b[i] = b;
        run_n[i] = n;
        bk_owned += n;
        while (n--)
            bk_put(b + n);
    }
    return pick(0, NRUN);
}

/* the runs whose banks are all in the pool: back to the system */
void bk_release(void)
{
    unsigned char i, k, b;
    for (i = 0; i < NRUN; ++i) {
        if (!run_n[i] || free_in(i) < run_n[i])
            continue;
        for (k = 0; k < run_n[i]; ++k) {
            b = run_b[i] + k;
            pool[b >> 3] &= ~bit[b & 7];
        }
        nfree -= run_n[i];
        bk_owned -= run_n[i];
        plat_bank_free(run_b[i]);
        run_n[i] = 0;
    }
}
