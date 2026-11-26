/* Veranda Write on Veranda: the PDF's file (overlay 15, with pdf.c;
   DECISIONS W47): pdf.c's buffers to the file pdf_fh (pgpanel.c's t_pdf
   runs it all).

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

/* (plat_pdf_poll, the status bar after each page and Esc: pgpanel.c,
   overlay 16, through glue.s: this overlay's room, 1.6's tables) */
