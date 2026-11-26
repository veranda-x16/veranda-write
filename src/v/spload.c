/* Veranda Write on Veranda: the overlays that are not records, loaded
   when they are wanted: spelling's when spelling is first used, given
   back before a document is opened or exported (W59); the office
   files' when a .docx or an .odt is opened or exported, given back after
   it (W66). Overlay 16, which has the room (1.6's tables wanted the
   banks the office overlays had for good).

   Overlays 23-26 (the dictionary and the marks, the suggestions and the
   files, the panel, the pop-up and the Tools menu: 32 KB) and 17-22 (the
   ZIP and XML readers, the import, .docx's, .odt's, the export and its
   parts: 48 KB) are not records of `Veranda Write.cvt`, which Veranda
   would load into banks at the start for good: at 512 KB the gate's
   600 KB .docx then no longer opened (W59), and 1.6's three table
   overlays left a document too few banks with spelling loaded (W66).
   They are `/VERANDA/DICT/SPELL.OVL` (tools/spovl.py makes it at the
   link): "WSPO", the build's mark (4 bytes, also put into sp_want here,
   so a file of another build is refused), then each overlay, 23-26 then
   17-22: its length (2 bytes) and its bytes. Their banks' variables are
   glue.s's in that order. Spelling's are read into banks of the
   document's pool (bk_get: a chunk of a document larger than RAM may go
   out to the card for them); the office files' into a run of six of
   Veranda's (BankAlloc), given back whole after (taken from the pool
   one at a time they filled the runs kept for what is never put out, and
   the gate's 600 KB .docx no longer opened at 512 KB), from the pool only
   when Veranda has no such run. Each bank's variables after its bytes
   cleared.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrpath.h>
#include "cwv.h"
#include "../spell.h"
#pragma code-name ("OVERLAY16")
#pragma rodata-name ("OVL16RO")
#pragma bss-name ("OVL16BSS")
#pragma data-name ("OVL16DATA")

unsigned char sp_want[4] = { 0, 0, 0, 0 };   /* (tools/spovl.py writes the build's mark here) */

unsigned int __fastcall__ sw_xfer(unsigned char h, unsigned char b, unsigned int n, unsigned char w);
#define PATH ((char *)iobuf + IOBUF - 64)
#define OB (&bank23)                    /* (glue.s's: 23-26, then 17-22) */

/* m: SPELL.OVL's overlays, bit k its k-th (23-26, 17-22) */
static void FASTCALL give(unsigned int m)
{
    static unsigned char k;
    for (k = 0; m; ++k, m >>= 1)
        if (m & 1) {
            bk_put(OB[k]);
            OB[k] = 0;
        }
}

/* SPELL.OVL's overlays m (bit k its k-th) into banks (their variables',
   else bk_get's); 1 done (else the caller gives them back) */
static unsigned char FASTCALL get(unsigned int m)
{
    static unsigned char h, k, ok;
    static unsigned int a, c, len;
    static unsigned long at;
    static const char *d;
    d = vr_cd(VR_SYS);
    strcpy(PATH, "/");
    if (d[1])
        strcat(PATH, d);
    strcat(PATH, "/DICT/:SPELL.OVL");
    vr_cdback();
    h = FOpen(PATH, 'r');
    if (h < 2)
        return 0;
    ok = FRead(h, iobuf, 8) == 8 && !memcmp(iobuf, "WSPO", 4) && !memcmp(iobuf + 4, sp_want, 4);
    at = 8;
    for (k = 0; m && ok; ++k, m >>= 1) {
        ok = FRead(h, iobuf, 2) == 2;
        len = iobuf[0] | (unsigned int)iobuf[1] << 8;
        at += 2;
        if (!ok || len > 8191)
            ok = 0;
        else if (!(m & 1))
            ok = !FSeek(h, (unsigned int)(at += len), (unsigned int)(at >> 16));
        else if (!(OB[k] || (OB[k] = bk_get())) || sw_xfer(h, OB[k], len, 0) != len)
            ok = 0;
        else {
            at += len;
            /* (W60) the variables after the bytes cleared: the bank comes
               from the pool with what it last held */
            memset(iobuf, 0, IOBUF);
            for (a = len; a < 8192; a += c) {
                c = 8192 - a > IOBUF ? IOBUF : 8192 - a;
                bk_wr(OB[k], 0xA000u + a, iobuf, c);
            }
        }
    }
    FClose(h);
    return ok;
}

/* (t_arg 1: loaded) */
void sp_load(void)
{
    t_arg = 1;
    if (bank23)
        return;
    t_arg = get(0x0F);
    if (!t_arg) {
        give(0x0F);
        sp_nofile = 1;
    }
}

void sp_unload(void)
{
    static unsigned char k;
    sp_nofile = 0;                      /* (another try after this) */
    if (!bank23 || spp_on)
        return;
    sp_drop(1);
    give(0x0F);
    for (k = 0; k < vn; ++k)
        vr[k].mk |= 0x80;               /* (looked at again when it is back) */
    sp_pend = 1;
}

/* (W66) the office overlays for an import (t_arg 0: 17-20, the ZIP and
   XML readers, the import, .docx's, .odt's) or an export (1: 18, 21,
   22, its progress, the export, its parts): fewer banks taken than all
   six (t_arg back 1: loaded; else said) */
static unsigned char run;              /* (the run's first bank, 0: from the pool) */
static unsigned int om;                 /* (the ones loaded) */

void ox_up(void)
{
    static unsigned char k, n, b;
    if (om) {
        t_arg = 1;
        return;
    }
    om = t_arg ? 0x320 : 0x0F0;
    n = t_arg ? 3 : 4;
    if ((b = run = BankAlloc(n)) != 0)
        for (k = 4; k < 10; ++k)
            if (om >> k & 1)
                OB[k] = b++;
    t_arg = 1;
    if (get(om))
        return;
    ox_down();
    t_arg = 0;
    message("Cannot read", "DICT/SPELL.OVL");
}

void ox_down(void)
{
    if (!run)
        give(om);
    else {
        BankFree(run);
        run = 0;
        memset(OB + 4, 0, 6);
    }
    om = 0;
}

/* ((W60) the settings' file, sp_cfgload and sp_cfgsave: v/spcfgio.c,
   overlay 7, since 1.6's tables wanted overlay 22's room) */

/* (W68) AI help (overlay 31, SPELL.OVL's eleventh: v/aiui.c) for a
   command (t_arg: the AI menu's item, AI_BACK), in a bank of Veranda's
   (else the pool's) for it alone, given back after: a record would hold
   a bank of the document's for good, for something used now and then */
void ai_go(void)
{
    static unsigned char k, own;
    k = t_arg;
    own = (OB[10] = BankAlloc(1)) != 0;
    if (get(0x400)) {
        t_arg = k;
        callb(OB[10], ai_cmd);
    } else {
        if (k == AI_BACK)
            ai_st = 3;                  /* (looked at again: AI > Check for the answer) */
        message("Cannot read", "DICT/SPELL.OVL");
    }
    if (own)
        BankFree(OB[10]);
    else if (OB[10])
        bk_put(OB[10]);
    OB[10] = 0;
}
