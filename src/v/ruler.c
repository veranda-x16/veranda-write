/* Veranda Write on Veranda: the ruler (overlay 9, with the find
   panel): card rows 6-7 under the toolbar, white; the inches (numbered),
   the half and the eighth inches from the text's left edge to its right;
   the paragraph's indents: the first line's (down, at the top), the left
   one and the right one (up, at the bottom). tbar.c draws it when the
   paragraph's values changed.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include "cwv.h"
#pragma code-name ("OVERLAY9")
#pragma rodata-name ("OVL9RO")

static char rl[2] = { 0 };              /* (DATA, main RAM: the kernel draws it) */

static void __fastcall__ mark(unsigned int x, unsigned int y, signed char d)
{
    unsigned char j;
    for (j = 0; j < 4; ++j, y += d)
        WHorizontalLine(0xFF, x - 3 + j, x + 3 - j, y);
}

/* t_arg: the paragraph style */
void t_ruler(void)
{
    const struct pstyle *ps = &par[t_arg];
    unsigned int x, e = TX0 + wrap_w;
    unsigned char k, h;
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    SetPattern(0);
    WRectangle(0, RL_Y, 639, RL_Y + 14);
    WHorizontalLine(0xFF, 0, 639, RL_Y + 15);
    UseSystemFont();
    for (k = 0, x = TX0; x <= e; ++k, x += 10) {
        h = (k & 7) == 0 ? 6 : (k & 3) == 0 ? 3 : 1;
        WVerticalLine(0xFF, x, RL_Y + 14 - h, RL_Y + 14);
        if ((k & 7) == 0 && k) {
            rl[0] = (char)('0' + (k >> 3));
            WPutString(rl, x + 2, RL_Y + 8);
        }
    }
    mark(TX0 + ps->left + ps->first, RL_Y + 1, 1);
    mark(TX0 + ps->left, RL_Y + 14, -1);
    mark(e - ps->right, RL_Y + 14, -1);
}
