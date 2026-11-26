/* Veranda Write on Veranda: a table's row on the screen (overlay 27,
   with table.c, which lays it out; DECISIONS W63).

   A row is one line, one row of the view (vr[]), as tall as its tallest
   cell (at most 255 pixels): draw_row (view.c) hands it here. Its
   sub-lines (table.c's tb_lay) are drawn as draw_row draws a line, by
   render.s into the row bank, in bands of up to 48 pixel rows (the
   buffer's) written to both screens one after the other, each band
   every cell's sub-lines that reach into it (a glyph's rows outside the
   band are render.s's to skip); then the borders, the spelling marks,
   the selection, with the kernel's lines on both screens. The cursor's
   place (tb_cpos), a place's x (tb_lx), the place at a point (tb_lh: the
   x and view.c's tb_hy, the y in the row of the last view_row), the
   selection's part of the row (tb_sel), and Up, Down, Home and End in a
   cell (tb_nav) walk its sub-lines as view2.c's walk does a line.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#include "view.h"
#include "../spell.h"
#pragma code-name ("OVERLAY27")
#pragma rodata-name ("OVL27RO")
#pragma bss-name ("OVL27BSS")

extern unsigned char lb[128];           /* (view.c's: main RAM, cb_read's) */
extern char run[100];
extern unsigned int xs, sp_k;
unsigned int __fastcall__ sp_w(unsigned int w);
extern unsigned int cx, ch;             /* (view.c: the cursor) */
extern unsigned int tb_hy;              /* (view.c's view_row: the y in the row) */
extern unsigned char mk_n;
extern unsigned int mk_o[], mk_x[];

#define RBH 48                          /* (render.s: the buffer's rows) */
void e_tb_sel(unsigned char i, pos_t lo, pos_t hi);

static pos_t rp;                        /* the row's place */
static unsigned int rem;                /* the text's bytes from it (0xFFFF at most) */
static unsigned int ry;                 /* its top on the screen (a row of the view's) */
static unsigned int bxs[TB_MAXC + 1];   /* its borders on the screen */
static struct sub cs;                   /* the sub-line walked (a copy: tsl[cj]) */
static unsigned char cj;

/* the row at p (style s) laid out: rp, rem, bxs */
static void FASTCALL at_row(pos_t p, unsigned char s)
{
    static unsigned char k;
    rp = p;
    rem = tx.total - p > 0xFFFF ? 0xFFFF : (unsigned int)(tx.total - p);
    tb_lay(p, s);
    for (k = 0; k <= tc_n; ++k)
        bxs[k] = TX0 + tc_x[k];
}

static void FASTCALL vrow(unsigned char i)
{
    ry = vr[i].y;
    at_row(vr[i].pos, vr[i].l.s);
}

static unsigned int FASTCALL off(pos_t p)
{
    if (p < rp)
        return 0;
    p -= rp;
    return p > 0xFFFF ? 0xFFFF : (unsigned int)p;
}

static void FASTCALL get(unsigned char j)
{
    cj = j;
    memcpy(&cs, tsl + j, sizeof cs);
}

/* walking sub-line cs (view2.c's walk): wm 0 wx the x of offset wt, 1
   wo the offset at x wx, 2 its text drawn into the band (wa: its
   baseline's row there) */
static unsigned char wm;
static unsigned int wt, wx, wo;
static int wa;
static unsigned char rn;                /* (2: the run's characters) */
static unsigned int rx;

static void flush(void)
{
    if (!rn)
        return;
    rr_s = run;
    rr_n = rn;
    rr_x = rx - PG_L;
    rr_top = wa - fl_a;
    rr_run();
    rn = 0;
}

static void FASTCALL style(unsigned char s)
{
    if (wm == 2)
        fl_rr(s);
    else
        fl_style(s);
}

static void swalk(void)
{
    static unsigned int x, o, e, last, k, i, w, cw;
    static unsigned char c, d;
    cw = wrap_w;
    wrap_w = TB_CW(cs.c);
    line_geom(&cs.l, rp + cs.off);
    wrap_w = cw;
    x = xs = bxs[cs.c] + TB_PADL + lg_x0;
    sp_k = 0;
    o = last = cs.off;
    e = o + cs.l.len;
    style(cs.l.s);
    rn = 0;
    while (o < e) {
        k = cb_read(&tx, rp + o, lb, e - o > sizeof lb ? sizeof lb : e - o);
        for (i = 0; i < k;) {
            c = lb[i];
            d = 0;
            if (wm == 2 && (IS_KIND(c) || c == CH_TAB || c == CH_CR || (c == ' ' && lg_nsp) || rn == sizeof run - 1)) {
                flush();
                d = 1;
            }
            if (IS_KIND(c)) {
                if (i + 3 > k)
                    break;
                if (c == K_CS)
                    style(lb[i + 1]);
                i += 3;
                continue;
            }
            if (!wm && o + i >= wt) {
                wx = x;
                return;
            }
            if (c == CH_TAB)
                w = TABW - (x - xs) % TABW;
            else if (c == CH_CR)
                w = 0;
            else {
                w = CHW(c);
                if (c == ' ')
                    w = sp_w(w);
            }
            if (wm == 1 && (c == CH_CR || wx < x + w / 2)) {
                wo = o + i;
                return;
            }
            if (wm == 2 && !d) {
                if (!rn)
                    rx = x;
                run[rn++] = c;
            }
            last = o + i;
            x += w;
            ++i;
        }
        if (!i)
            break;
        o += i;
    }
    if (wm == 2)
        flush();
    wx = x;
    wo = (cs.l.f & LF_PARA) || e >= rem ? e : last;
}

static unsigned int FASTCALL x_of(unsigned int t)
{
    wm = 0;
    wt = t;
    swalk();
    return wx;
}

static unsigned int FASTCALL o_at(unsigned int x)
{
    wm = 1;
    wx = x;
    swalk();
    return wo;
}

/*---------------------------------------------------------------
  drawing
 ---------------------------------------------------------------*/
/* sub-line cs into the band from the row's pixel row by: its list's
   bullet or number, its text */
static void FASTCALL sub_draw(unsigned char by)
{
    static const struct pstyle *ps;
    static unsigned char k;
    wa = (int)cs.y + cs.l.a - by;
    fl_rr(cs.l.s);
    if ((unsigned int)(wa - fl_a + 100) > 200)
        return;
    ps = &par[cs.l.p];
    if ((cs.l.f & LF_FIRST) && IS_LIST(ps->kind)) {
        rr_x = bxs[cs.c] + TB_PADL + ps->left + ps->first - PG_L;
        if (ps->kind == PK_BULLET) {
            rr_x += 2;
            rr_top = wa - (fl_a + 1) / 3 - 2;
            rr_dot();
        } else {
            k = cs.num;
            rn = sizeof run - 1;
            run[rn] = '.';
            do {
                run[--rn] = (char)('0' + k % 10);
                k /= 10;
            } while (k);
            rr_s = run + rn;
            rr_n = sizeof run - rn;
            rr_top = wa - fl_a;
            rr_run();
        }
    }
    wm = 2;
    swalk();
}

void __fastcall__ e_tb_draw(unsigned char i)
{
    static unsigned char j, by, bh, be, k, h, top;
    static unsigned int y, yt, yb, x0;
    if (vr[i].y > TA_BOT)
        return;
    vrow(i);
    h = tc_h;
    mk_n = 0;
    if (sp_live) {
        if (bank23)
            mk_pre(i);                  /* (W58: the row's unknown words) */
        else {
            vr[i].mk = 0x80;
            sp_pend = 1;
        }
    }
    for (by = 0; by < h && ry + by <= TA_BOT; by = be) {
        bh = h - by > RBH ? RBH : h - by;
        be = by + bh;
        rr_bt = 0;
        rr_bh = bh;
        rr_clear();
        for (j = 0; j < sl_n; ++j) {
            top = tsl[j].y;
            if (top < be && top + tsl[j].l.h > by) {
                get(j);
                sub_draw(by);
            }
        }
        rr_y = ry + by;
        rr_lh = bh;
        rr_ylo = TA_TOP;
        rr_yhi = TA_BOT;
        rr_blit();
    }
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    yt = ry < TA_TOP ? TA_TOP : ry;
    yb = row_bot(i);
    if (!(tc_f & TR_NOGRID)) {
        WHorizontalLine(0xFF, bxs[0], bxs[tc_n], ry);
        y = ry + h - 1;
        if (tc_last && y <= TA_BOT)
            WHorizontalLine(0xFF, bxs[0], bxs[tc_n], y);
        for (k = 0; k <= tc_n; ++k)
            WVerticalLine(0xFF, bxs[k], yt, yb);
    }
    /* (W58) the marks: each word's sub-line, its x from a walk */
    for (k = 0; k + 1 < mk_n; k += 2) {
        j = tb_sub(mk_o[k]);
        if (j == 0xFF)
            continue;
        get(j);
        if (mk_o[k + 1] > cs.off + cs.l.len)
            continue;
        x0 = x_of(mk_o[k]);
        y = ry + cs.y + cs.l.a + 2;
        if (y <= yb && x_of(mk_o[k + 1]) > x0)
            WHorizontalLine(0xAA, x0, wx - 1, y);
    }
    if (s_lo < s_hi)
        e_tb_sel(i, s_lo, s_hi);
    if (cur_on && cy >= ry && cy <= yb)
        cur_on = 0;
}

/* the selection's part of row i inverted (both screens: the caller's dispBufferOn) */
void e_tb_sel(unsigned char i, pos_t lo, pos_t hi)
{
    static unsigned char j;
    static unsigned int a, e, l0, h0, x0, x1, y0, y1, xr, yb;
    if (lo >= vr[i].pos + vr[i].l.len || hi <= vr[i].pos)
        return;
    vrow(i);
    yb = row_bot(i);
    l0 = off(lo);
    h0 = off(hi);
    for (j = 0; j < sl_n; ++j) {
        get(j);
        a = cs.off;
        e = a + cs.l.len;
        if (l0 > a)
            a = l0;
        x1 = h0 < e ? h0 : e;
        if (a >= x1)
            continue;
        x0 = x_of(a);
        x1 = x_of(x1);
        if (h0 >= e && (cs.l.f & LF_PARA))
            x1 += 5;                    /* the paragraph's end selected */
        xr = bxs[cs.c + 1];
        if (x1 > xr)
            x1 = xr;
        y0 = ry + cs.y;
        y1 = y0 + cs.l.h - 1;
        if (y1 > yb)
            y1 = yb;
        if (x1 > x0 && y0 <= y1)
            WInvertRectangle(x0, y0, x1 - 1, y1);
    }
}

/*---------------------------------------------------------------
  the cursor, places
 ---------------------------------------------------------------*/
/* the sub-line holding p, a copy of it in cs (0: none) */
static unsigned char FASTCALL sub_at(pos_t p)
{
    static unsigned char j;
    j = tb_sub(off(p));
    if (j == 0xFF)
        return 0;
    get(j);
    return 1;
}

/* the cursor in row i: cx, cy, ch (cy past the text's area: not drawn) */
void __fastcall__ e_tb_cpos(unsigned char i)
{
    vrow(i);
    cy = 0xFFF;
    ch = 1;
    if (!sub_at(cur))
        return;
    cx = x_of(off(cur));
    cy = ry + cs.y + cs.l.a - fl_a;
    ch = fl_a + fl_d;
}

/* line i (a row) laid out */
static void FASTCALL lay(unsigned int i)
{
    static struct line l;
    lt_get(i, &l);
    at_row(lt_start(i), l.s);
}

unsigned int __fastcall__ e_tb_lx(unsigned int i, pos_t p)
{
    lay(i);
    return sub_at(p) ? x_of(off(p)) : TX0;
}

/* the place at x in the cell's sub-line at y (from the row's top: its
   last one above y if none holds it; 255: its last) */
static pos_t FASTCALL hit(unsigned int x, unsigned char y)
{
    static unsigned char c, j, b;
    for (c = 0; c + 1 < tc_n && x >= bxs[c + 1]; ++c)
        ;
    b = 0xFF;
    for (j = 0; j < sl_n; ++j) {
        if (tsl[j].c != c)
            continue;
        if (b == 0xFF || tsl[j].y <= y)
            b = j;
        if (y != 255 && y < tsl[j].y + tsl[j].l.h)
            break;
    }
    if (b == 0xFF)
        return pos_norm(rp);
    get(b);
    return pos_norm(rp + o_at(x));
}

pos_t __fastcall__ e_tb_lh(unsigned int i, unsigned int x)
{
    lay(i);
    return hit(x, tb_hy > 254 ? 254 : (unsigned char)tb_hy);
}

/* Up, Down, Home, End (t_arg: 1-4, bit 7 Shift) in a row or into one:
   t_arg 1 done */
static unsigned char sh;

static void FASTCALL go(pos_t p)
{
    cur = pos_norm(p);
    if (!sh)
        anc = cur;
    ty_kept = 0;
    ui_moved();
    t_arg = 1;
}

void tb_nav(void)
{
    static unsigned char k, in, m, up, c;
    static unsigned int L;
    static struct line l;
    k = t_arg & 0x7F;
    sh = t_arg & 0x80;
    up = k == 1;
    t_arg = 0;
    lt_find(cur);
    L = lf_i;
    lt_get(L, &l);
    in = IS_TR(&l);
    if (in) {
        at_row(lf_pos, l.s);
        if (!sub_at(cur))
            return;
        if (k >= 3) {                   /* Home, End: the sub-line's start, end */
            goal_x = k == 3 ? 0 : 0xFFFF;
            go(rp + (k == 3 ? cs.off : o_at(0xFFFF)));
            return;
        }
        if (goal_x == 0xFFFF)
            goal_x = x_of(off(cur));
        /* the cell's line above or below */
        for (m = cj, c = cs.c;;) {
            if (up ? !m : m + 1 >= sl_n)
                break;
            m += up ? -1 : 1;
            if (tsl[m].c == c) {
                get(m);
                go(rp + o_at(goal_x));
                return;
            }
        }
    } else if (k >= 3)
        return;
    if (up ? !L : L + 1 >= lt_n) {
        t_arg = in;                     /* (the document's first or last line: stays) */
        return;
    }
    if (goal_x == 0xFFFF)
        goal_x = line_x(L, cur);
    L += up ? -1 : 1;
    lt_get(L, &l);
    if (IS_TR(&l)) {
        lay(L);
        go(hit(goal_x, up ? 255 : 0));
    } else if (in)
        go(line_hit(L, goal_x));
}
