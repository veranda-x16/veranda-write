/* Veranda Write on Veranda: the toolbar and the ruler (overlay 12,
   with the pager's card side: it had the room).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <theme.h>
#include "cwv.h"
#pragma code-name ("OVERLAY12")
#pragma rodata-name ("OVL12RO")
#pragma bss-name ("OVL12BSS")

static char tl[40] = { 0 };             /* (DATA, main RAM: the kernel draws it; cc65 puts an
                                           uninitialised one in the file's last BSS segment) */

static char *put_n(char *p, unsigned long v)
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

/* the toolbar (Veranda Look 3, Sunroom): pop-ups for the font, the size
   and the paragraph's style (a white field in a frame with rounded
   corners, a down arrow: a click opens its list, popup.c), buttons of 16
   x 16 pixels on whole cards (B I U; left, centre, right, justify; a
   bulleted, a numbered list; indent less, more), white, the ones that are
   on in the selection colour (Classic, no colours: inverted); under it
   the ruler (the inches, the half and eighth inches; the paragraph's
   indents: the first line's at the top, the left and the right at the
   bottom). Their places: cwv.h's TB_, RL_Y. Drawn again when what they
   show changed (the typing style's or the paragraph's values). */
static struct line tl_l;
static struct cstyle tb_c = { 0xFF };
static struct pstyle tb_q = { 0xFF };
static unsigned char tb_none;           /* the Classic theme: no colours */
static const char *const pk_name[PK_N] = {
    "Body", "Heading 1", "Heading 2", "Heading 3", "Quote", "Bulleted list", "Numbered list"
};
/* each alignment's four lines: their lengths (pixels) */
static const unsigned char al_len[4][4] = { { 10, 6, 10, 6 }, { 10, 6, 10, 6 }, { 10, 6, 10, 6 }, { 10, 10, 10, 10 } };

/* a field or a button: w / 8 cards from x in the toolbar's two card
   rows, white (on: the selection colour); done(): its frame 1 pixel
   inside them with its corners cut (after its text: a text clears the
   lines of its height) */
static void button(unsigned int x, unsigned char w, unsigned char on)
{
    SetPattern(0);
    WRectangle(x, TB_Y, x + w - 1, TB_Y + 14);
    ColorRect(x >> 3, w >> 3, TB_Y >> 3, 2, on ? C_THUMB : 0);
}

static void done(unsigned int x, unsigned char w, unsigned char on)
{
    rbox(x + 1, TB_Y + 1, x + w - 2, TB_Y + 14);
    if (on && tb_none)
        WInvertRectangle(x + 2, TB_Y + 2, x + 13, TB_Y + 13);
}

/* a pop-up: s in a field w wide, a down arrow at its right */
static void popup(unsigned int x, unsigned char w, const char *s)
{
    unsigned char j;
    button(x, w, 0);
    strncpy(tl, s, sizeof tl - 1);
    WPutString(tl, x + 6, TB_Y + 11);
    for (j = 0; j < 4; ++j)
        WHorizontalLine(0xFF, x + w - 13 + j, x + w - 7 - j, TB_Y + 6 + j);
    done(x, w, 0);
}

void t_toolbar(void)
{
    const struct cstyle *c;
    const struct pstyle *ps;
    unsigned int x, a;
    static const char bu[3] = { 'B', 0, 'U' };
    unsigned char k, j, n, on, pi;
    c = &sty[ty_style];
    lt_find(cur);                       /* (W43: the line's paragraph style, as par_at(par_start(cur)) */
    lt_get(lf_i, &tl_l);                /* without walking back to the paragraph's start) */
    pi = tl_l.p;
    ps = &par[pi];
    if (!memcmp(c, &tb_c, sizeof tb_c) && !memcmp(ps, &tb_q, sizeof tb_q))
        return;
    tb_none = theme_none();
    if (memcmp(ps, &tb_q, sizeof tb_q)) {
        t_arg = pi;
        CALL9(t_ruler);
    }
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    memcpy(&tb_c, c, sizeof tb_c);
    memcpy(&tb_q, ps, sizeof tb_q);
    UseFont(FONT_UI);
    currentMode = 0;
    popup(TB_FONT, TB_FONTW, fontname[c->font]);
    *put_n(tl, c->size) = 0;
    popup(TB_SIZE, TB_SIZEW, tl);
    popup(TB_NAME, TB_NAMEW, pk_name[ps->kind]);
    for (k = 0; k < 3; ++k) {
        x = TB_BIU + (k << 4);
        on = c->flags & (1 << k);
        button(x, 16, on);
        if (k == 1) {
            /* (an italic I with serifs, drawn: the UI font's italic I
               reads as a slash) */
            WHorizontalLine(0xFF, x + 7, x + 10, TB_Y + 4);
            WDrawLine(DRAW_INK, x + 9, TB_Y + 4, x + 6, TB_Y + 11);
            WHorizontalLine(0xFF, x + 4, x + 7, TB_Y + 11);
        } else {
            tl[0] = bu[k];
            tl[1] = 0;
            currentMode = k ? SET_UNDERLINE : SET_BOLD;
            WPutString(tl, x + 5 - !k, TB_Y + 11);
            currentMode = 0;
        }
        done(x, 16, on);
    }
    /* the alignment: four lines each */
    for (k = 0; k < 4; ++k) {
        x = TB_ALIGN + (k << 4);
        on = ps->align == k;
        button(x, 16, on);
        for (j = 0; j < 4; ++j) {
            n = al_len[k][j];
            a = k == PA_CENTRE ? x + 8 - n / 2 : k == PA_RIGHT ? x + 13 - n : x + 3;
            WHorizontalLine(0xFF, a, a + n - 1, TB_Y + 4 + 2 * j);
        }
        done(x, 16, on);
    }
    /* a bulleted list, a numbered one: a mark and two lines each */
    for (k = 0; k < 2; ++k) {
        x = TB_LIST + (k << 4);
        on = ps->kind == PK_BULLET + k;
        button(x, 16, on);
        for (j = 0; j < 3; ++j) {
            a = TB_Y + 4 + 3 * j;
            WHorizontalLine(0xFF, x + 3 + (k == 0), x + 4, a);
            WHorizontalLine(0xFF, x + 7, x + 12, a);
        }
        done(x, 16, on);
    }
    /* the indent: less, more (an arrow and the lines) */
    for (k = 0; k < 2; ++k) {
        x = TB_INDENT + (k << 4);
        button(x, 16, 0);
        for (j = 0; j < 4; ++j)
            WHorizontalLine(0xFF, x + 8, x + 12, TB_Y + 4 + 2 * j);
        for (j = 0; j < 3; ++j) {
            a = k ? x + 3 + j : x + 6 - j;
            WHorizontalLine(0xFF, a, a, TB_Y + 5 + j);
            WHorizontalLine(0xFF, a, a, TB_Y + 9 - j);
        }
        done(x, 16, 0);
    }
    UseFont(0);
}
