/* Veranda Write: word wrap and the line table.

   Every line of the document has an entry in the line table (a chunked
   buffer of LT_SZ-byte entries in RAM banks): its bytes, its height, its
   baseline, the style in effect at its start and whether it ends its
   paragraph. The lines' places are their lengths summed (lt_find walks
   from an anchor, the last line found), so an edit changes only the
   entries of the lines it rewraps: lay_dirty notes the edits, lay_fix
   wraps again from the line before the first one, line by line, until a
   new line starts where an old one did (moved by what was inserted and
   deleted), past the edits, in the same style; from there the old lines
   are right as they are (DECISIONS W4).

   Wrapping is greedy: a line takes characters while they fit wrap_w; it
   breaks after the last space, tab or hyphen that fitted (spaces at its
   end may run past the edge), else before the character that did not fit
   (a word longer than a line); CR ends it. Its height is the tallest
   ascent and descent of the styles of its characters, and LEADING.

   The damage (dm_lo .. dm_hi, places in the text as it is now) is what
   the view must draw again: the bytes edited and the lines wrapped again
   since the view last looked (the view clears dm_on).

   This file is the line table itself (main RAM on the X16); layfix.c
   wraps (overlay 4).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include "cw.h"
#include "laypriv.h"

struct cbuf lt;                         /* (its tables: swap.c's) */
unsigned int lt_n;
unsigned int wrap_w = 520;
unsigned int lf_i;
pos_t lf_pos;
unsigned int lay_c0, lay_c1;
unsigned int pg_lo = 0xFFFF;
unsigned int pq_l[PQ_N], pq_o[PQ_N], pq_c[PQ_N];
unsigned char pq_k;
int lay_shift;
void (*lay_busy)(void);

unsigned char dm_on;
pos_t dm_lo, dm_hi;
long dm_delta;

unsigned int an_i;               /* the anchor: a line, its start */
pos_t an_pos;
unsigned int ae_i;               /* the last edit's */
pos_t ae_pos;

unsigned char dirty;             /* the edits since lay_fix */
pos_t d_lo, d_hi;                /* their place, now */
long d_delta;                    /* bytes inserted - deleted */

/* a window of LW entries in main RAM: walking the table (finding a
   line, the pages) reads it a window at a time, not an entry; layfix.c
   empties it (lw_n = 0) whenever it changes the table */
#define LW 32
static unsigned char lw[LW * LT_SZ];
static unsigned int lw_i;               /* the first entry there */
unsigned char lw_n;                     /* entries there (0: none) */

static unsigned char *entry(unsigned int i)
{
    unsigned int k;
    if (i < lw_i || i >= lw_i + lw_n) {
        /* (walking back: i near the window's end; on: near its start) */
        if (i < lw_i)
            lw_i = i >= LW - LW / 4 ? i - (LW - LW / 4) : 0;
        else
            lw_i = i >= LW / 4 ? i - LW / 4 : 0;
        k = lt_n - lw_i;
        if (k > LW)
            k = LW;
        lw_n = (unsigned char)(cb_read(&lt, (pos_t)lw_i * LT_SZ, lw, k * LT_SZ) / LT_SZ);
    }
    return lw + (unsigned char)(i - lw_i) * LT_SZ;
}

void FASTCALL lt_get(unsigned int i, struct line *l)
{
    unsigned char *e = entry(i);
    l->len = e[0] | (unsigned int)(e[1] & 7) << 8;
    l->f = (e[1] >> 3) & 3;
    l->w = e[2] | (unsigned int)(e[1] >> 5) << 8;
    l->h = e[3];
    l->a = e[4];
    l->s = e[5];
    l->p = e[6];
}

unsigned int FASTCALL par_number(unsigned int i)
{
    static unsigned char *e;
    unsigned int n = 0;
    for (;;) {
        e = entry(i);
        if (e[1] & (LF_FIRST << 3)) {
            if (par[e[6]].kind != PK_NUMBER)
                return n;
            ++n;
        }
        if (!i)
            return n;
        --i;
        e = entry(i);
        if (par[e[6]].kind != PK_NUMBER)
            return n;
    }
}

/* line l's text, starting at p: where it starts, how its spaces stretch */
unsigned int lg_x0, lg_ex, lg_rem, lg_nsp;
static unsigned char gb[64];

void FASTCALL line_geom(const struct line *l, pos_t p)
{
    static const struct pstyle *ps;
    static unsigned int avail, n, k, i, pend;
    static unsigned char first, c, skip;
    static pos_t q, e;
    ps = &par[l->p];
    first = (l->f & LF_FIRST) && !IS_LIST(ps->kind);
    avail = wrap_w - ps->left - ps->right;
    lg_x0 = ps->left;
    if (first) {
        avail -= ps->first;
        lg_x0 += ps->first;
    }
    lg_ex = lg_rem = 0;
    lg_nsp = 0;
    if (l->w >= avail || ps->align == PA_LEFT)
        return;
    k = avail - l->w;
    if (ps->align == PA_CENTRE) {
        lg_x0 += k / 2;
        return;
    }
    if (ps->align == PA_RIGHT) {
        lg_x0 += k;
        return;
    }
    /* justified: not its paragraph's last line; the spaces before its
       last character */
    if (l->f & LF_PARA)
        return;
    q = p;
    e = q + l->len;
    if (e >= tx.total)
        return;
    n = pend = 0;
    skip = 0;
    while (q < e) {
        i = cb_read(&tx, q, gb, e - q > sizeof gb ? sizeof gb : (unsigned int)(e - q));
        for (k = 0; k < i; ++k) {
            c = gb[k];
            if (skip) {
                --skip;
                continue;
            }
            if (IS_KIND(c))
                skip = 2;
            else if (c == ' ')
                ++pend;
            else if (c != CH_TAB) {
                n += pend;
                pend = 0;
            }
        }
        q += i;
    }
    if (!n)
        return;
    lg_nsp = n;
    k = avail - l->w;
    lg_ex = k / n;
    lg_rem = k % n;
}

unsigned int FASTCALL lt_len_of(unsigned int i)
{
    unsigned char *e = entry(i);
    return e[0] | (unsigned int)(e[1] & 7) << 8;
}

/* the anchor to walk from (ak, as: a line, its start): the nearest to p
   of the last line found, the last edit's (ae: lay_fix's, so that the
   main loop's walks do not take it away from where the edits are), the
   start, and the last line (while the table is the text's) */
static unsigned int ak;
static pos_t as;

static void near_p(pos_t p)
{
    static pos_t d, e, s;
    static unsigned int k;
    ak = 0;
    as = 0;
    d = p;
    if (an_i < lt_n) {
        e = p > an_pos ? p - an_pos : an_pos - p;
        if (e < d) {
            d = e;
            ak = an_i;
            as = an_pos;
        }
    }
    if (ae_i < lt_n) {
        e = p > ae_pos ? p - ae_pos : ae_pos - p;
        if (e < d) {
            d = e;
            ak = ae_i;
            as = ae_pos;
        }
    }
    if (!dirty && lt_n > 1) {
        k = lt_n - 1;
        s = tx.total - lt_len_of(k);
        e = p > s ? p - s : s - p;
        if (e < d) {
            ak = k;
            as = s;
        }
    }
}

void FASTCALL lt_find(pos_t p)
{
    unsigned int i, l;
    pos_t s;
    near_p(p);
    i = ak;
    s = as;
    while (p < s && i) {
        --i;
        s -= lt_len_of(i);
    }
    for (;;) {
        l = lt_len_of(i);
        if (p < s + l || i + 1 >= lt_n)
            break;
        s += l;
        ++i;
    }
    lf_i = an_i = i;
    lf_pos = an_pos = s;
}

pos_t FASTCALL lt_start(unsigned int i)
{
    unsigned int k = an_i, d;
    pos_t s = an_pos;
    if (k >= lt_n) {
        k = 0;
        s = 0;
    }
    d = k > i ? k - i : i - k;
    if (i < d) {
        k = 0;
        s = 0;
        d = i;
    }
    if (ae_i < lt_n && (ae_i > i ? ae_i - i : i - ae_i) < d) {
        k = ae_i;
        s = ae_pos;
    }
    while (k > i) {
        --k;
        s -= lt_len_of(k);
    }
    while (k < i) {
        s += lt_len_of(k);
        ++k;
    }
    an_i = k;
    an_pos = s;
    return s;
}

static void region(pos_t *lo, pos_t *hi, pos_t p, pos_t n1, pos_t n0)
{
    if (p < *lo)
        *lo = p;
    if (p + n0 > *hi)
        *hi = p + n0;
    *hi = *hi - n0 + n1;
}

void lay_dirty(pos_t p, pos_t n1, pos_t n0)
{
    if (!dirty) {
        dirty = 1;
        d_lo = p;
        d_hi = p + n1;
        d_delta = (long)n1 - (long)n0;
    } else {
        region(&d_lo, &d_hi, p, n1, n0);
        d_delta += (long)n1 - (long)n0;
    }
    if (!dm_on) {
        dm_on = 1;
        dm_lo = p;
        dm_hi = p + n1;
        dm_delta = 0;
    } else
        region(&dm_lo, &dm_hi, p, n1, n0);
    dm_delta += (long)n1 - (long)n0;
}

