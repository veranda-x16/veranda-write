/* Veranda Write on Veranda: the status bar (overlay 8, which has the
   room): the page, the line, the column, the selection, a message,
   modified or saved. view.c's view_status calls it, ui.c's tick after
   the text and the cursor are drawn (DECISIONS W43).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <vrwin.h>
#include "cwv.h"
#include "view.h"
#pragma code-name ("OVERLAY8")
#pragma rodata-name ("OVL8RO")
#pragma bss-name ("OVL8BSS")

extern char sl[64], sr[40];             /* (main RAM: the window chrome reads them) */
extern unsigned char lb[128];           /* (view.c: main RAM, cb_read's) */

static char *put_u(char *p, unsigned long v)
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

static char *put_s(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    return p;
}

static unsigned char t_mod = 0xFF;      /* the title band's "modified" */

/* the characters (not the codes) from q to e: a window at a time (W43:
   pos_next a character took 0.03 s for a column of 80) */
static unsigned int __fastcall__ chars_to(pos_t q, pos_t e)
{
    static unsigned int n, i, k;
    n = 0;
    if (e > DOC_LEN)
        e = DOC_LEN;                    /* (W41: bounded) */
    while (q < e) {
        k = cb_read(&tx, q, lb, e - q > sizeof lb ? sizeof lb : (unsigned int)(e - q));
        for (i = 0; i < k;) {
            if (IS_KIND(lb[i])) {
                if (i + 3 > k)
                    break;              /* (a code across the window's end) */
                i += 3;
            } else {
                ++n;
                ++i;
            }
        }
        if (!i)
            break;
        q += i;
    }
    return n;
}

void t_status(void)
{
    char *p = sl;
    unsigned int col;
    lt_find(cur);
    pg_arg = lf_i;
    col = chars_to(lf_pos, cur);        /* the column: characters from the line's start */
    pg_at();
    p = put_s(p, pg_est & 1 ? "Page ~" : "Page ");
    p = put_u(p, pg_pc);
    p = put_s(p, pg_est & 2 ? " of ~" : " of ");
    p = put_u(p, pg_tot);
    if (ld_kind)
        p = put_s(p, "...");
    p = put_s(p, "   Line ");
    p = put_u(p, pg_lc);
    p = put_s(p, "   Col ");
    p = put_u(p, col + 1);
    if (HAS_SEL()) {
        p = put_s(p, "   ");
        p = put_u(p, SEL_HI() - SEL_LO());
        p = put_s(p, " selected");
    }
    *p = 0;
    p = sr;
    if (status_msg[0]) {
        p = put_s(p, status_msg);
        p = put_s(p, "   ");
    }
    p = put_s(p, doc_modified ? "Modified" : "Saved");
    *p = 0;
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    vr_win_status(&win, sl, sr);
    if (doc_modified != t_mod) {
        t_mod = doc_modified;
        view_title();
    }
    status_dirty = 0;
}
