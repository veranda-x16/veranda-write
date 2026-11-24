/* Veranda Write: the page setup (CWRD 1.4's PAGE chunk, docs/FORMAT.md;
   DECISIONS W44): the paper, the margins, the header and the footer.

   The sizes are twips (1/1440 inch, as the file keeps them): the X16's
   screen has 80 pixels an inch, a pixel 18 twips, so the text's width
   (wrap_w) is the paper's width less the left and right margins, and a
   page's text (pg_th, the pages' walk: v/pages.c) its height less the top
   and bottom ones. The header and the footer are in the top and bottom
   margins: they take nothing from the text.

   On the X16: overlay 14 (pgs and pg_th in main RAM: the file's code in
   overlay 2, the pages' in 13, the view and the PDF read them).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cw.h"
#ifdef CW_HOST
struct pgset pgs;                       /* (the X16's: main.c, main RAM) */
unsigned int pg_th = 778;
#else
#pragma code-name ("OVERLAY14")
#pragma rodata-name ("OVL14RO")
#pragma data-name ("OVL14DATA")
#define pgs_default e_pgs_default
#define pg_apply    e_pg_apply
#define pgs_paper   e_pgs_paper
#define ld_safe     e_ld_safe
#endif

/* A4 (210 x 297 mm) and US Letter (8.5 x 11 inches); 2.5 cm and 1 inch */
static const unsigned short paper_w[2] = { 11906, 12240 }, paper_h[2] = { 16838, 15840 };
static const unsigned short paper_m[2] = { 1417, 1440 };

struct pgset pgs_def = { PP_A4, 0, 11906, 16838, 1417, 1417, 1417, 1417, "", "" };

void FASTCALL pgs_paper(unsigned char k)
{
    pgs.paper = k;
    pgs.w = paper_w[k];
    pgs.h = paper_h[k];
}

/* a and b (margins) leave at least min twips of the paper's s: else the
   paper's own */
static void FASTCALL fit(unsigned short *a, unsigned short *b, unsigned short s, unsigned short min)
{
    if (*a > 14400 || *b > 14400 || (unsigned long)*a + *b + min > s)
        *a = *b = paper_m[pgs.paper == PP_LETTER];
}

void pg_apply(void)
{
    static unsigned int w;
    if (pgs.paper > PP_OTHER || pgs.w < 4 * 1440 || pgs.h < 4 * 1440 || pgs.w > 30000 || pgs.h > 30000)
        pgs_paper(PP_A4);
    fit(&pgs.left, &pgs.right, pgs.w, TXW_MIN * TW_PX);
    fit(&pgs.top, &pgs.bottom, pgs.h, TXH_MIN * TW_PX);
    pgs.head[HF_MAX] = pgs.foot[HF_MAX] = 0;
    w = (pgs.w - pgs.left - pgs.right) / TW_PX;
    wrap_w = w > TXW_MAX ? TXW_MAX : w;
    pg_th = (pgs.h - pgs.top - pgs.bottom) / TW_PX;
}

void pgs_default(void)
{
    memcpy(&pgs, &pgs_def, PGS_N);
    pg_apply();
}

/* (format.c's: overlay 2 had no room for it, W44) */
/* an edit up to p is safe while the rest is still to come: a paragraph
   ends after p before the last line there is (the rest cannot change
   it, nor it the rest) */
unsigned char FASTCALL ld_safe(pos_t p)
{
    static unsigned int i, k;
    static struct line l;
    if (p == 0xFFFFFFFFUL || p >= tx.total)
        return 0;
    lt_find(p);
    for (i = lf_i, k = 0; i + 1 < lt_n && k < 200; ++i, ++k) {
        lt_get(i, &l);
        if (l.f & LF_PARA)
            return 1;
    }
    return 0;
}

typedef char pgs_size[sizeof(struct pgset) == PGS_N ? 1 : -1];   /* (the file's 142 bytes) */
