/* Veranda Write on Veranda: every record overlay's variables cleared at
   the start (W60); overlay 9, which has the room (W66: 19 is no longer
   a record).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#include "../spell.h"
#include <vrpath.h>
#pragma code-name ("OVERLAY9")
#pragma rodata-name ("OVL9RO")
#pragma bss-name ("OVL9BSS")
#pragma data-name ("OVL9DATA")
#define PATH ((char *)iobuf + IOBUF - 64)

/*---------------------------------------------------------------
  (W60) every record overlay's variables cleared at the start (ui_main,
  first): the loader leaves an overlay's BSS as its bank held it (Veranda's
  V37), and the variables of a Veranda Write started again in the same
  session were its earlier self's (the gate's restart: spelling crashed
  when it was first loaded); v/ovlbss.s has each one's place. The loop's
  variables on the C stack: overlay 9's own BSS is cleared too
 ---------------------------------------------------------------*/
extern const unsigned int ovl_bss[20][3];

void ovl_clear(void)
{
    unsigned char k, b;
    unsigned int a, e, c;
    const unsigned int *t = ovl_bss[0];
    memset(iobuf, 0, IOBUF);
    for (k = 0; k < 20; ++k, t += 3) {
        b = *(unsigned char *)t[0];
        a = t[1];
        e = a + t[2];
        if (!b || a == e)
            continue;
        if (b == bank9) {
            memset((void *)a, 0, e - a);        /* (this one: mapped) */
            continue;
        }
        for (; a < e; a += c) {
            c = e - a > IOBUF ? IOBUF : e - a;
            bk_wr(b, a, iobuf, c);
        }
    }
}

/* (W60, W61: the gate's hooks, sp_tgate: v/tgate.c, overlay 10, since
   1.6's tables wanted this overlay's room in the test build) */

