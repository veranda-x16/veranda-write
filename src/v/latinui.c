/* Veranda Write on Veranda: what the conversion to Veranda's set left
   out, said (DECISIONS W38; overlay 10: overlay 2 has no room). Called
   through glue.s's l1_note when a document has been read whole.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#pragma code-name ("OVERLAY10")
#pragma rodata-name ("OVL10RO")

/* "n letters not in the set: shown as ?" in the status bar (iobuf: free
   between steps) */
void e_l1_note(void)
{
    char *b = (char *)iobuf, *q = b + 6;
    unsigned int n = fmt_lost;
    if (!n)
        return;
    *q = 0;
    do
        *--q = '0' + n % 10;
    while ((n /= 10) && q > b);
    strcpy(b + 6, " letters not in the set: shown as ?");
    memmove(b, q, strlen(q) + 1);
    set_status(b);
}
