/* Veranda Write: a byte stream in RAM banks, one chunk a bank: the
   reads (main RAM on the X16; the edits are cbufx.c's).

   Each chunk is the first len bytes at $A000 of its bank, in order; the
   stream is the chunks one after the other. An insertion moves at most
   one bank's bytes (a memmove in that bank) and, when they do not fit,
   splits the chunk (the bytes after the place go to a new bank); a
   deletion moves at most one bank's bytes too and merges neighbours that
   have become small. So no edit is slower for being far from the last
   one or in a big document (DECISIONS W2).

   The cache (ci, cs) is the chunk last used and where it starts: reading
   on from there costs nothing, and the chunks are walked from it.

   A paged cbuf (the text, the line table: W.pg) may have chunks that are
   only on the card (bank 0): res_i reads one back when it is used
   (swap.c), and each use stamps its bank for the pager's choice of what
   goes out next (the least recently used; DECISIONS W29). Their tables
   are in overlay 11's bank (sw_tbank), mapped while they are read.

   The code maps banks at $A000: it must be in main RAM, and every buffer
   it is given must be below $A000. It puts the caller's bank back.

   (cc65: each call copies the cbuf into W and works on W and static
   variables: absolute addresses instead of pointers and the C stack, a
   third of the code, and faster.)

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cbpriv.h"

struct cbuf W;                          /* the one being worked on */
unsigned int ci;                        /* W.ci and W.cs while working */
pos_t cs;
unsigned int off;                       /* the place's offset in chunk ci */
struct cbuf *cb_cur;
pos_t cb_src = WH_NONE;
unsigned char sw_tbank, sw_last, sw_scan, sw_err;
unsigned int sw_clk, sw_ci, sw_reads, sw_writes;
unsigned char sw_want = 32;             /* (main RAM: the pager's state others read) */

#ifdef CW_HOST
/* (the X16's are v/cbread.s: locate, res_i, cb_read, cb_byte) */
static void load(struct cbuf *c)
{
    memcpy(&W, c, sizeof W);
    ci = W.ci;
    cs = W.cs;
}

void FASTCALL locate(pos_t at)
{
    if (ci >= W.n) {
        ci = 0;
        cs = 0;
    }
    while (at < cs)
        cs -= L[--ci];
    while (ci + 1 < W.n && at >= cs + L[ci])
        cs += L[ci++];
    off = (unsigned int)(at - cs);
}

/* (the tables mapped: the pager's call maps them back after) */
unsigned char FASTCALL res_i(unsigned int i)
{
    static unsigned char b;
    b = B[i];
    if (!W.pg)
        return b;
    if (!b) {
        sw_ci = i;
        sw_in();
        b = B[i];
    }
    if (b != sw_last) {
        sw_last = b;
        if (!++sw_clk)
            sw_age();
        sw_stamp[b] = sw_scan ? 0 : sw_clk;
    }
    return b;
}

unsigned int cb_read(struct cbuf *c, pos_t at, unsigned char *dst, unsigned int n)
{
    static unsigned int k, done;
    static unsigned char b;
    if (at >= c->total)
        return 0;
    load(c);
    if (n > W.total - at)
        n = (unsigned int)(W.total - at);
    done = 0;
    {
        BANK_SAVE();
        while (n) {
            TB_MAP();
            locate(at);
            k = L[ci] - off;
            b = res();
            if (k > n)
                k = n;
            BANK_MAP(b);
            memcpy(dst, BANK_P + off, k);
            dst += k;
            at += k;
            n -= k;
            done += k;
        }
        BANK_RESTORE();
    }
    c->ci = ci;
    c->cs = cs;
    return done;
}

unsigned char cb_byte(struct cbuf *c, pos_t at)
{
    unsigned char b;
    BANK_SAVE();
    if (at >= c->total)
        return 0;
    load(c);
    TB_MAP();
    locate(at);
    b = res();
    BANK_MAP(b);
    b = BANK_P[off];
    BANK_RESTORE();
    c->ci = ci;
    c->cs = cs;
    return b;
}

#endif

/*---------------------------------------------------------------
  a cbuf made, emptied, its room (main RAM: overlay 11, cbufx.c's, has
  no room left; W43)
 ---------------------------------------------------------------*/
void cb_initp(struct cbuf *c, unsigned char *bank, unsigned int *len, unsigned char pg, unsigned int max)
{
    memset(c, 0, sizeof *c);
    c->bank = bank;
    c->len = len;
    c->max = max;
    c->pg = pg;
    if (pg)
        wh_clear(pg);
}

void cb_init(struct cbuf *c, unsigned char *bank, unsigned int *len, unsigned int max)
{
    cb_initp(c, bank, len, 0, max);
}

unsigned char cb_room(struct cbuf *c, pos_t n)
{
    return c->n + (unsigned int)(n / BANK_SIZE) + 2 <= c->max;
}

void cb_clear(struct cbuf *c)
{
    BANK_SAVE();
    TB_MAP();                           /* (the paged cbufs' tables: overlay 11's bank) */
    while (c->n)
        bk_put(c->bank[--c->n]);        /* (bank 0, a chunk on the card: nothing) */
    BANK_RESTORE();
    if (c->pg)
        wh_clear(c->pg);
    c->ci = 0;
    c->cs = 0;
    c->total = 0;
}

unsigned char cb_split;                 /* (cw.h: the typing; cbufx.c's cb_insert) */

/*---------------------------------------------------------------
  bytes in banks, for cbufx.c
 ---------------------------------------------------------------*/
void bm_move(unsigned char b, unsigned int to, unsigned int from, unsigned int n)
{
    BANK_SAVE();
    BANK_MAP(b);
    memmove(BANK_P + to, BANK_P + from, n);
    BANK_RESTORE();
}

/* n bytes from bank sb at offset so to bank db at dof (two banks), through
   a page in main RAM: memcpy both ways is about twice as fast as the
   kernel's BankCopy, which maps the banks for every byte */
static unsigned char bounce[256];

void bm_copy(unsigned char sb, unsigned int so, unsigned char db, unsigned int dof, unsigned int n)
{
    static unsigned int k;
    BANK_SAVE();
    while (n) {
        k = n > sizeof bounce ? sizeof bounce : n;
        BANK_MAP(sb);
        memcpy(bounce, BANK_P + so, k);
        BANK_MAP(db);
        memcpy(BANK_P + dof, bounce, k);
        so += k;
        dof += k;
        n -= k;
    }
    BANK_RESTORE();
}

void bm_in(unsigned char b, unsigned int o, const unsigned char *src, unsigned int n)
{
    BANK_SAVE();
    BANK_MAP(b);
    memcpy(BANK_P + o, src, n);
    BANK_RESTORE();
}
