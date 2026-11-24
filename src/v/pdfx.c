/* Veranda Write on Veranda: the PDF's file and progress (overlay 15,
   with pdf.c; DECISIONS W47): pdf.c's buffers to the file pdf_fh, the
   status bar's "Exporting page 3 of 12" after each page, Esc to stop
   (pgpanel.c's t_pdf runs it all).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrwin.h>
#include "cwv.h"
#include "view.h"
#pragma code-name ("OVERLAY15")
#pragma rodata-name ("OVL15RO")
#pragma bss-name ("OVL15BSS")

extern char sl[64], sr[40];             /* (view.c: main RAM, the status bar's) */
unsigned char cw_esc(void);             /* (glue.s) Esc in the ROM's key queue: taken, 1 */

extern unsigned char pdf_fh;            /* (main.c: pgpanel.c opens it) */

unsigned char FASTCALL plat_pdf_out(unsigned int n)
{
    return FWrite(pdf_fh, iobuf, n) != 0;
}

extern unsigned long pv;                /* (pdfnum.s: a number in decimal) */
extern char pd[11];
unsigned char pdf_dec(void);

static char *__fastcall__ put_u(char *p, unsigned int v)
{
    pv = v;
    pdf_dec();
    strcpy(p, pd);
    return p + strlen(p);
}

/* a page done: the status bar says so; Esc stops */
unsigned char plat_pdf_poll(void)
{
    char *p = sl;
    strcpy(p, "Exporting page ");
    p = put_u(p + 15, pdf_page + (pdf_page < pdf_pages));
    strcpy(p, " of ");
    p = put_u(p + 4, pdf_pages);
    strcpy(p, "...");
    strcpy(sr, "Esc: stop");
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    vr_win_status(&win, sl, sr);
    return cw_esc();
}

