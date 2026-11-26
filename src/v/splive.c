/* Veranda Write on Veranda: unknown words marked on the screen, live
   checking and Check spelling's search (overlay 23, with spell.c and
   v/sphash.s).

   Marks (W58): draw_row asks mk_pre for the row's words before it draws
   it: each looked up in the hash only (spell.c: no dictionary, no card);
   the unknown ones' starts and ends go to mk_o (main RAM), draw_row
   records their x as it draws (mk_rec, view.c), and mk_dots underlines
   them with a dotted line two rows under the baseline. A word not in
   the hash yet is left unmarked and the row noted (vr[i].mk bit 7,
   sp_pend); the marks drawn are counted in the row's entry (bits 0-6).
   So a row is drawn the same whether it is drawn after an edit or with
   all the others (W55's check): both read the same hash. While keys come
   (one in the last LIVE_PAUSE frames) mk_pre leaves the row unmarked and
   noted: the row being typed is looked at in the pause after it (W59);
   not while the view scrolls (v_scr, W73: the rows a scroll uncovers are
   marked as they are drawn, else each would be drawn twice).

   Live checking (sp_idle, from the main loop's idle pass when no key
   came for LIVE_PAUSE frames: sppop.c's sp_tick): the noted rows' words
   looked up (the dictionary, the card if need be), a key waiting stops
   it between two lookups (the rest later); a row whose marks are now
   more than it was drawn with is drawn again. The word the cursor is in
   (after its first letter) is not looked up: it is being typed; its row
   stays noted until the cursor leaves it.

   Check spelling's search (sp_next): every word from sp_from to the end,
   then from the start to sp_stop, through the hash (a word met again
   costs nothing) and the dictionary (the file open all the way).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "ovl23.h"                     /* (first: its names for spell.h's) */
#include "cwv.h"
#include "../spell.h"
#include "view.h"

unsigned char cw_esc(void);             /* (glue.s) Esc in the ROM's queue: taken */

#define MKMAX (MK_O / 2)
#define LIVE_PAUSE 8                    /* frames without a key before the words are looked at (ui.c's tick too) */

/* (draw_row's, before it draws row i) its words through the hash: the
   unknown ones' starts and ends (offsets in the row, in order) in mk_o,
   mk_n of them; vr[i].mk: their count, bit 7 a word not in the hash yet */
void __fastcall__ mk_pre(unsigned char i)
{
    static struct vrow *r;
    static unsigned char n, pend, res;
    r = &vr[i];
    r->mk = 0;
    if (r->y > TA_BOT || sp_nodic())
        return;
    if ((unsigned char)(cw_jiffies() - sp_kj) < LIVE_PAUSE && !v_scr) {
        r->mk = 0x80;                   /* (W59: typing: the row's words looked at in the pause after it) */
        sp_pend = 1;
        return;
    }
    sp_tokat(r->pos, r->pos + r->l.len);
    n = pend = 0;
    while (sp_tok()) {
        res = sp_probe();
        if (res == SP_NONE)
            pend = 0x80;
        else if (res == SP_UNKNOWN && !sp_abbr() && n < MKMAX) {
            mk_o[n << 1] = (unsigned int)(sp_ws - r->pos);
            mk_o[(n << 1) + 1] = (unsigned int)(sp_we - r->pos);
            ++n;
        }
    }
    r->mk = n | pend;
    mk_n = n << 1;
    if (pend)
        sp_pend = 1;
}

/* (draw_row's, after) the dotted lines two rows under the baseline,
   from each word's start to its end as drawn */
void __fastcall__ mk_dots(unsigned char i)
{
    static struct vrow *r;
    static unsigned int x0, x1, y;
    static unsigned char k;
    r = &vr[i];
    y = r->y + r->l.a + 2;
    if (y > r->y + r->l.h - 1)
        y = r->y + r->l.h - 1;
    if (y < TA_TOP || y > TA_BOT)
        return;
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    for (k = 0; k + 1 < mk_k; k += 2) {
        x0 = mk_x[k];
        x1 = mk_x[k + 1];
        if (x0 < PG_L)
            x0 = PG_L;
        if (x1 > PG_R)
            x1 = PG_R;
        if (x1 > x0)
            WHorizontalLine(0xAA, x0, x1 - 1, y);
    }
}

/* a key waiting, or (W73) live checking took its share of the pass (4
   frames: the main loop runs again; the rest the passes after) */
static unsigned char t0;

static unsigned char key_waiting(void)
{
    return (pressFlag & SET_KEYPRESS) || cw_keys() || (unsigned char)(cw_jiffies() - t0) >= 4;
}

/* a step of live checking: 1 when it looked something up or drew */
unsigned char sp_idle(void)
{
    static unsigned char i, n, pend, res, did, any;
    static struct vrow *r;
    static pos_t lastcur;
    t0 = cw_jiffies();
    if (!sp_live || sp_nodic()) {
        sp_pend = 0;
        return 0;
    }
    if (sp_pend == 2 && cur == lastcur)
        return 0;                       /* (only the word being typed left, the cursor in it still) */
    lastcur = cur;
    did = any = 0;
    for (i = 0; i < vn; ++i) {
        r = &vr[i];
        if (!(r->mk & 0x80))
            continue;
        if (r->y > TA_BOT) {
            r->mk = 0;
            continue;
        }
        sp_tokat(r->pos, r->pos + r->l.len);
        n = pend = 0;
        while (sp_tok()) {
            res = sp_probe();
            if (res == SP_NONE) {
                if (cur > sp_ws && cur <= sp_we) {
                    pend = 0x80;        /* (being typed: not yet) */
                    continue;
                }
                if (did && key_waiting()) {
                    sp_end();           /* (a key first: the rest later) */
                    return 1;
                }
                res = sp_check();
                did = 1;
                if (sp_nodic()) {
                    sp_pend = 0;        /* (W61: nothing said while typing; F7 says it) */
                    return 1;
                }
            }
            if (res == SP_UNKNOWN && !sp_abbr() && n < MKMAX)
                ++n;
        }
        if (n > (r->mk & 0x7F)) {
            clip(1);
            draw_row(i);                /* (mk_pre counts them again) */
            clip(0);
            if (!cur_on)
                view_cursor(1);
            did = 1;
        } else
            r->mk = (r->mk & 0x7F) | pend;
        if (r->mk & 0x80)
            any = 1;
        if (did && key_waiting())
            break;
    }
    sp_end();
    if (i == vn)
        sp_pend = any ? 2 : 0;          /* (2: only the cursor's word: not again till it moves) */
    return did;
}

unsigned char sp_next(void)
{
    static unsigned int k;
    if (!sp_ready())
        return 3;
    for (;;) {
        sp_tokat(sp_from, sp_wrap ? sp_stop : DOC_LEN);
        k = 0;
        while (sp_tok()) {
            if (sp_check() == SP_UNKNOWN && !sp_abbr()) {
                sp_from = sp_we;
                sp_end();
                return 1;
            }
            if (!(++k & 63) && cw_esc()) {
                sp_from = sp_tokpos();
                sp_end();
                return 2;
            }
        }
        if (sp_wrap || !sp_stop) {
            sp_end();
            return 0;
        }
        sp_wrap = 1;
        sp_from = 0;
    }
}
