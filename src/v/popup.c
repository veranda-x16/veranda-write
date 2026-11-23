/* Veranda Write on Veranda: the toolbar's pop-ups (overlay 13).

   A click on the font, the size or the paragraph style's field opens
   its list under it: the Font, Size or Style menu's items (tools.c's
   menu_make makes them, as for the menu bar; an item's "\x1f" starts a
   group: a dotted line above it), each 16 lines high on whole cards,
   white, in a frame with a shadow, the item under the pointer in the
   selection colour. Chosen as the kernel's lists are (Veranda's V137): a
   press, a drag and a release on an item; or a click (released on the
   field: the list stays open) and a click on an item; a click outside
   it, or Esc, closes it. Drawn on the front screen only: what it covered
   comes back from the back screen, and the cards their colours.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#pragma code-name ("OVERLAY13")
#pragma rodata-name ("OVL13RO")
#pragma bss-name ("OVL13BSS")

static unsigned int pl, pr, pb;         /* the list: left, right, bottom (its top PU_Y) */
static unsigned char pn, hi;            /* items, the one lit (0xFF: none) */
#define PU_Y  (TB_Y + 16)

static void light(unsigned char i, unsigned char c)
{
    if (i != 0xFF)
        ColorRect(pl >> 3, (pr + 1 - pl) >> 3, (PU_Y >> 3) + 2 * i, 2, c);
}

/* the item under x, y (0xFF: none) */
static unsigned char at(unsigned int x, unsigned int y)
{
    if (x < pl || x > pr || y < PU_Y || y > pb)
        return 0xFF;
    return (unsigned char)((y - PU_Y) >> 4);
}

static void draw(void)
{
    unsigned char i;
    unsigned int y;
    const char *s;
    dispBufferOn = ST_WR_FORE;
    ColorRect(pl >> 3, (pr + 1 - pl) >> 3, PU_Y >> 3, pn << 1, 0);
    SetPattern(0);
    WRectangle(pl, PU_Y, pr, pb);
    WFrameRectangle(0xFF, pl, PU_Y, pr, pb);
    SetPattern(2);                      /* the shadow */
    WRectangle(pr + 1, PU_Y + 3, pr + 3, pb + 3);
    WRectangle(pl + 3, pb + 1, pr + 3, pb + 3);
    UseFont(FONT_UI);
    currentMode = 0;
    for (i = 0, y = PU_Y; i < pn; ++i, y += 16) {
        s = itext[i];
        if (*s == '\x1f') {
            ++s;
            WHorizontalLine(0x55, pl + 1, pr - 1, y);   /* (as the kernel's lists, V146) */
        }
        WPutString((void *)s, pl + 8, y + 11);
    }
    UseFont(0);
}

/* the pointer and the button: the item under it lit */
static unsigned int mx, my;
static void look(void)
{
    unsigned char i;
    do {
        mx = mouseXPos;
        my = mouseYPosW;
    } while (mx != mouseXPos || my != mouseYPosW);
    i = at(mx, my);
    if (i != hi) {
        light(hi, 0);
        light(i, C_THUMB);
        hi = i;
    }
    cw_idle = 1;                        /* (the gate's player goes on) */
}

static unsigned char down(void)
{
    return !(mouseData & 0x80);
}

/* t_arg: the bar's menu (4 Style, 6 Font, 7 Size), r11: the field's left
   (a card), r12: its width; t_arg then the item chosen (0xFF: none) */
void t_popup(void)
{
    unsigned char r = 0xFF, k, moved = 0;
    menu_bar = t_arg;
    CALL8(menu_make);
    pn = msub.flags & 0x1F;
    pl = r11;
    pr = pl + ((msub.right + 8 + 7) & ~7) - 1;
    if (pr < pl + r12 - 1)
        pr = pl + r12 - 1;
    pb = PU_Y + (pn << 4) - 1;
    hi = 0xFF;
    view_cursor(0);
    draw();
    /* press, drag, release on an item; or released on the field: the
       list stays till the next click */
    while (down()) {
        look();
        if (hi != 0xFF)
            moved = 1;
    }
    look();
    if (hi != 0xFF)
        r = hi;
    else if (!moved && my >= TB_Y && my < PU_Y && mx >= pl && mx < pl + r12) {
        for (;;) {
            look();
            k = GetNextChar();
            if (k == 0x1B)              /* (Esc) */
                break;
            if (down()) {
                while (down())
                    look();
                look();
                r = hi;
                break;
            }
        }
    }
    /* the cards' colours again (the ruler's and the page's paper, the
       desk left of the page), the shadow's too */
    ColorRect(pl >> 3, ((pr + 1 - pl) >> 3) + 1, PU_Y >> 3, (pn << 1) + 1, 0);
    if (pl < PG_L)
        ColorRect(pl >> 3, PG_C0 - (pl >> 3), (PU_Y >> 3) + 2, (pn << 1) - 1, C_DESK);
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    WRecoverRectangle(pl, PU_Y, pr + 3, pb + 3);
    t_arg = r;
}
