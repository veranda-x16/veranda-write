/* Veranda Write on Veranda: the view (main RAM: it reads the text's
   banks while it draws).

   The window (vrwin.h, Veranda Look 2): the menu bar, the title band, a
   toolbar (the typing style: font, size, B I U), the page (a white
   column of 68 cards on the desk's colour, the text 520 pixels wide from
   x 64: 6.5 inches at 80 pixels an inch, as GEOS printed), the scroll bar,
   the status bar (page, line, column, modified).

   The rows on the screen (vr[]) are the line table's from top_line, with
   their places and their y. A row is drawn on the back screen (cleared,
   its runs of one style each by WPutString in that style's font and
   mode, the selection inverted) and then copied to the front
   (WRecoverRectangle): no flicker, and menus and dialog boxes find the
   back screen right. After edits, view_refresh draws only the rows the
   layout's damage touches; rows that only moved (a line more or less
   above them) are moved with ScrollRect instead of being drawn again.
   Scrolling (W73: view2.c, vscr.s, vscroll.s) moves the rows on the front
   screen by VERA's ports and FX cache, draws only the rows it uncovers, a
   few frames' worth a pass, and makes the back screen the front's when
   it stops. The
   cursor is a bar inverted on the front screen only, blinking; a
   selection's change without an edit inverts only the difference.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrwin.h>
#include "cwv.h"
#include "view.h"
#include "../spell.h"

struct vrow vr[VMAX];
unsigned int ta_bot = TA_END;
unsigned char vn;
unsigned int top_line;
pos_t s_lo, s_hi;                       /* the selection as drawn */
unsigned char cur_on;
static unsigned char blink;             /* cw_jiffies when the cursor was last shown or hidden */
unsigned int cx, ch;                    /* the cursor as drawn (tbview.c's in a table's row) */
unsigned int cy;                        /* (its top: view2.c's clear_below reads it) */
/* the cursor's x and its style's ascent and descent, found by draw_row
   when it drew the cursor's line (W43: view_cursor's walk along the line
   for them took 0.007 s): for the cursor at cc_cur in the row at cc_pos */
static pos_t cc_cur = 0xFFFFFFFFUL, cc_pos;
static unsigned int cc_x;
static unsigned char cc_a, cc_d, cc_i;  /* (cc_i: the row being drawn) */

static void __fastcall__ cc_set(unsigned int x)
{
    cc_x = x;
    cc_a = fl_a;
    cc_d = fl_d;
    cc_cur = cur;
    cc_pos = vr[cc_i].pos;
}

const struct vrwin win = { 0, 2, 80, 58, 2, 2, VW_CLOSE };
unsigned char sb_a = 0xFF, sb_n;        /* the scroll bar's thumb as drawn (view2.c) */
unsigned char sb_off;                   /* (W73) where the thumb was taken (sbar.c, view2.c) */
struct line wl;                         /* (vwalk.c's line_x and line_hit: e_walk, another overlay, reads it) */

/* a frame with its corners cut */
void __fastcall__ rbox(unsigned int l, unsigned int t, unsigned int r, unsigned int b)
{
    WHorizontalLine(0xFF, l + 1, r - 1, t);
    WHorizontalLine(0xFF, l + 1, r - 1, b);
    WVerticalLine(0xFF, l, t + 1, b - 1);
    WVerticalLine(0xFF, r, t + 1, b - 1);
}

/* the clip rectangle on the page's text area (Veranda's DECISIONS V81),
   or off and the text margins the whole screen again (WPutString keeps
   to them even with the clip off) */
void __fastcall__ clip(unsigned char on)
{
    if (on) {
        leftMargin = PG_L;
        rightMargin = PG_R;
        windowTopW = TA_TOP;
        windowBottomW = TA_BOT;
    } else {
        leftMargin = 0;
        rightMargin = 639;
        windowTopW = 0;
        windowBottomW = 479;
    }
    clipOn = on;
}

/*---------------------------------------------------------------
  the rows (rows_build: view2.c)
 ---------------------------------------------------------------*/
unsigned char pv_st[(VMAX + 7) / 8];    /* (pages.c's pg_view: which rows start a page) */
unsigned int pv_pg;

unsigned int tb_hy;                     /* (1.6) the y in the row (a table's: tbview.c's tb_lh) */

unsigned char __fastcall__ view_row(unsigned int y)
{
    unsigned char i;
    for (i = 0; i < vn; ++i)
        if (y < vr[i].y + vr[i].l.h) {
            tb_hy = y > vr[i].y ? y - vr[i].y : 0;
            return i;
        }
    return 0xFF;
}

/*---------------------------------------------------------------
  walking a line: x of a place, the place at an x
 ---------------------------------------------------------------*/
unsigned char lb[128];                  /* (cb_read's: main RAM) */
pos_t lw_start;

/* walk (view2.c, overlay 5): line l from p0, mode 0 the x of place t,
   mode 1 the place at x t (w_hit); its text's left edge and its
   justified spaces (line_geom) */
pos_t w_hit;
unsigned int xs;                        /* the line's text's left edge */
unsigned int sp_k;                      /* the spaces walked */

/* a space's width on the line (justified: its share of the slack) */
unsigned int __fastcall__ sp_w(unsigned int w)
{
    if (sp_k < lg_nsp) {
        w += lg_ex;
        if (sp_k < lg_rem)
            ++w;
    }
    ++sp_k;
    return w;
}

/*---------------------------------------------------------------
  drawing a row
 ---------------------------------------------------------------*/
/* (W58) the unknown words' marks: their starts and ends in the row
   (offsets from its place, in order: v/splive.c's mk_pre), their x as
   the row is drawn (mk_rec), then the dotted lines (mk_dots) */
unsigned char mk_n, mk_k;
unsigned int mk_nx, mk_o[MK_O], mk_x[MK_O];

static void __fastcall__ mk_rec(unsigned int x)
{
    mk_x[mk_k] = x;
    mk_nx = ++mk_k < mk_n ? mk_o[mk_k] : 0xFFFF;
}

char run[100];                          /* (a run of a row; v_pbreak's and the bands' text: main RAM) */

unsigned int __fastcall__ row_bot(unsigned char i)
{
    unsigned int b = vr[i].y + vr[i].l.h - 1;
    return b > TA_BOT ? TA_BOT : b;
}

/* (W73) rb_gen: counted by render.s's rr_clear and rr_tables, so a row
   the row bank still holds is known by it (view2.c's v_rows); v_rb: that
   row written again, not put together again; v_scr: the view scrolling */
unsigned char rb_gen, v_rb, v_scr;

/* row i drawn by render.s: the band of the line where its glyphs are
   (its fonts' tallest ascent and deepest descent: the line's height but
   the space before and after the paragraph, the line spacing's and the
   leading) put together in the row bank, then written to both screens
   with the line's other rows blank */
void __fastcall__ draw_row(unsigned char i)
{
    static struct vrow *r;
    static unsigned int x, rx, k, j;
    static pos_t p, e;
    static unsigned char s, n, c, a;
    static unsigned int cj, o;          /* the cursor's byte in the line (0xFFFF: not in it), p's */
    static const struct pstyle *ps;
    r = &vr[i];
    if (r->y > TA_BOT)
        return;
    if (IS_TR(&r->l)) {
        tb_draw(i);                     /* (1.6: a table's row, tbview.c) */
        return;
    }
    if (v_rb)
        goto blit;                      /* (W73: scrolling, the row the row bank holds: written again) */
    ps = &par[r->l.p];
    a = r->l.a;
    rr_bt = (r->l.f & LF_FIRST) ? ps->before : 0;
    k = r->l.h - rr_bt - ((r->l.f & LF_PARA) ? ps->after : 0);
    if (ps->spacing > 2)
        k = k * 2 / ps->spacing + 1;
    k -= LEADING;
    rr_bh = k > 48 ? 48 : k;
    rr_clear();
    a -= rr_bt;                         /* (the baseline's row in the band) */
    line_geom(&r->l, r->pos);
    x = xs = TX0 + lg_x0;
    sp_k = 0;
    p = r->pos;
    e = p + r->l.len;
    s = r->l.s;
    n = 0;
    cj = cur >= p && cur <= e ? (unsigned int)(cur - p) : 0xFFFF;
    cc_i = i;
    o = 0;
    mk_n = mk_k = 0;
    mk_nx = 0xFFFF;
    if (sp_live) {
        if (bank23) {
            mk_pre(i);                  /* (W58: the row's unknown words: overlay 23) */
            if (mk_n)
                mk_nx = mk_o[0];
        } else {
            r->mk = 0x80;               /* (spelling not loaded: when it is, W59) */
            sp_pend = 1;
        }
    }
    fl_rr(s);
    if ((r->l.f & LF_FIRST) && IS_LIST(ps->kind)) {
        /* a list's bullet or number, in the line's first style */
        rr_x = TX0 + ps->left + ps->first - PG_L;
        if (ps->kind == PK_BULLET) {
            rr_x += 2;
            rr_top = a - (fl_a + 1) / 3 - 2;
            rr_dot();
        } else {
            k = r->n;
            n = sizeof run - 1;
            run[n] = '.';
            do {
                run[--n] = (char)('0' + k % 10);
                k /= 10;
            } while (k);
            rr_s = run + n;
            rr_n = sizeof run - n;
            rr_top = a - fl_a;
            rr_run();
            n = 0;
        }
    }
    while (p < e) {
        k = cb_read(&tx, p, lb, e - p > sizeof lb ? sizeof lb : (unsigned int)(e - p));
        for (j = 0; j < k;) {
            c = lb[j];
            if (IS_KIND(c) || c == CH_TAB || c == CH_CR || (c == ' ' && lg_nsp) || n == sizeof run - 1) {
                if (n) {
                    rr_s = run;
                    rr_n = n;
                    rr_x = rx - PG_L;
                    rr_top = a - fl_a;
                    rr_run();
                    n = 0;
                }
                if (o + j == mk_nx)
                    mk_rec(x);
                if (IS_KIND(c)) {
                    if (j + 3 > k)
                        break;
                    if (c == K_CS) {
                        s = lb[j + 1];
                        fl_rr(s);
                    }
                    j += 3;
                    continue;
                }
                if (o + j == cj)
                    cc_set(x);          /* (the cursor's character: a tab, CR or space) */
                if (c == CH_TAB) {
                    x += TABW - (x - xs) % TABW;
                    ++j;
                    continue;
                }
                if (c == CH_CR) {
                    ++j;
                    continue;
                }
                if (c == ' ') {
                    x += sp_w(CHW(c));
                    ++j;
                    continue;
                }
            }
            if (o + j == cj)
                cc_set(x);              /* (the cursor's character) */
            if (o + j == mk_nx)
                mk_rec(x);
            if (!n)
                rx = x;
            run[n++] = c;
            x += CHW(c);
            ++j;
        }
        if (!j)
            break;
        p += j;
        o += j;
    }
    if (o == cj)
        cc_set(x);                      /* (the cursor at the line's end: the document's) */
    if (o == mk_nx)
        mk_rec(x);
    if (n) {
        rr_s = run;
        rr_n = n;
        rr_x = rx - PG_L;
        rr_top = a - fl_a;
        rr_run();
    }
blit:
    rr_y = r->y;
    rr_lh = r->l.h;
    rr_ylo = TA_TOP;
    rr_yhi = TA_BOT;
    rr_blit();
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    if (mk_k > 1)
        mk_dots(i);                     /* (W58: the unknown words marked: overlay 23) */
    if (r->l.f & LF_PB)
        v_pbreak(i);                    /* (a page break's line: overlay 5) */
    if (r->g)
        v_band(i);                      /* (the band above: overlay 14) */
    if (s_lo < s_hi)
        row_sel(i, s_lo, s_hi);
    if (cur_on && cy >= r->y && cy <= row_bot(i))
        cur_on = 0;                     /* (the screens written: the cursor's gone) */
}

/* (view2.c's clear_below: the paper under the last row; clr_y: the
   screens are the paper from there down, 0xFFFF not known) */
unsigned int clr_y = 0xFFFF;
unsigned int clr_pg;                    /* (its page: the footer under the last line) */
/* (W55) the bands' {pages}: hf_np, one drawn since v_all (hf.c sets
   it, v_all clears it), hf_tot the count v_all drew with; when the
   status bar's count is another, v_refresh draws all again (a footer's
   "Page 2 of 2" when there are 3) */
unsigned char hf_np;
unsigned int hf_tot;

/*---------------------------------------------------------------
  the cursor
 ---------------------------------------------------------------*/
static void cursor_xor(void)
{
    dispBufferOn = ST_WR_FORE;
    WInvertRectangle(cx, cy, cx + 1, cy + ch - 1);
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
}

void view_cursor(unsigned char on)
{
    unsigned int L;
    unsigned char i;
    static unsigned char a, d;
    if (cur_on) {
        clip(1);
        cursor_xor();
        clip(0);
        cur_on = 0;
    }
    if (!on || HAS_SEL())
        return;
    blink = cw_jiffies();               /* (W73: off the screen: tried again on the clock, */
    lt_find(cur);                       /*  not each pass of the main loop) */
    L = lf_i;
    if (L < top_line || L >= top_line + vn)
        return;
    i = (unsigned char)(L - top_line);
    if (IS_TR(&vr[i].l))
        tb_cpos(i);                     /* (1.6: in a table's row: cx, cy, ch) */
    else {
        if (cc_cur == cur && cc_pos == vr[i].pos) {
            cx = cc_x;                  /* (draw_row found it) */
            a = cc_a;                   /* (W55: not into fl_a and fl_d, which */
            d = cc_d;                   /*   are fl_last's: word wrap read them) */
        } else {
            cx = walk(vr[i].pos, &vr[i].l, 0, cur);
            a = fl_a;                   /* (walk: the style at the cursor) */
            d = fl_d;
        }
        cy = vr[i].y + vr[i].l.a - a;
        ch = a + d;
    }
    if (cx > PG_R - 2)
        cx = PG_R - 2;
    if (cy + ch - 1 > TA_BOT)
        return;
    clip(1);
    cursor_xor();
    clip(0);
    cur_on = 1;
}

/* the cursor blinks on the clock, not on main loop passes (DECISIONS
   W42): shown 32 frames (0.53 s), hidden 20 (0.33 s), however fast the
   loop runs (32 and 20 passes blinked several times a second on an
   8 MHz X16) */
void view_blink(void)
{
    if (HAS_SEL() || (unsigned char)(cw_jiffies() - blink) < (cur_on ? 32 : 20))
        return;
    if (!cur_on)
        view_cursor(1);
    else {
        clip(1);
        cursor_xor();
        clip(0);
        cur_on = 0;
        blink = cw_jiffies();
    }
}

/*---------------------------------------------------------------
  the status bar
 ---------------------------------------------------------------*/
char sl[64] = { 0 }, sr[40] = { 0 };   /* (DATA, main RAM: the window chrome reads them;
                                           view2.c's v_status writes them) */

/* the page and its line of the cursor's line, the pages: pages.c's
   (overlay 13); "~" while they are a guess (a document being opened, or
   not counted to its end yet) */
unsigned int pg_arg, pg_pc, pg_lc, pg_tot, pg_line;
unsigned char pg_est;
unsigned char pg_buf[36 * LT_SZ];       /* (pages.c's window of the line table: main RAM, cb_read's) */

/*---------------------------------------------------------------
  the rest of the view: overlay 5 (view2.c), through these
 ---------------------------------------------------------------*/
int v_n;
/* (view_all, view_frame, view_refresh, view_title, view_status,
   view_toolbar, view_fix_cursor: glue.s's far calls, 7 bytes each, 1.6) */
void __fastcall__ view_scroll(int n) { v_n = n; callb(bank5, v_scroll); }
