/* Veranda Write on Veranda: an export's progress (oexp.c's oe_poll;
   DECISIONS W54, W71). In overlay 22, the templates', loaded for an
   export too (W71: it was v/oxnote.c's, overlay 18, whose room the
   import wanted). A file of its own: cc65 puts a file's string literals
   in its last rodata segment (in oxnote.c, behind pragmas, they went to
   overlay 18's and were read from 22's bank).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrwin.h>
#include "cwv.h"
#include "view.h"
#include "../office.h"
#pragma code-name ("OVERLAY22")
#pragma rodata-name ("OVL22RO")
#pragma bss-name ("OVL22BSS")

/* (oexp.c's, overlay 21: its room, 1.6) the export's progress in the
   status bar, "Exporting 45%..." (its pass, 0 or 1, and its place in the
   text, oe_rp); Esc in the ROM's queue: 1 */
extern char sl[64], sr[40];             /* (view.c: main RAM, the status bar's) */
unsigned char cw_esc(void);             /* (glue.s) */

extern pos_t oe_rp;                     /* (main.c: main RAM, oexp.c's place in the text) */

unsigned char __fastcall__ e_oe_poll(unsigned char pass)
{
    static char *p;
    static unsigned char pc;
    pc = (unsigned char)(pass * 50 + oe_rp * 50 / (tx.total + 1));
    p = sl;
    strcpy(p, "Exporting ");
    p += 10;
    if (pc >= 10)
        *p++ = (char)('0' + pc / 10);
    *p++ = (char)('0' + pc % 10);
    strcpy(p, "%...");
    strcpy(sr, "Esc: stop");
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    vr_win_status(&win, sl, sr);
    return cw_esc();
}

