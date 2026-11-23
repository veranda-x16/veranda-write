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
   Scrolling moves the rows with ScrollRect and draws the new ones. The
   cursor is a bar inverted on the front screen only, blinking; a
   selection's change without an edit inverts only the difference.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrwin.h>
#include "cwv.h"
#include "view.h"

struct vrow vr[VMAX];
unsigned int ta_bot = TA_END;
unsigned char vn;
unsigned int top_line;
pos_t s_lo, s_hi;                       /* the selection as drawn */
unsigned char cur_on;
static unsigned char blink;             /* cw_jiffies when the cursor was last shown or hidden */
static unsigned int cx, cy, ch;         /* the cursor as drawn */

const struct vrwin win = { 0, 2, 80, 58, 2, 2, VW_CLOSE };
unsigned char sb_a = 0xFF, sb_n;        /* the scroll bar's thumb as drawn (view2.c) */

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
  the rows
 ---------------------------------------------------------------*/
void rows_build(void)
{
    unsigned int i = top_line, y = TY0, n;
    pos_t p;
    struct vrow *r = vr;
    if (top_line >= lt_n)
        top_line = i = lt_n - 1;
    p = lt_start(i);
    n = par_number(i);
    vn = 0;
    while (vn < VMAX && i < lt_n && y <= TA_BOT) {
        lt_get(i, &r->l);
        if (vn && (r->l.f & LF_FIRST))  /* (a numbered list's next: one more) */
            n = par[r->l.p].kind != PK_NUMBER ? 0 : n + 1;
        r->n = n;
        r->pos = p;
        r->y = y;
        p += r->l.len;
        y += r->l.h;
        ++i;
        ++vn;
        ++r;
    }
}

unsigned char __fastcall__ view_row(unsigned int y)
{
    unsigned char i;
    for (i = 0; i < vn; ++i)
        if (y < vr[i].y + vr[i].l.h)
            return i;
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
static char run[100];
unsigned char pb_row;                   /* the row has a page break's code */

unsigned int __fastcall__ row_bot(unsigned char i)
{
    unsigned int b = vr[i].y + vr[i].l.h - 1;
    return b > TA_BOT ? TA_BOT : b;
}

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
    static const struct pstyle *ps;
    r = &vr[i];
    if (r->y > TA_BOT)
        return;
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
                if (IS_KIND(c)) {
                    if (j + 3 > k)
                        break;
                    if (c == K_CS) {
                        s = lb[j + 1];
                        fl_rr(s);
                    } else if (c == K_PB)
                        pb_row = 1;
                    j += 3;
                    continue;
                }
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
            if (!n)
                rx = x;
            run[n++] = c;
            x += CHW(c);
            ++j;
        }
        if (!j)
            break;
        p += j;
    }
    if (n) {
        rr_s = run;
        rr_n = n;
        rr_x = rx - PG_L;
        rr_top = a - fl_a;
        rr_run();
    }
    rr_y = r->y;
    rr_lh = r->l.h;
    rr_ylo = TA_TOP;
    rr_yhi = TA_BOT;
    rr_blit();
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    if (pb_row)
        v_pbreak(i);                    /* (a page break's line: overlay 5) */
    if (s_lo < s_hi)
        row_sel(i, s_lo, s_hi);
    if (cur_on && cy >= r->y && cy <= row_bot(i))
        cur_on = 0;                     /* (the screens written: the cursor's gone) */
}

/* below the last row: the page's paper */
void clear_below(void)
{
    unsigned int y = vn ? vr[vn - 1].y + vr[vn - 1].l.h : TY0;
    if (y > TA_BOT)
        return;
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    SetPattern(0);
    WRectangle(PG_L, y, PG_R, TA_BOT);
    if (cur_on && cy >= y)
        cur_on = 0;
}

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
    if (cur_on) {
        clip(1);
        cursor_xor();
        clip(0);
        cur_on = 0;
    }
    if (!on || HAS_SEL())
        return;
    lt_find(cur);
    L = lf_i;
    if (L < top_line || L >= top_line + vn)
        return;
    i = (unsigned char)(L - top_line);
    cx = walk(vr[i].pos, &vr[i].l, 0, cur);
    if (cx > PG_R - 2)
        cx = PG_R - 2;
    cy = vr[i].y + vr[i].l.a - fl_a;    /* (walk: the style at the cursor) */
    ch = fl_a + fl_d;
    if (cy + ch - 1 > TA_BOT)
        return;
    clip(1);
    cursor_xor();
    clip(0);
    cur_on = 1;
    blink = cw_jiffies();
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
static char sl[64] = { 0 }, sr[40] = { 0 };   /* (DATA, main RAM: the window chrome reads them) */

static char *put_u2(char *p, unsigned long v)
{
    char b[11];
    unsigned char n = 0;
    do {
        b[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        *p++ = b[--n];
    return p;
}

static char *put_s2(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    return p;
}

/* the page and its line of the cursor's line, the pages: pages.c's
   (overlay 5); "~" while they are a guess (a document being opened, or
   not counted to its end yet) */
unsigned int pg_arg, pg_pc, pg_lc, pg_tot, pg_line;
unsigned char pg_est;
unsigned char pg_buf[36 * LT_SZ];       /* (pages.c's window of the line table: main RAM, cb_read's) */

static unsigned char t_mod = 0xFF;      /* the title band's "modified" */

void view_status(void)
{
    char *p = sl;
    unsigned int col = 0;
    pos_t q;
    lt_find(cur);
    q = pos_norm(lf_pos);               /* (past the codes the line starts with) */
    pg_arg = lf_i;
    pg_at();
    while (q < cur && q < DOC_LEN) {    /* the column: characters from the line's start (W41: bounded) */
        q = pos_next(q);
        ++col;
    }
    p = put_s2(p, pg_est & 1 ? "Page ~" : "Page ");
    p = put_u2(p, pg_pc);
    p = put_s2(p, pg_est & 2 ? " of ~" : " of ");
    p = put_u2(p, pg_tot);
    if (ld_kind)
        p = put_s2(p, "...");
    p = put_s2(p, "   Line ");
    p = put_u2(p, pg_lc);
    p = put_s2(p, "   Col ");
    p = put_u2(p, col + 1);
    if (HAS_SEL()) {
        p = put_s2(p, "   ");
        p = put_u2(p, SEL_HI() - SEL_LO());
        p = put_s2(p, " selected");
    }
    *p = 0;
    p = sr;
    if (status_msg[0]) {
        p = put_s2(p, status_msg);
        p = put_s2(p, "   ");
    }
    p = put_s2(p, doc_modified ? "Modified" : "Saved");
    *p = 0;
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    vr_win_status(&win, sl, sr);
    if (doc_modified != t_mod) {
        t_mod = doc_modified;
        view_title();
    }
    status_dirty = 0;
}



/*---------------------------------------------------------------
  the rest of the view: overlay 5 (view2.c), through these
 ---------------------------------------------------------------*/
int v_n;
void view_all(void) { callb(bank5, v_all); }
void view_frame(void) { callb(bank5, v_frame); }
void view_refresh(void) { callb(bank5, v_refresh); }

void view_title(void) { callb(bank5, v_title); }
void view_toolbar(void) { callb(bank12, t_toolbar); }
void view_fix_cursor(void) { callb(bank5, v_fix_cursor); }
void __fastcall__ view_scroll(int n) { v_n = n; callb(bank5, v_scroll); }
