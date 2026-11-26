/* Veranda Write: word wrap, the line table made and mended (overlay 4
   on the X16; layout.c says how).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CW_HOST
#include "v/ovl4.h"
#endif
#include "cw.h"
#include "laypriv.h"
#ifndef CW_HOST
extern unsigned char bank27;            /* (table.c's overlay: v/cwv.h) */
void __fastcall__ callb(unsigned char b, void (*f)(void));
#endif

/*---------------------------------------------------------------
  one line
 ---------------------------------------------------------------*/
/* (wr_len, wr_w, wr_h, wr_a, wr_f, wr_s, lc_first, lc_ps: layout.c's,
   main RAM: a table's row, table.c, another overlay, wraps its cells
   with this and reads them) */
unsigned int wr_lim;                    /* the line's width (wrap_w less the indents) */

/* the window of the text being wrapped, and wr_scan's state (the X16:
   wrscan.s reads them; the host: the C below) */
#ifndef CW_HOST
#pragma bss-name (push, "BSS")          /* (cbuf.c reads and writes them: main RAM) */
#endif
unsigned char wr_rb[128];
#ifndef CW_HOST
#pragma bss-name (pop)
#endif
unsigned char wr_ri, wr_rn;             /* the next byte in wr_rb, the bytes there */
static pos_t wr_at;                     /* where wr_rb is from in the text (wr_rn 0: nowhere) */
unsigned int wr_x;                      /* the line's width so far */
unsigned char wr_n;                     /* wr_scan: characters taken */
unsigned char wr_any;                   /* the line has a character (one that does not fit is taken alone) */
unsigned char wr_ovf;                   /* wr_scan stopped at one that does not fit */

#ifdef CW_HOST
/* plain characters (not a space, tab, hyphen, CR or code) from wr_ri
   while they fit wrap_w */
void wr_scan(void)
{
    unsigned char c, w;
    wr_n = 0;
    wr_ovf = 0;
    while (wr_ri < wr_rn) {
        c = wr_rb[wr_ri];
        if (c <= ' ' || c == '-')
            return;
        w = CHW(c);
        if (wr_x + w > wr_lim && wr_any) {
            wr_ovf = 1;
            return;
        }
        wr_x += w;
        wr_any = 1;
        ++wr_ri;
        ++wr_n;
    }
}
#else
void wr_scan(void);                     /* wrscan.s */
#endif

/* the paragraph starting at p (its code read: wr_rb holds it); the
   line's width */
static void para_in(pos_t p)
{
    static struct pstyle *ps;
    if (lc_first)
        lc_ps = wr_ri + 2 < wr_rn && wr_rb[wr_ri] == K_PS ? wr_rb[wr_ri + 1] : 0;
    ps = &par[lc_ps];
    wr_lim = wrap_w - ps->left - ps->right;
    if (lc_first && !IS_LIST(ps->kind))
        wr_lim -= ps->first;
    if ((int)wr_lim < (tb_in ? 16 : 40))
        wr_lim = tb_in ? 16 : 40;       /* (a narrow table's cell: 16) */
    (void)p;
}

void wrap(pos_t p, unsigned char s)
{
    static unsigned int i, brk, bx, w, vis, bvis;
    static unsigned char c, am, dm, ba, bd, bs, counted, pbk;
    static struct pstyle *ps;
    fl_style(s);
    wr_x = i = brk = bx = vis = bvis = 0;
    am = dm = counted = wr_any = pbk = 0;
    if (wr_rn && p >= wr_at && p + 2 < wr_at + wr_rn)
        wr_ri = (unsigned char)(p - wr_at);   /* (the line before's window: a layout reads on) */
    else {
        wr_at = p;
        wr_rn = (unsigned char)cb_read(&tx, p, wr_rb, sizeof wr_rb);
        wr_ri = 0;
    }
    if (lc_first && !tb_in && wr_ri < wr_rn && IS_KIND(wr_rb[wr_ri])) {
        /* (1.6) a paragraph that starts with a code: a table's row? (table.c) */
        tb_p = p;
        tb_s = s;
#ifdef CW_HOST
        tb_wrap();
#else
        callb(bank27, tb_wrap);
#endif
        if (tb_is)
            return;
    }
    para_in(p);
    wr_f = 0;
    for (;;) {
        if (wr_ri + 2 >= wr_rn) {       /* (a code is read whole) */
            if (p + i >= tx.total)
                break;
            if (wr_ri) {
                wr_at = p + i;
                wr_rn = (unsigned char)cb_read(&tx, wr_at, wr_rb, sizeof wr_rb);
                wr_ri = 0;
            }
            if (!wr_rn)
                break;
        }
        c = wr_rb[wr_ri];
        if (c > ' ' && c != '-') {
            /* a run of plain characters, as many as fit (wrscan.s) */
            if (!counted) {
                if (fl_a > am) am = fl_a;
                if (fl_d > dm) dm = fl_d;
                counted = 1;
            }
            wr_scan();
            i += wr_n;
            if (wr_n) {
                vis = wr_x;
                pbk = 2;                /* (a character: no page break's paragraph) */
            }
            if (wr_ovf) {
                if (brk) {
                    i = brk;
                    am = ba;
                    dm = bd;
                    s = bs;
                    wr_x = bx;
                    vis = bvis;
                }
                break;
            }
            if (i >= MAXLINE)
                break;
            continue;
        }
        if (IS_KIND(c)) {
            if (c == K_CS) {
                s = wr_rb[wr_ri + 1];
                fl_style(s);
                counted = 0;
            } else if (c == K_PB)
                pbk |= 1;
            wr_ri += 3;
            i += 3;
            continue;
        }
        if (c == CH_CR) {
            if (!counted) {
                if (fl_a > am) am = fl_a;
                if (fl_d > dm) dm = fl_d;
            }
            ++i;
            wr_f = LF_PARA;
            break;
        }
        if (c == ' ' || c == CH_TAB) {
            w = c == ' ' ? fl_w[0] + fl_bold : TABW - wr_x % TABW;
            if (!counted) {
                if (fl_a > am) am = fl_a;
                if (fl_d > dm) dm = fl_d;
                counted = 1;
            }
            wr_x += w;
            if (c == CH_TAB)
                vis = wr_x;
            pbk = 2;
            ++wr_ri;
            brk = ++i;
            ba = am;
            bd = dm;
            bs = s;
            bx = wr_x;
            bvis = vis;
            continue;
        }
        /* a hyphen: a character, a break after it */
        w = CHW(c);
        if (wr_x + w > wr_lim && wr_any) {
            if (brk) {
                i = brk;
                am = ba;
                dm = bd;
                s = bs;
                wr_x = bx;
                vis = bvis;
            }
            break;
        }
        if (!counted) {
            if (fl_a > am) am = fl_a;
            if (fl_d > dm) dm = fl_d;
            counted = 1;
        }
        wr_x += w;
        vis = wr_x;
        wr_any = 1;
        pbk = 2;
        ++wr_ri;
        brk = ++i;
        ba = am;
        bd = dm;
        bs = s;
        bx = wr_x;
        bvis = vis;
        if (i >= MAXLINE)
            break;
    }
    if (!am && !dm) {
        fl_style(s);
        am = fl_a;
        dm = fl_d;
    }
    /* the height: the line's, spaced, the paragraph's space above its
       first line and below its last */
    ps = &par[lc_ps];
    w = am + dm + LEADING;
    if (ps->spacing > 2)
        w = w * ps->spacing / 2;
    bx = am;
    if (lc_first) {
        w += ps->before;
        bx += ps->before;
        wr_f |= LF_FIRST;
    }
    if (wr_f & LF_PARA)
        w += ps->after;
    /* (1.4) a page break's own paragraph: the code and the CR, nothing else */
    if (pbk == 1 && (wr_f & (LF_FIRST | LF_PARA)) == (LF_FIRST | LF_PARA))
        wr_f |= LF_PB;
    wr_len = i;
    wr_w = vis > TB_W - 1 ? TB_W - 1 : vis;   /* (TB_W: a table's row's) */
    wr_a = bx > 250 ? 250 : (unsigned char)bx;
    wr_h = w > 255 ? 255 : (unsigned char)w;
    wr_s = s;
    lc_first = wr_f & LF_PARA;
}

/*---------------------------------------------------------------
  the table
 ---------------------------------------------------------------*/
#define NB 16
#ifndef CW_HOST
#pragma bss-name (push, "BSS")
#endif
static unsigned char nb[NB * LT_SZ];
#ifndef CW_HOST
#pragma bss-name (pop)
#endif
static unsigned char nb_n;

/* an entry (layout.c's lt_get reads it): the length's low byte; its
   high bits (0-2), LF_PARA and LF_FIRST (3-4), the width's high bits
   (5-6: 1,023 at most, the text 528 pixels wide at most), LF_PB (7); the
   width's low byte, the height, the baseline, the style at its start,
   the paragraph's style */
static unsigned char *nb_e;             /* (nb + nb_n * LT_SZ) */

static void nb_add(unsigned char s)
{
    unsigned char *e = nb_e;
    nb_e += LT_SZ;
    if (wr_w > 1023)
        wr_w = 1023;                    /* (tabs past the edge) */
    e[0] = (unsigned char)wr_len;
    e[1] = ((unsigned char)(wr_len >> 8) & 7) | (wr_f & 3) << 3 | (unsigned char)(wr_w >> 8) << 5
           | (wr_f & LF_PB ? 0x80 : 0);
    e[2] = (unsigned char)wr_w;
    e[3] = wr_h;
    e[4] = wr_a;
    e[5] = s;
    e[6] = lc_ps;
    ++nb_n;
}

static unsigned char flush(void)
{
    unsigned char e = 0;
    lw_n = 0;
    if (lt_n + nb_n > LT_LINES) {
        e = 1;
        nb_n = 0;
    }
    if (nb_n && cb_append(&lt, nb, nb_n * LT_SZ, BANK_SIZE - BANK_SIZE % LT_SZ))
        e = 1;
    lt_n += nb_n;
    nb_n = 0;
    nb_e = nb;
    if (lay_busy)
        lay_busy();
    return e;
}

unsigned char lay_all(void)
{
    pos_t p = 0;
    unsigned char s = 0, e = 0;
    cb_clear(&lt);
    ++ai_gen;                           /* (W68: AI help's answer to an earlier text not used) */
    lt_n = 0;
    lw_n = 0;
    sa_ok = 0;
    wr_rn = 0;                          /* (the text changed: no window) */
    nb_n = 0;
    nb_e = nb;
    dirty = 0;
    an_i = ae_i = 0;
    an_pos = ae_pos = 0;
    lc_first = 1;
    lc_ps = 0;
    for (;;) {
        wrap(p, s);
        nb_add(s);
        p += wr_len;
        s = wr_s;
        if (nb_n == NB)
            e |= flush();
        if (p >= tx.total) {
            if (wr_len && (wr_f & LF_PARA)) {
                wrap(p, s);             /* the empty line after the last CR */
                nb_add(s);
            }
            break;
        }
        if (!wr_len)
            break;
    }
    e |= flush();
    lay_c0 = 0;
    lay_c1 = lt_n;
    lay_shift = 0;
    pg_lo = 0;
    pq_k = 0xFF;
    dm_on = 1;
    dm_lo = de_lo = 0;
    dm_hi = de_hi = tx.total;
    dm_delta = 0;
    return e;
}

static unsigned int wi, oc;             /* the next new line's index; old lines taken */
static unsigned int ob;                 /* old lines replaced so far */
static unsigned char lay_e;

/* the new lines in place of the old ones taken: as many as there are of
   both written over them, the rest deleted or inserted (W43: deleting
   all and inserting all moved the table's chunk twice a key, its bytes
   after the place; 8 KB at most each time) */
static void commit(void)
{
    unsigned int k = oc, m;
    lw_n = 0;
    if (lt_n - k + nb_n > LT_LINES) {
        lay_e = 1;                      /* (too many lines: the new ones dropped) */
        nb_n = 0;
    }
    m = k < nb_n ? k : nb_n;
    if (m)
        cb_write(&lt, (pos_t)wi * LT_SZ, nb, m * LT_SZ);
    if (k > m)
        cb_delete(&lt, (pos_t)(wi + m) * LT_SZ, (pos_t)(k - m) * LT_SZ);
    if (nb_n > m && cb_insert(&lt, (pos_t)(wi + m) * LT_SZ, nb + m * LT_SZ, (nb_n - m) * LT_SZ))
        lay_e = 1;
    lt_n = lt_n - k + nb_n;
    wi += nb_n;
    ob += k;
    oc = 0;
    nb_n = 0;
    nb_e = nb;
}

unsigned char lay_fix(void)
{
    unsigned int L, oi, old_n, ol;
    pos_t P, P0, op, hi_old, ns;
    unsigned char S, inside, ended;
    struct line l;
    if (!dirty)
        return 0;
    wr_rn = 0;                          /* (the text changed: no window) */
    lay_e = 0;
    hi_old = (pos_t)((long)d_hi - d_delta);
    lt_find(d_lo);                      /* (dirty: the table not the text's at its end) */
    dirty = 0;
    L = lf_i;
    P = lf_pos;
    if (L > 0) {
        --L;
        P -= lt_len_of(L);
    }
    lt_get(L, &l);
    S = l.s;
    lc_first = l.f & LF_FIRST;
    lc_ps = l.p;
    P0 = P;
    old_n = lt_n;
    oi = L;                             /* the old line not yet taken ... */
    op = P;                             /* ... and its place, before the edits */
    wi = L;
    oc = 0;
    ob = 0;
    nb_n = 0;
    nb_e = nb;
    PM("lf<");
    for (;;) {
        wrap(P, S);
        PM("wrap");
        nb_add(S);
        P += wr_len;
        S = wr_s;
        ended = (wr_f & LF_PARA) != 0;
        /* the old lines that start before P are replaced */
        for (;;) {
            if (oi >= old_n)
                break;
            inside = op > d_lo && op < hi_old;
            ns = op < hi_old ? op : (pos_t)((long)op + d_delta);
            if (inside ? P <= d_lo : ns >= P)
                break;
            ol = lt_len_of(wi + oc);
            op += ol;
            ++oi;
            ++oc;
        }
        if (nb_n == NB)
            commit();
        if (oi < old_n && op >= hi_old) {
            /* an old line that starts after the edits, where this one ends */
            if ((pos_t)((long)op + d_delta) == P) {
                lt_get(wi + oc, &l);
                /* (the empty last line hangs on the CR before it; W43: a
                   line that starts a paragraph has LF_FIRST, lc_first is
                   LF_PARA then: compared as they were, 2 and 1, no line
                   at a paragraph's start was ever in step, and a key
                   wrapped again every line to the next paragraph's
                   second, or to the document's end) */
                if (l.s == S && l.len && !(l.f & LF_FIRST) == !lc_first && (lc_first || l.p == lc_ps))
                    break;              /* in step with the old lines again */
            }
        }
        if (P >= tx.total) {
            if (ended && wr_len) {
                wrap(P, S);             /* the empty line after the last CR */
                nb_add(S);
            }
            oc += old_n - oi;
            oi = old_n;
            break;
        }
        if (!wr_len)
            break;
        if (lay_busy && !(wi & 31) && !nb_n)
            lay_busy();
    }
    PM("lf>");
    commit();
    PM("commit");
    an_i = ae_i = L;
    an_pos = ae_pos = P0;
    lay_c0 = L;
    lay_c1 = wi;
    lay_shift = (int)(wi - L) - (int)ob;
    /* (the pages from here: the view's to count again) */
    if (L < pg_lo)
        pg_lo = L;
    if (pq_k < PQ_N) {
        pq_l[pq_k] = L;
        pq_o[pq_k] = ob;
        pq_c[pq_k] = wi;
        ++pq_k;
    } else
        pq_k = 0xFF;
    /* the damage: the lines wrapped again */
    if (P0 < dm_lo)
        dm_lo = P0;
    if (P > dm_hi)
        dm_hi = P;
    return lay_e ? 2 : 1;
}
