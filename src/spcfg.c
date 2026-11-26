/* Veranda Write: spelling's settings file (DECISIONS W60; docs/SPELLING.md).

   /VERANDA/.SYSTEM/WRITE.CFG, 7 bytes:

     0  4  "WCFG"
     4  1  the version, 1 (a later one only appends fields: these are read)
     5  1  flags: bit 0 misspellings not marked as you type, bit 1 spelling
           only when asked (F7, F8, the Tools menu: nothing loaded or
           marked before), bit 2 (W68) AI help off (no AI menu: Tools >
           AI help); the other bits 0 (a Write before W68 passes bit 2
           over, and writes it 0: AI help on)
     6  1  the language of new documents and of documents saved without
           one: 1 English (0 too), 2 Polish, 3 the keyboard's (Polish when
           Veranda's keyboard setting is Polish, else English)

   A file that is missing, shorter, not "WCFG" or of version 0 gives the
   defaults: marks on, spelling as you work, English (whatever the
   keyboard: the user's choice); a language it does not know is English.
   sp_cfg (main RAM) keeps them as SPC_LANG (LANG_EN, LANG_PL, SPC_KEYB:
   never 0), SPC_NOMARK, SPC_ASK (LANG_EN alone: the defaults). The file is read and written by v/spload.c
   (v/spcfgio.c, overlay 7 since 1.6: the program's start, the Tools menu); these two are the
   format, the same on the host (make test).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "spell.h"
#ifndef CW_HOST
#pragma code-name ("OVERLAY7")
#pragma rodata-name ("OVL7RO")
#pragma bss-name ("OVL7BSS")
#endif

unsigned char FASTCALL sp_cfgparse(const unsigned char *b, unsigned int n)
{
    if (n < SPC_LEN || memcmp(b, "WCFG", 4) || !b[4])
        return LANG_EN;
    return (b[6] >= LANG_PL && b[6] <= SPC_KEYB ? b[6] : LANG_EN) | (b[5] & 7) << 2;   /* (the flags: SPC_NOMARK, SPC_ASK, SPC_NOAI) */
}

void FASTCALL sp_cfgmake(unsigned char *b, unsigned char c)
{
    memcpy(b, "WCFG", 4);
    b[4] = 1;
    b[5] = c >> 2 & 7;
    b[6] = c & SPC_LANG;
}
