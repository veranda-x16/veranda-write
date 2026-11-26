/* Veranda Write on Veranda: strncat for the SDK's Open and Save dialog
   (vrchoose.c, overlay 3: compiled with -Dstrncat=cw_strncat), here in
   its overlay; the library's was 72 bytes of main RAM, for its only user
   (W68: AI help's room).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#define strncat cw_strncat
#include <string.h>
#pragma code-name ("OVERLAY3")

char* __fastcall__ strncat(char* d, const char* s, size_t n)
{
    char *p = d + strlen(d);
    for (; n && *s; --n)
        *p++ = *s++;
    *p = 0;
    return d;
}
