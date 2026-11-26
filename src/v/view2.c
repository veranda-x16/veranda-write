/* Veranda Write on Veranda: the view, its larger part (overlay 5):
   the window, the whole view drawn, scrolling, what changed drawn again,
   the scroll bar, the status bar, the toolbar. view.c (main RAM) draws
   the rows and the cursor; the comment there says how.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <theme.h>
#include <vrwin.h>
#include "cwv.h"
#include "../spell.h"
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

void v_title(void)
{
    v_title_text();
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    vr_win_title(&win, title);
}

#pragma bss-name (push, "OVL5BSS")
static struct vrow ov[VMAX];            /* (the rows before an edit: this overlay's) */

/* the rows made again (pgview.c): their spelling marks not known
   (W58: v/splive.c's live checking looks at them) */
static void rows(void)
{
    static unsigned char k;
    rows_build();
    for (k = 0; k < vn; ++k)
        vr[k].mk = 0x80;
    sp_pend = 1;
}

static pos_t mo[VMAX];                  /* (their places now: moved()) */
static unsigned char ovn;
static unsigned char dirty[VMAX];
#pragma bss-name (pop)

/*---------------------------------------------------------------
  the scroll bar
 ---------------------------------------------------------------*/
/* (sbar.c, overlay 7, with the window chrome) */
void vw_bar(void)
{
    callb(bank7, t_sbar);
}
#define scrollbar vw_bar

/*---------------------------------------------------------------
  the whole view
 ---------------------------------------------------------------*/
extern unsigned char vp_i, vp_e;        /* (W73, vscr.s) rows vp_i .. vp_e - 1 still to draw (v_more) */
void __fastcall__ v_draw(unsigned char k);  /* (vscr.s) row k drawn, perhaps from the row bank */
#pragma bss-name (push, "OVL5BSS")
static unsigned char vt;
#pragma bss-name (pop)
typedef char vrow_is_21[sizeof(struct vrow) == 21 ? 1 : -1];   /* (vscr.s's offsets) */
unsigned char v_more, v_stale, v_sj;     /* (main RAM: tick, the gate, ui.c, vscroll.s; v_sj: when it last scrolled) */
int v_acc;

/* the rows still to draw, a few frames' worth at a time (all: the rest,
   none from the row bank); then the paper under them, the status bar */
void __fastcall__ vw_rows(unsigned char all)
{
    vt = cw_jiffies();
    clip(1);
    v_scr = !all;
    while (vp_i < vp_e) {
        v_draw(vp_i++);
        if (!all && vp_i < vp_e && (unsigned char)(cw_jiffies() - vt) >= 3)
            break;
    }
    v_scr = 0;
    if (vp_i >= vp_e) {
        v_more = 0;
        clear_below();
        status_dirty = 1;
    } else
        need_view = 1;                  /* (the next pass draws on: the main loop runs between) */
    clip(0);
}

static void v_begin(void)
{
    view_cursor(0);
    clr_y = 0xFFFF;
    clip(1);
    rows();
    s_lo = SEL_LO();
    s_hi = SEL_HI();
    hf_np = 0;
    hf_tot = pg_tot;
    dm_on = 0;
    vp_i = 0;
    vp_e = vn;
}

void v_all(void)
{
    v_begin();
    vw_rows(1);
    scrollbar();
}

/* (W73) the rows TY0 .. TA_BOT moved by dy lines, those it uncovers
   made paper (dy 0: all of them): vscroll.s, on the front screen only
   while no menu is open (v_stale; v_sync makes the back screen the
   front's again once the scrolling stops) */
void __fastcall__ vw_move(int dy)
{
    clip(0);
    r13 = dy;
    callb(bank10, e_vw_move);
}

/* (W73) the view drawn again a pass at a time: scrolling by a screen or
   more (Page Down, the track, the thumb); the text area made paper
   first, so what is not drawn yet is not the page before */
void vw_page(void)
{
    v_begin();
    vw_move(0);
    v_more = 1;
    scrollbar();
    vw_rows(0);
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

/* (W73) v_scroll, v_draw: vscr.s (this overlay), for the room */

/* (W73) the scroll bar's thumb dragged (r11: the pointer's y; sbar.c's
   t_sbpress took it, sb_off below its top): the line at the thumb's top
   as the track's pixels share out the lines; the view scrolled there,
   once a pass of the main loop however far the pointer went */
#pragma bss-name (push, "OVL5BSS")
static unsigned int sb_y;
#pragma bss-name (pop)

void t_sbdrag(void)
{
    sb_y = r11 - sb_off;
    sb_y = sb_y < SB_UP + 1 || sb_y > 0x7FFF ? 0 : sb_y - (SB_UP + 1);
    sb_y = (unsigned int)((unsigned long)sb_y * lt_n / (SB_ROWS * 8));
    if (sb_y >= lt_n)
        sb_y = lt_n - 1;
    if (sb_y != top_line) {
        v_n = (int)sb_y - (int)top_line;
        v_scroll();
    }
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

/* old place q (before the edits) where it is now; 0xFFFFFFFF: edited
   (de_lo .. de_hi: the bytes edited since the last drawing, W43) */
static pos_t moved(pos_t q)
{
    if (q < de_lo)
        return q;
    if (q >= (pos_t)((long)de_hi - dm_delta))
        return (pos_t)((long)q + dm_delta);
    return 0xFFFFFFFFUL;
}

/* old row j is new row i: its place moved (mo[j]), the same line (its
   entry: its bytes, height, baseline, style, paragraph, width) and
   number. A row's drawing is all its own: its text, its entry, its number
   (W43) */
static unsigned char __fastcall__ same(unsigned char j, unsigned char i)
{
    static struct vrow *a, *b;          /* (W45: an index a field cost a multiplication) */
    a = &ov[j];
    b = &vr[i];
    return mo[j] == b->pos && !memcmp(&a->l, &b->l, sizeof(struct line)) && a->n == b->n
           && a->g == b->g && (!b->g || a->pg == b->pg);
}

/* what the edits changed drawn again. dirty[i]: 1 row i drawn, 0 on the
   screen as it is, 2 moved there by ScrollRect (drawn when it is cut at
   the bottom now). A row is drawn again when its bytes were edited, a
   selection was or is on it, or no old row was the same line; the old
   row at the same y is left as it is (W43: so the line before the edit,
   wrapped again as it was, the one after it and the one cut at the
   bottom are not drawn again: 4 or 5 rows a key were 1 or 2); rows that
   only moved go by ScrollRect, as many as moved by as much as the first */
void v_refresh(void)
{
    static unsigned char i, j, k, i0, j0, nmove;
    static int dmove;
    static pos_t q, slo, shi;
    pos_t lo = SEL_LO(), hi = SEL_HI(), e;
    view_cursor(0);
    if (v_more && dm_on)
        vw_rows(1);                      /* (W73: an edit: the rows drawn first, all of them) */
    if (v_acc) {
        v_n = v_acc;                    /* (W73: Page Up and Down, as many as came: one scroll) */
        v_acc = 0;
        if (dm_on) {
            if (v_n < 0 && (unsigned int)-v_n > top_line)
                v_n = -(int)top_line;
            top_line += v_n;
            if (top_line >= lt_n)
                top_line = lt_n - 1;
        } else
            v_scroll();
    } else if (v_more)
        vw_rows(0);                      /* (the rows still to draw: a few more) */
    if (hf_np && hf_tot != pg_tot)
        vw_page();                      /* (W55: a band's {pages} drawn with another count; W73: a pass at a time) */
    if (dm_on) {
        memcpy(ov, vr, sizeof vr);
        ovn = vn;
        for (j = 0; j < ovn; ++j)
            mo[j] = moved(ov[j].pos);
        rows();
        memset(dirty, 1, vn);
        PM("rows");
        nmove = 0;
        j = 0;
        slo = lo < s_lo ? lo : s_lo;    /* (a selection there, before or now) */
        shi = hi > s_hi ? hi : s_hi;
        if (s_lo >= s_hi && lo >= hi)
            shi = 0;
        for (i = 0; i < vn; ++i) {
            e = vr[i].pos + vr[i].l.len;
            if (vr[i].pos < de_hi && e > de_lo)
                continue;               /* edited */
            if (vr[i].pos < shi && e > slo)
                continue;
            /* the old row now at its place: the old rows' places, moved,
               only grow (the edited ones, 0xFFFFFFFF, are between those
               before the edits and those after them), as the rows' do, so
               one walk through them for all the rows (W43: a search
               through all of them for each row took 0.14 s a key) */
            while (j < ovn && ((q = mo[j]) == 0xFFFFFFFFUL || q < vr[i].pos))
                ++j;
            if (j == ovn || !same(j, i))
                continue;               /* (new on the screen) */
            vr[i].mk = ov[j].mk;        /* (W58: its marks as drawn) */
            if (ov[j].y == vr[i].y)
                dirty[i] = 0;           /* (as it is, cut at the bottom or not) */
            else if (ov[j].y + ov[j].l.h - 1 > TA_BOT)
                continue;               /* (cut before: drawn) */
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
                if (j >= ovn || !same(j, k) || (int)vr[k].y - (int)ov[j].y != dmove || ov[j].y + ov[j].l.h - 1 > TA_BOT)
                    break;
                e = vr[k].pos + vr[k].l.len;
                if ((vr[k].pos < de_hi && e > de_lo) || (vr[k].pos < shi && e > slo))
                    break;
                dirty[k] = 2;
            }
            for (; k < vn; ++k)
                dirty[k] = 1;           /* (under the moved rows now) */
            for (k = 0; k < i0; ++k)
                if (vr[k].y + vr[k].l.h > ov[j0].y - ov[j0].g)
                    dirty[k] = 1;       /* (where the rows were) */
            dispBufferOn = ST_WR_FORE | ST_WR_BACK;
            clr_y = 0xFFFF;
            if (dmove > 0)
                ScrollRect(PG_C0, PG_CW, ov[j0].y - ov[j0].g, TA_BOT, dmove, 0);
            else
                ScrollRect(PG_C0, PG_CW, vr[i0].y - vr[i0].g, TA_BOT, dmove, 0);
        }
        clip(1);
        s_lo = lo;
        s_hi = hi;
        PM("cmp");
        for (i = 0; i < vn; ++i)
            if (dirty[i] == 1 || (dirty[i] == 2 && vr[i].y + vr[i].l.h - 1 > TA_BOT)) {
                draw_row(i);
                PM("row");
                if (vr[i].y + vr[i].l.h > clr_y)
                    clr_y = 0xFFFF;     /* (W55: drawn over the paper clear_below left) */
            }
        clear_below();
        clip(0);
        dm_on = 0;
        PM("clr");
        scrollbar();
        PM("sbar");
    } else if (lo != s_lo || hi != s_hi) {
        /* the selection alone: the difference inverted, both screens */
        clip(1);
        dispBufferOn = ST_WR_FORE | ST_WR_BACK;
        for (i = 0; i < vn; ++i) {
            if (s_lo < s_hi && lo < hi && s_lo == lo) {
                row_sel(i, s_hi < hi ? s_hi : hi, s_hi < hi ? hi : s_hi);
            } else if (s_lo < s_hi && lo < hi && s_hi == hi) {
                row_sel(i, s_lo < lo ? s_lo : lo, s_lo < lo ? lo : s_lo);
            } else {
                if (s_lo < s_hi)
                    row_sel(i, s_lo, s_hi);
                if (lo < hi)
                    row_sel(i, lo, hi);
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
    PM("fix");
    view_cursor(1);                     /* (the status bar after it: ui.c's tick, W43) */
    PM("cur");
}

