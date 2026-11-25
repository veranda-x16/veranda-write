/* Veranda Write: the XML text of the .docx and .odt it exports, its
   templates expanded (DECISIONS W52; tools/oparts.py makes them, with
   their dictionary, into oparts.h; oexp.c puts the values in).

   A template is bytes: below $20 (but \n) a value's number (31: the next
   byte its number less 31), $80 and up a piece of the dictionary, the
   rest the text. op_open starts template k (on a stack: a value may be
   another template); op_next puts its next bytes straight into the
   exporter's buffer (iobuf + 256, at oe_on: main RAM) and returns at a
   value (its number; 31: op_x the next byte), when the buffer is full
   ($FF: written out, then call again) or at the template's end (0: the
   template before it is the one read again). The X16's op_next is
   v/oparts.s (cc65's C for it took 400 cycles a byte, this 30).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "office.h"
#ifndef CW_HOST
#pragma code-name ("OVERLAY22")
#pragma rodata-name ("OVL22RO")
#pragma bss-name ("OVL22BSS")
#define op_open e_op_open
#endif
#define OPARTS_DATA
#include "oparts.h"

/* the stack (v/oparts.s reads it): each template's next byte, its end */
#define NST 8
const unsigned char *op_at[NST], *op_end[NST];
unsigned char op_sd;
const unsigned char *op_pd;             /* a piece being copied, */
unsigned char op_pn;                    /* its bytes left */

void FASTCALL op_open(unsigned char k)
{
    if (!op_sd)
        op_pn = 0;
    if (op_sd < NST) {
        op_at[op_sd] = t_bytes + t_off[k];
        op_end[op_sd] = t_bytes + t_off[k + 1];
        ++op_sd;
    }
}

#ifdef CW_HOST
unsigned char oe_on, op_x;              /* (the X16's: main.c, main RAM) */

unsigned char op_next(void)
{
    static const unsigned char *a;
    static unsigned char c;
    if (!op_sd)
        return 0;
    a = op_at[op_sd - 1];
    for (;;) {
        if (oe_on == OE_N) {
            op_at[op_sd - 1] = a;
            return 0xFF;
        }
        if (op_pn) {
            iobuf[256 + oe_on++] = *op_pd++;
            --op_pn;
            continue;
        }
        if (a == op_end[op_sd - 1]) {
            --op_sd;
            return 0;
        }
        c = *a++;
        if (c >= 0x80) {
            op_pd = d_bytes + d_off[c - 0x80];
            op_pn = (unsigned char)(d_off[c - 0x7F] - d_off[c - 0x80]);
            continue;
        }
        if (c < 0x20 && c != '\n') {
            if (c == 31)
                op_x = *a++;
            op_at[op_sd - 1] = a;
            return c;
        }
        iobuf[256 + oe_on++] = c;
    }
}
#endif
