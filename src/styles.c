/* Veranda Write: the styles and the paragraphs: the style tables, the
   style in effect at a place, the typing style, a paragraph's start and
   style (overlay 13 on the X16: called through glue.s's wrappers; text.c
   has the text's places, main RAM). text.c's comment says how the text
   and its codes are made.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CW_HOST
#include "v/ovl2s.h"
#endif
#include <string.h>
#include "cw.h"

/*---------------------------------------------------------------
  the style table
 ---------------------------------------------------------------*/
unsigned char FASTCALL font_index(const char *name)
{
    unsigned char i;
    for (i = 0; i < nfont; ++i)
        if (!strcmp(fontname[i], name))
            return i;
    if (nfont >= MAXFONT)
        return 0xFF;
    strncpy(fontname[nfont], name, FONT_LEN - 1);
    fontname[nfont][FONT_LEN - 1] = 0;
    return nfont++;
}

unsigned char sty_find(unsigned char font, unsigned char size, unsigned char flags, unsigned char colour)
{
    unsigned char i;
    struct cstyle *s = sty;
    for (i = 0; i < nsty; ++i, ++s)
        if (s->font == font && s->size == size && s->flags == flags && s->colour == colour)
            return i;
    if (nsty >= MAXSTY)
        return 0xFF;
    s->font = font;
    s->size = size;
    s->flags = flags;
    s->colour = colour;
    return nsty++;
}

void sty_reset(void)
{
    nsty = 0;
    nfont = 0;
    font_index(def_font);
    sty_find(0, DEF_SIZE, 0, 0);
    fl_flush();
}

unsigned char FASTCALL par_find(const struct pstyle *p)
{
    unsigned char i;
    for (i = 0; i < npar; ++i)
        if (!memcmp(&par[i], p, sizeof *p))
            return i;
    if (npar >= MAXPAR)
        return 0xFF;
    memcpy(&par[npar], p, sizeof *p);
    return npar++;
}

void par_reset(void)
{
    npar = 0;
    par_find(&named[PK_BODY]);
}

/* style s with the change op v; 0xFF: no room for it */
unsigned char FASTCALL fmt_map(unsigned char s, unsigned char op, unsigned char v)
{
    struct cstyle c;
    memcpy(&c, &sty[s], sizeof c);
    switch (op) {
    case FMT_SET: c.flags |= v; break;
    case FMT_CLR: c.flags &= ~v; break;
    case FMT_FONT: c.font = v; break;
    case FMT_SIZE: c.size = v; break;
    case FMT_PLAIN: c.flags = 0; break;
    }
    return sty_find(c.font, c.size, c.flags, c.colour);
}

/* the start of the style codes straight before p */
pos_t FASTCALL codes_before(pos_t p)
{
    while (p >= 3 && cb_byte(&tx, p - 1) == K_CS)
        p -= 3;
    return p;
}

/* the paragraphs, through the line table: a paragraph's first line has
   LF_FIRST */
pos_t FASTCALL par_start(pos_t p)
{
    struct line l;
    unsigned int i;
    lt_find(p);
    i = lf_i;
    for (;;) {
        lt_get(i, &l);
        if ((l.f & LF_FIRST) || !i)
            break;
        --i;
    }
    return lt_start(i);
}

pos_t FASTCALL par_next(pos_t p)
{
    struct line l;
    unsigned int i;
    lt_find(p);
    for (i = lf_i; i < lt_n; ++i) {
        lt_get(i, &l);
        if (l.f & LF_PARA)
            return i + 1 < lt_n ? lt_start(i + 1) : 0xFFFFFFFFUL;
    }
    return 0xFFFFFFFFUL;
}

unsigned char FASTCALL par_at(pos_t q)
{
    return q + 2 < tx.total && cb_byte(&tx, q) == K_PS ? cb_byte(&tx, q + 1) : 0;
}

static unsigned char sbuf[64];

unsigned char FASTCALL style_at(pos_t p)
{
    struct line l;
    pos_t q;
    unsigned int n, i;
    unsigned char s, k;
    if (p > tx.total)
        p = tx.total;
    if (sa_ok && p == sa_pos)
        return sa_s;                    /* (W43: the last answer, the text as it was) */
    lt_find(p);
    lt_get(lf_i, &l);
    s = l.s;
    q = lf_pos;
    while (q < p) {
        n = cb_read(&tx, q, sbuf, p - q > sizeof sbuf ? sizeof sbuf : (unsigned int)(p - q));
        for (i = 0; i < n; ++i) {
            k = sbuf[i];
            if (IS_KIND(k)) {
                if (i + 2 >= n)         /* the code across the window's end */
                    break;
                if (k == K_CS)
                    s = sbuf[i + 1];
                i += 2;
            }
        }
        if (!i)
            break;
        q += i;
    }
    sa_pos = p;
    sa_s = s;
    sa_ok = 1;
    return s;
}

unsigned char FASTCALL style_typing(pos_t p)
{
    pos_t q = codes_before(p);
    if (q >= 3 && cb_byte(&tx, q - 1) == K_PS)
        q -= 3;                         /* (a paragraph's start) */
    if (q == 0 || cb_byte(&tx, q - 1) == CH_CR)
        return style_at(p);
    return style_at(q - 1);
}

