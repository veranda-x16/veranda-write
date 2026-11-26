/* Veranda Write on Veranda: a table's keys (overlay 28, with tabedit.c;
   DECISIONS W62-W64), before ui.c's own (tb_key): in a cell Tab goes to
   the next cell (in the last one it adds a row), Shift+Tab to the one
   before; Return makes a further paragraph of the cell; Backspace at a
   cell's start and Delete at its end do nothing (they would join two
   cells), Backspace after a table goes into its last cell; Up, Down,
   Home and End go by the cell's lines (table.c's sub-lines: tbview.c,
   overlay 27); a deletion that would break a table (a selection over
   two cells, or from outside into one) is refused; typing stops when a
   row has TB_ROWMAX bytes.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "ovl28.h"                       /* (tabedit.c's, called straight) */
#include "cwv.h"
extern unsigned char lb[128];           /* (view.c's: main RAM) */
#include "view.h"
#include "../spell.h"
#pragma code-name ("OVERLAY28")
#pragma rodata-name ("OVL28RO")
#pragma bss-name ("OVL28BSS")


static void FASTCALL go(pos_t p)
{
    cur = anc = pos_norm(p);
    ty_kept = 0;
    goal_x = 0xFFFF;
    ui_moved();
}

static void after(void)
{
    ul_break();
    go(tb_at);
    need_view = 1;
    status_dirty = 1;
    follow = 1;
}

static void FASTCALL said(unsigned char e)
{
    if (e == DE_MEM)
        message("There is not enough memory", "for that.");
    else if (e == DE_PAR)
        message("The document has too many paragraph", "styles (32 at most).");
    doc_err = 0;
}

/*---------------------------------------------------------------
  keys
 ---------------------------------------------------------------*/
void tb_key(void)
{
    static unsigned char k, m, sh, in, r;
    static struct line l;
    k = t_arg;
    m = keyDataMods;
    sh = m & 1;
    t_arg = 0;
    if (m & KEYM_ARROW) {
        if (k == KEY_UP || k == KEY_DOWN) {
            t_arg = (k == KEY_UP ? 1 : 2) | (sh << 7);
            callb(bank27, tb_nav);
        }
        return;
    }
    if (m & 2)
        return;                         /* (Ctrl: ui.c's) */
    if (k == KEY_HOME || k == KEY_CLEAR || k == KEY_END) {
        t_arg = (k == KEY_END ? 4 : 3) | ((sh || k == KEY_CLEAR) << 7);
        callb(bank27, tb_nav);
        return;
    }
    lt_find(cur);
    lt_get(lf_i, &l);
    in = IS_TR(&l);
    if (HAS_SEL() && (k == 8 || k == KEY_DELETE || k == CR || k == 0x8D || (k == KEY_TAB && !in)
                      || (k >= 32 && k < 127) || k >= 160) && !tb_delok(SEL_LO(), SEL_HI())) {
        set_status("Select in one cell, or all the table");
        t_arg = 1;
        return;
    }
    switch (k) {
    case KEY_TAB:
        if (!in)
            return;
        t_arg = 1;
        if (tb_next(cur, !sh)) {
            go(tb_at);
            return;
        }
        if (sh)
            return;
        ul_step(U_CMD, cur, anc);       /* (the last cell: a row more) */
        said(tb_addrow(cur, 1));
        after();
        return;
    case CR:
    case 0x8D:
        if (!in)
            return;
        t_arg = 1;
        if (l.len >= TB_ROWMAX) {
            set_status("This row of the table is full");
            return;
        }
        ul_step(U_TYPE, cur, anc);
        if (HAS_SEL()) {
            ty_style = style_typing(SEL_LO());
            cur = anc = doc_delete(SEL_LO(), SEL_HI());
        }
        said(tb_return(cur, ty_style));
        after();
        return;
    case 8:
        if (HAS_SEL())
            return;
        r = tb_bs(cur);
        if (r == 1)
            return;
        if (r == 2)
            go(tb_at);
        t_arg = 1;
        return;
    case KEY_DELETE:
        if (!HAS_SEL() && !tb_del(cur))
            t_arg = 1;
        return;
    }
    if (in && l.len >= TB_ROWMAX && ((k >= 32 && k < 127) || k >= 160)) {
        set_status("This row of the table is full");
        t_arg = 1;
    }
}


/* (v/tbcmd.c's, overlay 29) the edit t_arg at the cursor (TD_), tr_w its
   numbers (tr_n of them: copied first, tb_row writes them); t_arg back:
   its result (TD_ROW: in a row, tr_ the row's) */
void tb_do(void)
{
    static unsigned char w[TB_MAXC], n, op;
    memcpy(w, tr_w, sizeof w);
    n = tr_n;
    op = t_arg;
    tb_at = cur;
    switch (op) {
    case TD_INSERT: op = tb_insert(cur, w[1], w[0]); break;
    case TD_ROWUP: case TD_ROWDN: op = tb_addrow(cur, op == TD_ROWDN); break;
    case TD_COLL: case TD_COLR: op = tb_addcol(cur, op == TD_COLR); break;
    case TD_DELROW: op = tb_delrow(cur); break;
    case TD_DELCOL: op = tb_delcol(cur); break;
    case TD_TABLE: op = tb_deltable(cur); break;
    case TD_WIDTHS: op = tb_widths(cur, w, n); break;
    case TD_GRID: op = tb_grid(cur, w[0]); break;
    case TD_ROWAT: op = tb_row(tb_p); break;
    case TD_USED: tb_used(iobuf + 160); break;
    default: op = tb_row(cur); break;
    }
    t_arg = op;
}

/* (pdf.c's row, overlay 15's room) the row's cells' boxes as PDF
   content in iobuf: lb[1] cells, their borders at lb + 4 (pixels), at
   lb + 24 x0, the boxes' bottom, their height (thousandths of a point);
   its bytes */
static unsigned int on;

static void FASTCALL put(const char *s)
{
    while (*s)
        iobuf[on++] = *s++;
}

/* v thousandths as points, up to 3 decimals, a space after */
static void FASTCALL num(long v)
{
    static char d[12];
    static unsigned char k, f;
    static unsigned int r;
    if (v < 0) {
        iobuf[on++] = '-';
        v = -v;
    }
    r = (unsigned int)(v % 1000);
    v /= 1000;
    k = sizeof d;
    do {
        d[--k] = (char)('0' + (unsigned char)(v % 10));
        v /= 10;
    } while (v);
    while (k < sizeof d)
        iobuf[on++] = d[k++];
    if (r) {
        iobuf[on++] = '.';
        for (f = 0; f < 3 && r; ++f) {
            iobuf[on++] = (char)('0' + r / 100);
            r = r % 100 * 10;
        }
    }
    iobuf[on++] = ' ';
}

unsigned int e_pdf_boxes(void)
{
    static long v[3];
    static unsigned int *x;
    static unsigned char k;
    memcpy(v, lb + 24, sizeof v);
    on = 0;
    put("0.5 w\n");
    for (k = lb[1], x = (unsigned int *)(lb + 4); k; --k, ++x) {
        num(v[0] + (long)x[0] * 900);
        num(v[1]);
        num((long)(x[1] - x[0]) * 900);
        num(v[2]);
        put("re\n");
    }
    put("S\n");
    return on;
}
