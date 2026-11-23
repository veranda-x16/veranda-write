/* Veranda Write: the chunked buffer's edits (cbuf.c says how it works;
   overlay 11 on the X16, with the paged cbufs' tables and the pager).

   On a paged cbuf an edit first brings the chunks it changes into RAM
   (res_i) and pins their banks (sw_pin1, sw_pin2) so that taking another
   bank (which may put a chunk out to the card) leaves them; a chunk it
   changes loses its copy on the card (dirty: sw_bd, wh_dirty); a chunk
   deleted whole is dropped without being read back. Every bank is taken
   before anything changes, so a failure (no bank: the card full) leaves
   the stream as it was.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CW_HOST
#include "v/ovl11.h"
#endif
#include <string.h>
#include "cbpriv.h"

static void load(struct cbuf *c)
{
    memcpy(&W, c, sizeof W);
    ci = W.ci;
    cs = W.cs;
    cb_cur = c;
    bk_pg = W.pg != 0;                  /* (bank.c: its banks from the later runs) */
}

#ifdef DBG
void sw_check(const char *w);
#endif
static void store(struct cbuf *c)
{
    W.ci = ci;
    W.cs = cs;
    memcpy(c, &W, sizeof W);
    cb_cur = 0;
    bk_pg = 0;
    sw_pin1 = sw_pin2 = 0;
#ifdef DBG
    if (c == &tx)
        sw_check("store");
#endif
}

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

void cb_clear(struct cbuf *c)
{
    while (c->n)
        bk_put(c->bank[--c->n]);        /* (bank 0, a chunk on the card: nothing) */
    if (c->pg)
        wh_clear(c->pg);
    c->ci = 0;
    c->cs = 0;
    c->total = 0;
}

/* chunk i changed: its copy on the card (if any) no longer it */
static void dirty(unsigned int i)
{
    static unsigned char b;
    if (W.pg) {
        b = B[i];
        if (!sw_bd[b]) {
            sw_bd[b] = 1;
            wh_dirty(W.pg, i);
        }
    }
}

/* a new chunk at index i (bank b, empty: dirty) */
static void add_chunk(unsigned int i, unsigned char b)
{
    unsigned int k = W.n;
    memmove(B + i + 1, B + i, k - i);
    memmove(L + i + 1, L + i, sizeof *L * (k - i));
    B[i] = b;
    L[i] = 0;
    ++W.n;
    if (W.pg) {
        wh_ins(W.pg, i, k - i);
        sw_bd[b] = 1;
    }
}

static void drop_chunk(unsigned int i)
{
    bk_put(B[i]);
    --W.n;
    memmove(B + i, B + i + 1, W.n - i);
    memmove(L + i, L + i + 1, sizeof *L * (W.n - i));
    if (W.pg)
        wh_del(W.pg, i, W.n - i);
}

unsigned char cb_room(struct cbuf *c, pos_t n)
{
    return c->n + (unsigned int)(n / BANK_SIZE) + 2 <= c->max;
}

unsigned char cb_insert(struct cbuf *c, pos_t at, const unsigned char *src, unsigned int n)
{
    static unsigned char need, k, b, got[10];
    static unsigned int l, tail, room, cur;
    if (!n)
        return 0;
    load(c);
    if (!W.n) {
        if (!W.max || !(k = bk_get())) {
            store(c);
            return 1;
        }
        add_chunk(0, k);
        ci = 0;
        cs = 0;
    }
    locate(at);
    sw_pin1 = b = res();
    l = L[ci];
    if (l + n <= BANK_SIZE) {
        bm_move(b, off + n, off, l - off);
        bm_in(b, off, src, n);
        L[ci] = l + n;
        dirty(ci);
        W.total += n;
        store(c);
        return 0;
    }
    /* the banks it needs, taken first so that nothing changes on failure:
       one for the bytes after the place, and as many as the new bytes
       need past the room left in this one */
    tail = l - off;
    room = BANK_SIZE - off;
    need = tail ? 1 : 0;
    if (n > room)
        need += (unsigned char)((n - room + BANK_SIZE - 1) / BANK_SIZE);
    if (need > sizeof got || W.n + need > W.max) {
        store(c);
        return 1;
    }
    for (k = 0; k < need; ++k)
        if (!(got[k] = bk_get())) {
            while (k)
                bk_put(got[--k]);
            store(c);
            return 1;
        }
    k = 0;
    dirty(ci);
    if (tail) {
        add_chunk(ci + 1, got[k++]);
        bm_copy(B[ci], off, B[ci + 1], 0, tail);
        L[ci + 1] = tail;
        L[ci] = off;
    }
    W.total += n;
    cur = ci;
    while (n) {
        room = BANK_SIZE - L[cur];
        if (!room) {
            add_chunk(cur + 1, got[k++]);
            ++cur;
            continue;
        }
        if (room > n)
            room = n;
        bm_in(B[cur], L[cur], src, room);
        L[cur] += room;
        src += room;
        n -= room;
    }
    while (k < need)
        bk_put(got[k++]);
    store(c);
    return 0;
}

/* chunks i and i + 1 into one when they fit half a bank */
static void merge(unsigned int i)
{
    static unsigned char b0, b1;
    if (i + 1 >= W.n || L[i] + L[i + 1] > BANK_SIZE / 2)
        return;
    sw_pin1 = b0 = res_i(i);
    if (L[i + 1]) {
        sw_pin2 = b1 = res_i(i + 1);
        bm_copy(b1, 0, b0, L[i], L[i + 1]);
    }
    L[i] += L[i + 1];
    dirty(i);
    drop_chunk(i + 1);
}

void cb_delete(struct cbuf *c, pos_t at, pos_t n)
{
    static unsigned int l, k;
    static unsigned char b;
    if (at >= c->total)
        return;
    load(c);
    if (n > W.total - at)
        n = W.total - at;
    while (n) {
        locate(at);
        l = L[ci];
        k = l - off;
        if (k > n)
            k = (unsigned int)n;
        if (k == l) {
            drop_chunk(ci);             /* (the cache: the next chunk, same start) */
        } else {
            b = res();
            bm_move(b, off, off + k, l - off - k);
            L[ci] = l - k;
            dirty(ci);
        }
        W.total -= k;
        n -= k;
    }
    if (W.n) {
        /* small neighbours merged around the place */
        if (ci >= W.n) {
            ci = 0;
            cs = 0;
            merge(W.n - 1);
        } else if (ci > 0 && L[ci - 1] + L[ci] <= BANK_SIZE / 2) {
            merge(ci - 1);
            ci = 0;
            cs = 0;
        } else
            merge(ci);
    } else {
        ci = 0;
        cs = 0;
    }
    store(c);
}

void cb_write(struct cbuf *c, pos_t at, const unsigned char *src, unsigned int n)
{
    static unsigned int k;
    static unsigned char b;
    load(c);
    while (n && at < W.total) {
        locate(at);
        k = L[ci] - off;
        if (k > n)
            k = n;
        b = res();
        bm_in(b, off, src, k);
        dirty(ci);
        src += k;
        at += k;
        n -= k;
    }
    store(c);
}

unsigned char cb_append(struct cbuf *c, const unsigned char *src, unsigned int n, unsigned int fill)
{
    static unsigned int i, k;
    static unsigned char b, e;
    load(c);
    e = 0;
    while (n) {
        i = W.n - 1;
        if (!W.n || L[i] >= fill) {
            if (W.n >= W.max || !(b = bk_get())) {
                e = 1;
                break;
            }
            add_chunk(W.n, b);
            i = W.n - 1;
        }
        sw_pin1 = b = res_i(i);
        k = fill - L[i];
        if (k > n)
            k = n;
        /* a chunk the file's bytes fill as they are needs no copy in the swap */
        if (W.pg && !(sw_bd[b] && L[i]) && wh_app(W.pg, i, L[i]))
            sw_bd[b] = 0;
        else
            dirty(i);
        bm_in(b, L[i], src, k);
        L[i] += k;
        W.total += k;
        src += k;
        n -= k;
        if (cb_src != WH_NONE)
            cb_src += k;
    }
    cb_src = WH_NONE;
    store(c);
    return e;
}
