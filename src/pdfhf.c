/* Veranda Write: the PDF's header and footer (DECISIONS W46, W47): part
   k (0 left, 1 centre, 2 right) of the header (pdf_hfw 0) or the footer
   (1) of page pdf_hfp of pdf_hfn, as PDF content (a text object in
   Helvetica 9) at the start of iobuf; its bytes. Part 0 makes the line
   (hf.c's hf_make); a part that is empty is no bytes. The header's
   baseline is a little under the middle of the top margin, the footer's
   of the bottom one; the parts at the left margin, centred between the
   margins, at the right margin.

   On the X16 in overlay 14 with hf.c (pdf.c, overlay 15, calls it
   through glue.s and writes iobuf out after each part: the PDF's
   overlay had no room for it).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include "cw.h"
#ifndef CW_HOST
#pragma code-name ("OVERLAY14")
#pragma rodata-name ("OVL14RO")
#pragma bss-name ("OVL14BSS")
#define pdf_hf e_pdf_hf
void pg_name(void);                     /* (pgview.c) hf_name: the document's */
#endif
#define PDFW_HELV                       /* (pdfw.h: Helvetica's widths only) */
#include "pdfw.h"

#ifdef CW_HOST
unsigned char pdf_hfw;                  /* (the X16's: main.c, main RAM; pdf.c sets them) */
unsigned int pdf_hfp, pdf_hfn;
#endif

static unsigned int n;

static void FASTCALL put(const char *s)
{
    while (*s)
        iobuf[n++] = *s++;
}

/* v thousandths of a point as points, two decimals at most, a space after */
static void FASTCALL pt(unsigned long v)
{
    static char b[12];
    static unsigned char k, f;
    v = (v + 5) / 10;
    f = (unsigned char)(v % 100);
    v /= 100;
    k = 0;
    do {
        b[k++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (k)
        iobuf[n++] = b[--k];
    if (f) {
        iobuf[n++] = '.';
        iobuf[n++] = (char)('0' + f / 10);
        if (f % 10)
            iobuf[n++] = (char)('0' + f % 10);
    }
    iobuf[n++] = ' ';
}

unsigned int FASTCALL pdf_hf(unsigned char k)
{
    static const char *t;
    static unsigned long w, l, r;
    static unsigned char c;
    n = 0;
    if (!k) {
#ifndef CW_HOST
        pg_name();
        hf_today();
#endif
        hf_make(pdf_hfw ? pgs.foot : pgs.head, pdf_hfp, pdf_hfn);
    }
    t = hf_part[k];
    if (!*t)
        return 0;
    for (w = 0; *t; ++t)
        w += pw_helv[CSLOT((unsigned char)*t)];
    w *= 36;                            /* (9 points; the widths / 4) */
    l = (unsigned long)pgs.left * 50;
    r = (unsigned long)(pgs.w - pgs.right) * 50;
    if (k == 1)
        l = (l + r - w) / 2;
    else if (k == 2)
        l = r - w;
    if ((long)l < 0)
        l = 0;
    put("BT /F0 9 Tf 1 0 0 1 ");
    pt(l);
    l = pdf_hfw ? (unsigned long)pgs.bottom * 25 : (unsigned long)pgs.h * 50 - (unsigned long)pgs.top * 25;
    pt(l > 3200 ? l - 3200 : 0);
    put("Tm (");
    for (t = hf_part[k]; *t; ++t) {
        c = *t;
        if (c == '(' || c == ')' || c == '\\')
            iobuf[n++] = '\\';
        iobuf[n++] = c;
    }
    put(") Tj ET\n");
    return n;
}
