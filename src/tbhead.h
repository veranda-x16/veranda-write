/* Veranda Write: a paragraph's table code (cw.h, docs/FORMAT.md 1.6),
   read from its first codes: compiled into table.c and tabedit.c (two
   overlays on the X16, each its own copy).

   The codes a paragraph starts with, in any order a program left them
   (its paragraph code first, as the format says; a character style code
   may be among them): the first table code is the paragraph's; a row's
   K_TROW is followed by its first cell's K_TCELL (its width; TB_DEFW
   when it has none).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
static unsigned char th_k, th_a, th_w, th_ro, th_co, th_po;

static void FASTCALL tb_head(pos_t q)
{
    static unsigned char n, c, o;
    th_k = 0;
    th_w = TB_DEFW;
    th_ro = th_co = 0xFF;
    th_po = 0;
    for (n = o = 0; n < 6 && q + 2 < tx.total; ++n, q += 3, o += 3) {
        c = cb_byte(&tx, q);
        if (!IS_KIND(c))
            break;
        if (c == K_PS && !n)
            th_po = 3;
        if (!IS_TKIND(c))
            continue;
        if (c == K_TROW) {
            if (th_k)
                break;
            th_k = c;
            th_ro = o;
            th_a = cb_byte(&tx, q + 1);
            continue;
        }
        if (c == K_TCELL) {
            th_co = o;
            if (th_k == K_TROW)
                th_w = cb_byte(&tx, q + 1);
            else {
                th_k = c;
                th_a = cb_byte(&tx, q + 1);
            }
        } else if (!th_k)
            th_k = c;
        break;
    }
    if (th_k == K_TCELL && !th_a)
        th_a = TB_DEFW;
    if (!th_w)
        th_w = TB_DEFW;
}
