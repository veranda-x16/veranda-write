/* Veranda Write: undo and redo, a log of the text's primitives in RAM
   banks of its own (a chunked buffer, cbuf.c).

   A record is [type][place: 3 bytes][n: 3][n bytes][its size: 3]: 'S' a
   step's start (the cursor; the payload the selection's other end and the
   kind), 'I' n bytes inserted (the bytes), 'D' n bytes deleted (the
   bytes), 'R' one byte replaced (the old, the new). The size at the end
   lets undo walk back. ul_pos is the end of what has been done: undo walks
   back from it to the step's 'S', redo forward to the next; a new step
   drops what was undone (the redo).

   Typing makes one step for a run of characters (U_TYPE) and adds each
   character to the run's 'I' record. The log holds UL_BANKS - 1 banks:
   when full, the oldest steps go; a step bigger than all of it cannot be
   undone (ul_lost) and empties the log.

   (Overlay 6 on the X16, with edit.c.)

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CW_HOST
#include "v/ovl6.h"
#endif
#include "cw.h"

static unsigned char ul_bank[UL_BANKS + 2];
static unsigned int ul_len[UL_BANKS + 2];
struct cbuf ul;
unsigned char ul_lost;
pos_t ul_cur, ul_anchor;

#define UL_CAP ((pos_t)(UL_BANKS - 1) * BANK_SIZE)
#define HDR 7
#define TRL 3

static pos_t ul_pos;                    /* the end of what is done */
static pos_t ul_cs;                     /* the step's 'S' */
static pos_t ul_last;                   /* the last record */
static unsigned char typing;            /* a typing step: its 'I' grows */
static unsigned char hd[HDR + 4];
static unsigned char ubuf[64];

static void put3(unsigned char *p, pos_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
}

static pos_t get3(const unsigned char *p)
{
    return p[0] | (unsigned int)p[1] << 8 | (pos_t)p[2] << 16;
}

void ul_init(void)
{
    cb_init(&ul, ul_bank, ul_len, UL_BANKS + 2);
    ul_reset();
}

void ul_reset(void)
{
    cb_clear(&ul);
    ul_pos = ul_cs = ul_last = 0;
    typing = 0;
}

/* the oldest step out, if it is not the one being made; 1 done */
static unsigned char drop_oldest(void)
{
    pos_t q, cut = 0;
    unsigned char t[HDR];
    q = 0;
    while (q < ul.total) {
        cb_read(&ul, q, t, HDR);
        if (t[0] == 'S' && q) {
            cut = q;
            break;
        }
        q += HDR + get3(t + 4) + TRL;
    }
    if (!cut || cut > ul_cs)
        return 0;
    cb_delete(&ul, 0, cut);
    ul_pos -= cut;
    ul_cs -= cut;
    ul_last -= cut;
    return 1;
}

/* room for k more bytes; 0: none, the log emptied (ul_lost) */
static unsigned char room(pos_t k)
{
    while (ul.total + k > UL_CAP)
        if (!drop_oldest()) {
            ul_reset();
            ul_lost = 1;
            return 0;
        }
    return 1;
}

static unsigned char app(const unsigned char *p, unsigned int n)
{
    if (cb_append(&ul, p, n, BANK_SIZE)) {
        ul_reset();
        ul_lost = 1;
        return 1;
    }
    return 0;
}

/* a record's header (the payload follows: the caller's) */
static unsigned char begin(unsigned char t, pos_t p, pos_t n)
{
    if (ul_lost || !room(HDR + n + TRL))
        return 1;
    ul_last = ul.total;
    hd[0] = t;
    put3(hd + 1, p);
    put3(hd + 4, n);
    return app(hd, HDR);
}

static void end(pos_t n)
{
    unsigned char t[3];
    put3(t, HDR + n + TRL);
    if (!app(t, TRL))
        ul_pos = ul.total;
}

void ul_step(unsigned char kind, pos_t cur, pos_t anchor)
{
    unsigned char t[4];
    if (ld_kind)
        ld_need(cur > anchor ? cur : anchor);   /* (opening: the rest read first if the edit is near its end) */
    if (kind == U_TYPE && typing && ul_pos == ul.total && !ul_lost)
        return;
    typing = kind == U_TYPE;
    ul_lost = 0;
    if (ul_pos < ul.total)
        cb_delete(&ul, ul_pos, ul.total - ul_pos);     /* the redo goes */
    ul_cs = ul.total;
    put3(t, anchor);
    t[3] = kind;
    if (!begin('S', cur, 4) && !app(t, 4))
        end(4);
}

void ul_break(void)
{
    typing = 0;
}

void ul_log(unsigned char t, pos_t p, const unsigned char *s, unsigned int n)
{
    unsigned char h[HDR];
    pos_t m;
    if (ul_lost || ul_pos != ul.total)
        return;
    if (typing && t == 'I' && ul_last > ul_cs && n < 64) {
        cb_read(&ul, ul_last, h, HDR);
        m = get3(h + 4);
        if (h[0] == 'I' && get3(h + 1) + m == p && room(n)) {
            cb_delete(&ul, ul.total - TRL, TRL);
            if (app(s, n))
                return;
            m += n;
            put3(h + 4, m);
            cb_write(&ul, ul_last, h, HDR);
            end(m);
            return;
        }
    }
    if (!begin(t, p, n) && !app(s, n))
        end(n);
}

void ul_log_del(pos_t p, pos_t n)
{
    pos_t k;
    unsigned int m;
    if (ul_lost || ul_pos != ul.total)
        return;
    if (begin('D', p, n))
        return;
    for (k = 0; k < n; k += m) {
        m = cb_read(&tx, p + k, ubuf, n - k > sizeof ubuf ? sizeof ubuf : (unsigned int)(n - k));
        if (!m) {                       /* (W41: past the text's end: never forever) */
            ul_reset();
            ul_lost = 1;
            return;
        }
        if (app(ubuf, m))
            return;
    }
    end(n);
}

void FASTCALL ul_log_set(pos_t p, unsigned char old, unsigned char c)
{
    unsigned char t[2];
    t[0] = old;
    t[1] = c;
    ul_log('R', p, t, 2);
}

unsigned char ul_can_undo(void)
{
    return ul_pos > 0;
}

unsigned char ul_can_redo(void)
{
    return ul_pos < ul.total;
}

/* the payload at q (n bytes) into the text at p */
static void payload_in(pos_t q, pos_t p, pos_t n)
{
    pos_t k;
    unsigned int m;
    for (k = 0; k < n; k += m) {
        m = cb_read(&ul, q + k, ubuf, n - k > sizeof ubuf ? sizeof ubuf : (unsigned int)(n - k));
        if (!m)
            return;                     /* (W41: a log cut short: never forever) */
        if (cb_insert(&tx, p + k, ubuf, m)) {
            doc_err = DE_MEM;
            return;
        }
    }
}

unsigned char ul_undo(void)
{
    unsigned char t[3];
    pos_t r, p, n;
    if (ld_kind)
        ld_need(0xFFFFFFFFUL);
    if (!ul_pos)
        return 0;
    typing = 0;
    for (;;) {
        cb_read(&ul, ul_pos - TRL, t, TRL);
        r = ul_pos - get3(t);
        cb_read(&ul, r, hd, HDR + 2);
        p = get3(hd + 1);
        n = get3(hd + 4);
        ul_pos = r;
        switch (hd[0]) {
        case 'S':
            ul_cur = p;
            cb_read(&ul, r + HDR, t, 3);
            ul_anchor = get3(t);
            lay_fix();
            doc_modified = 1;
            return 1;
        case 'I':
            cb_delete(&tx, p, n);
            lay_dirty(p, 0, n);
            break;
        case 'D':
            payload_in(r + HDR, p, n);
            lay_dirty(p, n, 0);
            break;
        case 'R':
            cb_write(&tx, p, hd + HDR, 1);
            lay_dirty(p, 1, 1);
            break;
        }
        if (!ul_pos) {                  /* (a log cut short: no 'S') */
            ul_cur = ul_anchor = p;
            lay_fix();
            return 1;
        }
    }
}

unsigned char ul_redo(void)
{
    pos_t p, n, r;
    unsigned char first = 1;
    if (ld_kind)
        ld_need(0xFFFFFFFFUL);
    if (ul_pos >= ul.total)
        return 0;
    typing = 0;
    ul_cur = 0;
    while (ul_pos < ul.total) {
        r = ul_pos;
        cb_read(&ul, r, hd, HDR + 2);
        if (hd[0] == 'S' && !first)
            break;
        first = 0;
        p = get3(hd + 1);
        n = get3(hd + 4);
        ul_pos = r + HDR + n + TRL;
        switch (hd[0]) {
        case 'S':
            ul_cur = p;
            break;
        case 'I':
            payload_in(r + HDR, p, n);
            lay_dirty(p, n, 0);
            ul_cur = p + n;
            break;
        case 'D':
            cb_delete(&tx, p, n);
            lay_dirty(p, 0, n);
            ul_cur = p;
            break;
        case 'R':
            cb_write(&tx, p, hd + HDR + 1, 1);
            lay_dirty(p, 1, 1);
            ul_cur = p;
            break;
        }
    }
    ul_anchor = ul_cur;
    lay_fix();
    doc_modified = 1;
    return 1;
}
