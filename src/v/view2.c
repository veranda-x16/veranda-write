/* Veranda Write on Veranda: the view, its larger part (overlay 5):
   the window, the whole view drawn, scrolling, what changed drawn again,
   the scroll bar, the status bar, the toolbar. view.c (main RAM) draws
   the rows and the cursor; the comment there says how.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <theme.h>
#include <vrwin.h>
#include "cwv.h"
#include "view.h"
#pragma code-name ("OVERLAY5")
#pragma rodata-name ("OVL5RO")

static char title[80];                  /* (main RAM: the kernel draws it) */

/* the title: the document's name, modified, the program */
static void v_title_text(void)
{
    char *p = title;
    const char *s = docname[0] ? docname : "Untitled";
    while (*s && p < title + 60)
        *p++ = *s++;
    if (!docname[0] && docname[1]) {    /* (a converted geoWrite document: Untitled-name) */
        *p++ = '-';
        for (s = docname + 1; *s && p < title + 60; )
            *p++ = *s++;
    }
    if (doc_modified) {
        *p++ = ' ';
        *p++ = '*';
    }
    s = " - Veranda Write";
    while (*s)
        *p++ = *s++;
    *p = 0;
}

/* row i has a page break's code (draw_row, through glue.s): a row of
   its own paragraph (no characters) gets dashes across the column, a
   little above its baseline */
extern unsigned char pb_row;

void __fastcall__ e_v_pbreak(unsigned char i)
{
    unsigned int y = vr[i].y + vr[i].l.a - 3;
    pb_row = 0;
    if (!vr[i].l.w && y >= TA_TOP && y <= TA_BOT)
        WHorizontalLine(0xF0, TX0, TX0 + wrap_w - 1, y);
}

void v_title(void)
{
    v_title_text();
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    vr_win_title(&win, title);
}

#pragma bss-name (push, "OVL5BSS")
static struct vrow ov[VMAX];            /* (the rows before an edit: this overlay's) */
static unsigned char ovn;
static unsigned char dirty[VMAX];
#pragma bss-name (pop)

/*---------------------------------------------------------------
  walking a line: x of a place, the place at an x (main RAM's view.c
  and overlay 1 call them through glue.s)
 ---------------------------------------------------------------*/
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

static struct line wl;                  /* (this overlay's: only it reads it) */

unsigned int __fastcall__ e_line_x(unsigned int i, pos_t p)
{
    lw_start = lt_start(i);
    lt_get(i, &wl);
    return e_walk(lw_start, &wl, 0, p);
}

pos_t __fastcall__ e_line_hit(unsigned int i, unsigned int x)
{
    lw_start = lt_start(i);
    lt_get(i, &wl);
    e_walk(lw_start, &wl, 1, x);
    return pos_norm(w_hit);
}

/* the selection's part of row i inverted (on the screens dispBufferOn says) */
void e_row_sel(unsigned char i, pos_t lo, pos_t hi)
{
    struct vrow *r = &vr[i];
    pos_t e = r->pos + r->l.len;
    unsigned int x0, x1;
    if (lo < r->pos)
        lo = r->pos;
    if (hi > e)
        hi = e;
    if (lo >= hi)
        return;
    x0 = e_walk(r->pos, &r->l, 0, lo);
    x1 = e_walk(r->pos, &r->l, 0, hi);
    if (hi == e && (r->l.f & LF_PARA))
        x1 += 5;                        /* the paragraph's end selected */
    if (x1 > PG_R)
        x1 = PG_R;
    if (x1 > x0)
        WInvertRectangle(x0, r->y, x1 - 1, row_bot(i));
}

/*---------------------------------------------------------------
  the scroll bar
 ---------------------------------------------------------------*/
/* (sbar.c, overlay 7, with the window chrome) */
static void scrollbar(void)
{
    callb(bank7, t_sbar);
}

/*---------------------------------------------------------------
  the whole view
 ---------------------------------------------------------------*/
void v_all(void)
{
    unsigned char i;
    view_cursor(0);
    clip(1);
    rows_build();
    s_lo = SEL_LO();
    s_hi = SEL_HI();
    for (i = 0; i < vn; ++i)
        draw_row(i);
    clear_below();
    clip(0);
    dm_on = 0;
    scrollbar();
    status_dirty = 1;
}

void v_frame(void)
{
    theme_load();
    if (!theme_none())
        theme_palette();
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    v_title_text();
    vr_win(&win, title);
    SetPattern(0);
    WRectangle(0, TA_TOP, 639, TA_END);
    ColorRect(0, PG_C0, TA_TOP >> 3, TA_ROWS, C_DESK);
    ColorRect(PG_C0 + PG_CW, SB_C - PG_C0 - PG_CW, TA_TOP >> 3, TA_ROWS, C_DESK);
    /* (Look 3: the page's edges are its colour's; no lines down the
       screen, no dithered shadow: each costs bytes on every line when
       Alt+Tab packs the screens, W39, and the 512 KB machine's big
       document has no bank to spare) */
    sb_a = 0xFF;
    view_toolbar();
    v_all();
}

/* n lines down (n > 0) or up, the rows moved with ScrollRect */
void v_scroll(void)
{
    int n = v_n;
    unsigned int dy, m, i;
    struct line l;
    unsigned char k;
    if (n > 0) {
        if (top_line + n >= lt_n)
            n = lt_n - 1 - top_line;
        if (n <= 0)
            return;
        if (n >= vn) {
            top_line += n;
            v_all();
            return;
        }
        view_cursor(0);
        dy = vr[n].y - TY0;
        clip(0);
        dispBufferOn = ST_WR_FORE | ST_WR_BACK;
        ScrollRect(PG_C0, PG_CW, TY0, TA_BOT, -(int)dy, 0);
        top_line += n;
        clip(1);
        rows_build();
        for (k = 0; k < vn; ++k)
            if (vr[k].y + vr[k].l.h - 1 > TA_BOT - dy)
                draw_row(k);
        clear_below();
    } else {
        m = -n;
        if (m > top_line)
            m = top_line;
        if (!m)
            return;
        dy = 0;
        for (i = 1; i <= m; ++i) {
            lt_get(top_line - i, &l);
            dy += l.h;
        }
        if (dy >= TA_BOT - TA_TOP - 16) {
            top_line -= m;
            v_all();
            return;
        }
        view_cursor(0);
        clip(0);
        dispBufferOn = ST_WR_FORE | ST_WR_BACK;
        ScrollRect(PG_C0, PG_CW, TY0, TA_BOT, dy, 0);
        top_line -= m;
        clip(1);
        rows_build();
        for (k = 0; k < vn && vr[k].y < TY0 + dy; ++k)
            draw_row(k);
        if (vn)
            draw_row(vn - 1);           /* (cut at the bottom now) */
        clear_below();
    }
    clip(0);
    dm_on = 0;
    scrollbar();
}

/* the cursor's line on the screen, whole */
void v_fix_cursor(void)
{
    unsigned int L, y;
    unsigned char i;
    lt_find(cur);
    L = lf_i;
    if (L < top_line) {
        if (top_line - L > 20) {
            top_line = L;
            v_all();
        } else
            { v_n = (int)L - (int)top_line; v_scroll(); }
        return;
    }
    if (L >= top_line + vn + 20) {
        top_line = L > 10 ? L - 10 : 0;
        v_all();
        return;
    }
    for (;;) {
        if (L < top_line + vn) {
            i = (unsigned char)(L - top_line);
            y = vr[i].y + vr[i].l.h - 1;
            if (y <= TA_BOT || i == 0)
                return;
            /* the rows to go: until it fits */
            for (i = 1; i < vn && y - (vr[i].y - TY0) > TA_BOT; ++i)
                ;
            { v_n = i; v_scroll(); }
            return;
        }
        { v_n = (int)(L - top_line - vn) + 1; v_scroll(); }
    }
}

/* old place q (before the edits) where it is now; 0xFFFFFFFF: edited */
static pos_t moved(pos_t q)
{
    if (q < dm_lo)
        return q;
    if (q >= (pos_t)((long)dm_hi - dm_delta))
        return (pos_t)((long)q + dm_delta);
    return 0xFFFFFFFFUL;
}

void v_refresh(void)
{
    static unsigned char i, j, k, i0, j0, nmove;
    static int dmove;
    pos_t lo = SEL_LO(), hi = SEL_HI(), e;
    view_cursor(0);
    if (dm_on) {
        memcpy(ov, vr, sizeof vr);
        ovn = vn;
        rows_build();
        memset(dirty, 1, vn);
        nmove = 0;
        for (i = 0; i < vn; ++i) {
            e = vr[i].pos + vr[i].l.len;
            if (vr[i].pos <= dm_hi && e >= dm_lo)
                continue;               /* edited */
            if ((s_lo < s_hi || lo < hi) && vr[i].pos < (hi > s_hi ? hi : s_hi) && e > (lo < s_lo ? lo : s_lo))
                continue;               /* (a selection there, before or now) */
            for (j = 0; j < ovn; ++j)
                if (moved(ov[j].pos) == vr[i].pos && ov[j].l.len == vr[i].l.len && ov[j].l.h == vr[i].l.h
                    && ov[j].l.a == vr[i].l.a && ov[j].l.s == vr[i].l.s && ov[j].l.f == vr[i].l.f
                    && ov[j].n == vr[i].n)
                    break;
            if (j == ovn || ov[j].y + ov[j].l.h - 1 > TA_BOT)
                continue;               /* (new on the screen, or cut before) */
            if (ov[j].y == vr[i].y)
                dirty[i] = 0;
            else if (!nmove) {
                nmove = 1;              /* the first row that only moved */
                i0 = i;
                j0 = j;
                dmove = (int)vr[i].y - (int)ov[j].y;
            }
        }
        clip(0);
        if (nmove) {
            /* that row and the ones after it that moved as much: ScrollRect */
            for (k = i0; k < vn && dirty[k]; ++k) {
                j = j0 + (k - i0);
                if (j >= ovn || moved(ov[j].pos) != vr[k].pos || ov[j].l.len != vr[k].l.len || ov[j].l.h != vr[k].l.h
                    || ov[j].l.a != vr[k].l.a || ov[j].l.s != vr[k].l.s || ov[j].l.f != vr[k].l.f || ov[j].n != vr[k].n
                    || (int)vr[k].y - (int)ov[j].y != dmove || ov[j].y + ov[j].l.h - 1 > TA_BOT)
                    break;
                if (vr[k].pos <= dm_hi && vr[k].pos + vr[k].l.len >= dm_lo)
                    break;
                dirty[k] = 0;
            }
            for (; k < vn; ++k)
                dirty[k] = 1;           /* (under the moved rows now) */
            for (k = 0; k < i0; ++k)
                if (vr[k].y + vr[k].l.h > ov[j0].y)
                    dirty[k] = 1;       /* (where the rows were) */
            dispBufferOn = ST_WR_FORE | ST_WR_BACK;
            if (dmove > 0)
                ScrollRect(PG_C0, PG_CW, ov[j0].y, TA_BOT, dmove, 0);
            else
                ScrollRect(PG_C0, PG_CW, vr[i0].y, TA_BOT, dmove, 0);
        }
        clip(1);
        s_lo = lo;
        s_hi = hi;
        for (i = 0; i < vn; ++i)
            if (dirty[i] || vr[i].y + vr[i].l.h - 1 > TA_BOT)
                draw_row(i);
        clear_below();
        clip(0);
        dm_on = 0;
        scrollbar();
    } else if (lo != s_lo || hi != s_hi) {
        /* the selection alone: the difference inverted, both screens */
        clip(1);
        dispBufferOn = ST_WR_FORE | ST_WR_BACK;
        for (i = 0; i < vn; ++i) {
            if (s_lo < s_hi && lo < hi && s_lo == lo) {
                e_row_sel(i, s_hi < hi ? s_hi : hi, s_hi < hi ? hi : s_hi);
            } else if (s_lo < s_hi && lo < hi && s_hi == hi) {
                e_row_sel(i, s_lo < lo ? s_lo : lo, s_lo < lo ? lo : s_lo);
            } else {
                if (s_lo < s_hi)
                    e_row_sel(i, s_lo, s_hi);
                if (lo < hi)
                    e_row_sel(i, lo, hi);
            }
        }
        clip(0);
        s_lo = lo;
        s_hi = hi;
    }
    if (follow) {
        follow = 0;
        v_fix_cursor();
    }
    if (status_dirty)
        view_status();
    view_cursor(1);
}

