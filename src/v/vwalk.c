/* Veranda Write on Veranda: walking a line (overlay 5, with view2.c),
   the x of a place and the place at an x (overlay 12), a row's part of
   the selection inverted (overlay 29): out of view2.c for the room
   scrolling wanted there (W73). Main RAM's view.c and the overlays call
   them through glue.s; the last two walk the line through it too.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <vrwin.h>
#include "cwv.h"
#include "view.h"
#pragma code-name ("OVERLAY5")
#pragma rodata-name ("OVL5RO")
#pragma bss-name ("OVL5BSS")

extern unsigned char lb[128];
extern unsigned int xs, sp_k;
unsigned int __fastcall__ sp_w(unsigned int w);

unsigned int e_walk(pos_t p0, const struct line *l, unsigned char mode, pos_t t)
{
    static unsigned int x, k, j, w;
    static pos_t p, e, last;
    static unsigned char c, s;
    line_geom(l, p0);
    x = xs = TX0 + lg_x0;
    sp_k = 0;
    p = p0;
    e = p0 + l->len;
    last = p0;
    s = l->s;
    fl_style(s);
    while (p < e) {
        k = cb_read(&tx, p, lb, e - p > sizeof lb ? sizeof lb : (unsigned int)(e - p));
        for (j = 0; j < k;) {
            c = lb[j];
            if (IS_KIND(c)) {
                if (j + 3 > k)
                    break;
                if (c == K_CS) {
                    s = lb[j + 1];
                    fl_style(s);
                }
                j += 3;
                continue;
            }
            if (mode == 0 && p + j >= t)
                return x;
            w = c == CH_TAB ? TABW - (x - xs) % TABW : c == CH_CR ? 0 : c == ' ' ? sp_w(CHW(c)) : CHW(c);
            if (mode == 1 && (c == CH_CR || t < x + w / 2)) {
                w_hit = p + j;
                return x;
            }
            last = p + j;
            x += w;
            ++j;
        }
        if (!j)
            break;
        p += j;
    }
    if (mode == 1)
        w_hit = (l->f & LF_PARA) || e >= tx.total ? e : last;
    return x;
}

#pragma code-name ("OVERLAY12")
#pragma rodata-name ("OVL12RO")
#pragma bss-name ("OVL12BSS")
extern struct line wl;                  /* (view.c: main RAM, e_walk reads it from overlay 5) */

unsigned int __fastcall__ e_line_x(unsigned int i, pos_t p)
{
    lw_start = lt_start(i);
    lt_get(i, &wl);
    if (IS_TR(&wl))
        return tb_lx(i, p);             /* (1.6: a table's row, tbview.c) */
    return walk(lw_start, &wl, 0, p);
}

pos_t __fastcall__ e_line_hit(unsigned int i, unsigned int x)
{
    lw_start = lt_start(i);
    lt_get(i, &wl);
    if (IS_TR(&wl))
        return tb_lh(i, x);
    walk(lw_start, &wl, 1, x);
    return pos_norm(w_hit);
}

#pragma code-name ("OVERLAY29")
#pragma rodata-name ("OVL29RO")
#pragma bss-name ("OVL29BSS")

/* the selection's part of row i inverted (on the screens dispBufferOn says) */
void e_row_sel(unsigned char i, pos_t lo, pos_t hi)
{
    struct vrow *r = &vr[i];
    pos_t e = r->pos + r->l.len;
    unsigned int x0, x1;
    if (IS_TR(&r->l)) {
        tb_sel(i, lo, hi);              /* (1.6: a table's row, tbview.c) */
        return;
    }
    if (lo < r->pos)
        lo = r->pos;
    if (hi > e)
        hi = e;
    if (lo >= hi)
        return;
    x0 = walk(r->pos, &r->l, 0, lo);
    x1 = walk(r->pos, &r->l, 0, hi);
    if (hi == e && (r->l.f & LF_PARA))
        x1 += 5;                        /* the paragraph's end selected */
    if (x1 > PG_R)
        x1 = PG_R;
    if (x1 > x0)
        WInvertRectangle(x0, r->y, x1 - 1, row_bot(i));
}

