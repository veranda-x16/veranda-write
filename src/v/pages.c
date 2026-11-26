/* Veranda Write on Veranda: the pages (overlay 13, with styles.c): the page and its
   line of any line, and how many pages there are, for the status bar,
   Word count, Go to page and the view's bands between the pages. A page
   holds pg_th pixels of text (the paper's height less its top and
   bottom margins: page.c, W44): a line starts the next page when it does
   not fit on this one, and the line after a page break's (LF_PB, W45)
   always does; a page break's line itself is never moved to the next
   page for its height (it would leave an empty page). The PDF's pages
   (pdf.c) follow the same rule.

   Counting means walking the line table, which a big document keeps on
   the card (DECISIONS W29): so the walk is done once and remembered. The
   walk (fr) has counted every line before fr.i; every gap pages it keeps
   a page start (pe_l, pe_p: a line, its page; gap 1, doubled when they
   are PE_MAX: a 1,000-page document 8). A line before fr.i is found from
   the page start before it (at most gap pages walked);
   one after it moves the walk on (pg_idle moves it on too, in the main
   loop's idle time: a document just opened is counted while it is read).

   An edit (the layout's lay_fix: pq_ in layout.c) changes the lines from
   L on: the walk goes back to the page start before L, and the page
   starts after the edit's lines are kept moved by its lines more or
   less, as guesses (from pe index vt on), with where the walk had got to
   (ff). When the walk, past every edit (vhi), starts a page on a line a
   guess has, the guesses after it are right but for the number of pages
   more or less: they are corrected, and the walk jumps to ff. So typing
   in a long document walks a page or two again, not the whole of it.
   Until the walk reaches the end, the count is an estimate ("~").

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifdef CW_HOST
#include "../cw.h"                      /* (the unit tests: tests/test_main.c) */
extern unsigned int pg_arg, pg_pc, pg_lc, pg_tot, pg_line, pv_pg;
extern unsigned char pg_est, pg_buf[], pv_st[];
#define VMAX 40
#else
#include "cwv.h"
#include "../spell.h"                    /* (sp_kj: the last key, W73) */
#pragma code-name ("OVERLAY13")
#pragma rodata-name ("OVL13RO")
#pragma bss-name ("OVL13BSS")
#endif
#define pg_at    e_pg_at
#define pg_idle  e_pg_idle
#define pg_first e_pg_first
#define pg_view  e_pg_view

#define PAGE_H pg_th
#define PE_MAX 224                      /* (240 until W45: overlay 13 had no room for the view's pages) */
#define PBB(e) ((e)[1] & 0x80)          /* an entry's LF_PB */

struct wk { unsigned int i, page, acc, ln; };   /* at line i: its page, the heights before it there, its line there */
static unsigned int pe_l[PE_MAX], pe_p[PE_MAX];
static unsigned char pe_n, vt;          /* page starts kept; the guesses from vt on */
static struct wk fr, ff, t;             /* the walk; where it was before the edits (ff_on); a walk of the moment */
static unsigned char fr_end, ff_on, ff_end;
static unsigned int vhi;                /* the guesses are checked past this line */
static unsigned int gap;                /* pages between the page starts kept (doubled when full) */
/* the line table read PB entries at a time into pg_buf (main RAM:
   view.c's) for the lines' heights (byte 3 of an entry): walking by a
   byte pointer, not lt_get's struct (20 times faster: the whole of a
   1 MB document's 12,000 lines in a fifth of a second, not 6 s) */
#define PB 36
static unsigned int hb_i;               /* the first line in pg_buf */
static unsigned char hb_n;              /* lines there (0: none) */

static void refill(unsigned int i)
{
    static unsigned int k;
    k = lt_n - i;
    if (k > PB)
        k = PB;
    hb_i = i;
    hb_n = (unsigned char)(cb_read(&lt, (pos_t)i * LT_SZ, pg_buf, k * LT_SZ) / LT_SZ);
}

/* line i's entry in pg_buf */
static unsigned char *ent(unsigned int i)
{
    if (i < hb_i || i >= hb_i + hb_n)
        refill(i);
    return pg_buf + (unsigned char)(i - hb_i) * LT_SZ;
}

/* t on to line `to` (or the last line), a byte pointer through pg_buf:
   the next line starts a page after a page break's line, or when it does
   not fit (a page break's line always does). (W45: a function a line
   through a struct pointer made the view's page starts take 30 ms a
   key) */
static void adv(unsigned int to)
{
    static unsigned char *e, b;
    static unsigned int h;
    if (t.i >= to || t.i + 1 >= lt_n)
        return;
    e = ent(t.i);
    while (t.i < to && t.i + 1 < lt_n) {
        if ((unsigned char)(t.i - hb_i) + 1 >= hb_n) {
            refill(t.i);                /* (this line and the next in the buffer) */
            e = pg_buf;
        }
        h = t.acc + e[3];
        b = e[1] & 0x80;
        e += LT_SZ;
        ++t.i;
        if (b || (h && !(e[1] & 0x80) && h + e[3] > PAGE_H)) {
            ++t.page;
            t.acc = 0;
            t.ln = 1;
        } else {
            t.acc = h;
            ++t.ln;
        }
    }
}

/* the walk on through the lines that start no page: up to n of them,
   to before the end, from the buffer (fr.i's line and the next one in
   it); the lines left of n */
static unsigned int run(unsigned int n)
{
    static unsigned char *e, m, k;
    static unsigned int h, acc;
    for (;;) {
        if (!n || fr.i + 1 >= lt_n)
            return n;
        if (fr.i < hb_i || fr.i + 1 >= hb_i + hb_n)
            refill(fr.i);
        if (hb_n < 2)
            return n;
        k = hb_n - 1 - (unsigned char)(fr.i - hb_i);   /* (lines whose next is in the buffer) */
        if (k > n)
            k = (unsigned char)n;
        e = pg_buf + (unsigned char)(fr.i - hb_i) * LT_SZ + 3;
        acc = fr.acc;
        for (m = 0; m < k; ++m) {
            if (e[-2] & 0x80)
                break;                  /* (a page break's line: step's) */
            h = acc + *e;
            e += LT_SZ;
            if (h && !(e[-2] & 0x80) && h + *e > PAGE_H)
                break;                  /* (the next line starts a page: step's) */
            acc = h;
        }
        fr.acc = acc;
        fr.i += m;
        fr.ln += m;
        n -= m;
        if (m < k)
            return n;
    }
}

/* the walk back to the last page start kept at or before line L */
/* the page starts kept at or before line L among the first hi: a
   binary search (pe_l[0] is line 0; W45: the walks from either end took
   a millisecond or two of each key in a long document) */
static unsigned char below(unsigned int L, unsigned char hi)
{
    static unsigned char lo, mid;
    lo = 1;
    while (lo < hi) {
        mid = (unsigned char)(((unsigned int)lo + hi) >> 1);
        if (pe_l[mid] <= L)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

static unsigned char back(unsigned int L)
{
    static unsigned char k;
    k = below(L, vt ? vt : 1);
    fr.i = pe_l[k - 1];
    fr.page = pe_p[k - 1];
    fr.acc = 0;
    fr.ln = 1;
    fr_end = 0;
    return k;
}

static void drop(unsigned char k)
{
    static unsigned char j;
    --pe_n;
    for (j = k; j < pe_n; ++j) {
        pe_l[j] = pe_l[j + 1];
        pe_p[j] = pe_p[j + 1];
    }
}

static void reset(unsigned int L)
{
    if (!pe_n || !L) {
        pe_n = 1;
        pe_l[0] = 0;
        pe_p[0] = 1;
        gap = 1;
    }
    while (pe_n > 1 && pe_l[pe_n - 1] > L)
        --pe_n;
    vt = pe_n;
    ff_on = 0;
    vhi = 0;
    back(L);
}

/* lines L .. L + o were replaced by L .. c */
static void change(unsigned int L, unsigned int o, unsigned int c)
{
    static unsigned char k;
    static int sh;
    sh = (int)(c - L) - (int)o;
    for (k = below(L, pe_n); k < pe_n;) {
        if (pe_l[k] < L + o) {
            drop(k);
            if (k < vt)
                --vt;
            continue;
        }
        if (!sh)
            break;                      /* (the rest are where they were) */
        pe_l[k] += sh;
        ++k;
    }
    if (ff_on) {
        if (ff.i <= L)
            ff_end = 0;
        else if (ff.i >= L + o)
            ff.i += sh;
        else
            ff_on = 0;
    }
    if (vhi > L)
        vhi = vhi >= L + o ? vhi + sh : c;
    if (c > vhi)
        vhi = c;
    if (fr.i <= L)
        fr_end = 0;                     /* (lines after the walk's: more to walk) */
    else {
        if (!ff_on && vt == pe_n && fr.i >= L + o) {
            ff = fr;                    /* (where the walk was: a guess) */
            ff.i += sh;
            ff_end = fr_end;
            ff_on = 1;
        }
        k = back(L);
        if (k < vt)
            vt = k;
    }
}

static void drain(void)
{
    static unsigned char k;
    if (pq_k)
        hb_n = 0;                       /* (the table changed) */
    if (pq_k == 0xFF)
        reset(pg_lo);
    else
        for (k = 0; k < pq_k; ++k)
            change(pq_l[k], pq_o[k], pq_c[k]);
    pq_k = 0;
    pg_lo = 0xFFFF;
    if (!pe_n)
        reset(0);
}

/* the walk on up to n lines (or to line L); 1 when it is at the end */
static unsigned char walk_on(unsigned int n)
{
    static unsigned char j;
    static int d;
    while (n && !fr_end) {
        n = run(n);
        if (!n)
            break;
        --n;
        if (fr.i + 1 >= lt_n) {
            fr_end = 1;
            pe_n = vt;                  /* (guesses past the end: none right) */
            ff_on = 0;
            break;
        }
        t = fr;                         /* (one line on) */
        adv(fr.i + 1);
        fr = t;
        if (fr.acc || fr.ln != 1)
            continue;
        /* a page start: a guess here? */
        while (vt < pe_n && pe_l[vt] < fr.i)
            drop(vt);
        if (vt < pe_n && pe_l[vt] == fr.i && fr.i >= vhi) {
            d = (int)fr.page - (int)pe_p[vt];
            for (j = vt; j < pe_n; ++j)
                pe_p[j] += d;
            vt = pe_n;
            vhi = 0;
            if (ff_on) {
                fr = ff;
                fr.page += d;
                fr_end = ff_end;
                ff_on = 0;
            }
            continue;
        }
        if (pe_n == PE_MAX) {
            /* full: every other one kept, twice the gap */
            for (j = 2, d = 1; j < pe_n; j += 2, ++d) {
                pe_l[d] = pe_l[j];
                pe_p[d] = pe_p[j];
            }
            vt = (unsigned char)((vt + 1) / 2);
            pe_n = (unsigned char)d;
            gap <<= 1;
        }
        if (fr.page - pe_p[vt - 1] >= gap && pe_n < PE_MAX) {
            for (j = pe_n; j > vt; --j) {
                pe_l[j] = pe_l[j - 1];
                pe_p[j] = pe_p[j - 1];
            }
            pe_l[vt] = fr.i;
            pe_p[vt] = fr.page;
            ++vt;
            ++pe_n;
        }
    }
    return fr_end;
}

/* the page and the line there of line L (pg_pc, pg_lc), and the pages
   (pg_tot); pg_est: 1 the page a guess, 2 the count a guess */
void pg_at(void)
{
    static unsigned int L;
    static unsigned char k;
    static unsigned long e;
    L = pg_arg;
    drain();
    if (L >= fr.i)
        walk_on(L - fr.i + 1 < 400 ? L - fr.i + 1 : 400);
    pg_est = 0;
    if (L < fr.i || fr_end) {
        k = below(L, vt);
        t.i = pe_l[k - 1];
        t.page = pe_p[k - 1];
        t.acc = 0;
        t.ln = 1;
        adv(L);
        pg_pc = t.page;
        pg_lc = t.ln;
    } else {
        e = fr.i ? (unsigned long)(L - fr.i) * fr.page / fr.i : 0;
        pg_pc = fr.page + (unsigned int)e;
        pg_lc = 1;
        pg_est = 1;
    }
    if (fr_end && !ff_on) {
        pg_tot = fr.page;
        if (ld_kind)
            pg_est |= 2;
    } else {
        /* the pages so far, as many more for the lines (and the text) to come */
        e = fr.i > 50 ? (unsigned long)fr.page * lt_n / fr.i : lt_n / 48 + 1;
        if (ld_kind && ld_size > ld_left)
            e = e * (ld_size >> 8) / ((ld_size - ld_left) >> 8 | 1);
        pg_tot = (unsigned int)(e > 65535UL ? 65535UL : e);
        pg_est |= 2;
    }
    if (pg_tot < pg_pc)
        pg_tot = pg_pc;
}

/* the main loop's idle time: the walk on a while; 1 more to do */
unsigned char pg_idle(void)
{
#ifndef CW_HOST
    if ((unsigned char)(cw_jiffies() - sp_kj) < 30)
        return 1;                       /* (W73: half a second after a key: typing goes first) */
#endif
    drain();
    return !walk_on(64) || ff_on;      /* (W73: 64 lines a pass: a key waits no longer) */
}

/* the first line of page pg_arg, all of the document counted (pg_tot) */
void pg_first(void)
{
    static unsigned char k;
    drain();
    while (!walk_on(0xFFFF))
        ;
    pg_tot = fr.page;
    for (k = vt; k > 1 && pe_p[k - 1] > pg_arg; --k)
        ;
    t.i = pe_l[k - 1];
    t.page = pe_p[k - 1];
    t.acc = 0;
    t.ln = 1;
    while (t.page < pg_arg && t.i + 1 < lt_n)
        adv(t.i + 1);
    pg_line = t.i;
}

/* the view's lines pg_arg .. pg_arg + VMAX - 1 (pgview.c's rows_build):
   pv_st's bit k set when line pg_arg + k starts a page (line 0 not),
   pv_pg line pg_arg's page: counted from the last page start kept before
   them that is not a guess (the walk need not be past them: W45, taking
   it there cost 3.6 ms a key) */
void pg_view(void)
{
    static const unsigned char bit[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
    static unsigned int L, i, acc, h;
    static unsigned char k, b, *e;
    L = pg_arg;
    drain();
    for (k = 0; k < (VMAX + 7) / 8; ++k)
        pv_st[k] = 0;
    k = below(L, vt);
    t.i = pe_l[k - 1];
    t.page = pe_p[k - 1];
    t.acc = 0;
    t.ln = 1;
    adv(L);
    pv_pg = t.page;
    /* the window's lines in one pass, as adv walks (W45: a call a line
       took 1,600 cycles) */
    i = t.i;
    acc = t.acc;
    e = ent(i);
    for (k = 0;;) {
        if (i && !acc)
            pv_st[k >> 3] |= bit[k & 7];
        if (++k >= VMAX || i + 1 >= lt_n)
            break;
        if ((unsigned char)(i - hb_i) + 1 >= hb_n) {
            refill(i);
            e = pg_buf;
        }
        h = acc + e[3];
        b = e[1] & 0x80;
        e += LT_SZ;
        ++i;
        acc = b || (h && !(e[1] & 0x80) && h + e[3] > PAGE_H) ? 0 : h;
    }
}
