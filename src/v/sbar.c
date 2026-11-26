/* Veranda Write on Veranda: the scroll bar (overlay 7, with the window
   chrome). view2.c calls it after the view moved.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include "cwv.h"
#pragma code-name ("OVERLAY7")
#pragma rodata-name ("OVL7RO")

/* (Look 3) cards 78-79 of the text's area: an arrow box at each end
   (white), between them the track (the mist; plain, no line at its left:
   dots and lines down the screen pack badly at Alt+Tab, W39) and in it
   the thumb on whole card rows (white, a frame with
   cut corners, a grip of three lines when it is tall enough) */
#define SB_Y(a) (SB_UP + 1 + ((unsigned int)(a) << 3))

static void sb_track(unsigned char a, unsigned char n)
{
    SetPattern(0);
    WRectangle(SB_X + 1, SB_Y(a), 639, SB_Y(a + n) - 1);
    ColorRect(SB_C, 2, (SB_UP + 1) / 8 + a, n, C_BAR);
}

static void sb_arrow(unsigned int y, signed char d)
{
    unsigned char j;
    for (j = 0; j < 4; ++j)
        WHorizontalLine(0xFF, 632 - j, 632 + j, y + d * j);
}

void t_sbar(void)
{
    unsigned char a, n;
    unsigned int y, b;
    unsigned long t = lt_n;
    a = (unsigned char)((unsigned long)top_line * SB_ROWS / t);
    n = (unsigned char)((unsigned long)vn * SB_ROWS / t);
    if (n < 2)
        n = 2;
    if (a + n > SB_ROWS)
        a = SB_ROWS - n;
    if (a == sb_a && n == sb_n)
        return;
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    if (sb_a == 0xFF) {
        /* the whole bar: its line, the arrows' boxes */
        SetPattern(0);
        WRectangle(SB_X, TA_TOP, 639, TA_END);
        WHorizontalLine(0xFF, SB_X, 639, SB_UP);
        WHorizontalLine(0xFF, SB_X, 639, SB_DN);
        ColorRect(SB_C, 2, TA_TOP >> 3, 2, 0);
        ColorRect(SB_C, 2, (SB_DN + 1) >> 3, 2, 0);
        sb_arrow(TA_TOP + 5, 1);
        sb_arrow(TA_END - 5, -1);
        sb_track(0, SB_ROWS);
    } else
        sb_track(sb_a, sb_n);
    y = SB_Y(a);
    b = SB_Y(a + n) - 1;
    SetPattern(0);
    WRectangle(SB_X + 1, y, 639, b);
    ColorRect(SB_C, 2, (SB_UP + 1) / 8 + a, n, 0);
    rbox(SB_X + 2, y + 1, 638, b - 1);
    if (n > 2)
        for (y = (y + b) / 2 - 2; y <= (SB_Y(a) + b) / 2 + 2; y += 2)
            WHorizontalLine(0xFF, SB_X + 5, 635, y);
    sb_a = a;
    sb_n = n;
}

/* (W73) a press in the scroll bar (r11: y; ui.c's press): the arrows
   three lines, the track above or below the thumb a screen; on the
   thumb, t_arg 2: dragged (view2.c's t_sbdrag), held where it was taken */
void t_sbpress(void)
{
    t_arg = 0;
    if (r11 <= SB_UP)
        view_scroll(-3);
    else if (r11 >= SB_DN)
        view_scroll(3);
    else if (r11 < SB_Y(sb_a))
        view_scroll(1 - (int)vn);
    else if (r11 >= SB_Y(sb_a + sb_n))
        view_scroll(vn - 1);
    else {
        sb_off = (unsigned char)(r11 - SB_Y(sb_a));
        t_arg = 2;
    }
}
