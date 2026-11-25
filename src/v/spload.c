/* Veranda Write on Veranda: spelling's overlays loaded when spelling is
   first used, and given back before a document is opened or exported
   (overlay 22, which has the room; W59).

   Overlays 23-26 (the dictionary and the marks, the suggestions and the
   files, the panel, the pop-up and the Tools menu: 32 KB) are not
   records of `Veranda Write.cvt`, which Veranda would load into four
   banks at the start for good: at 512 KB the gate's 600 KB .docx then
   no longer opened (it had 2 banks to spare, measured). They are
   `/VERANDA/DICT/SPELL.OVL`, beside the dictionaries (tools/spovl.py
   makes it at the link): "WSPO", the build's mark (4 bytes, also put
   into sp_want here, so a file of another build is refused), then each
   overlay: its length (2 bytes) and its bytes. sp_load reads them into
   four banks of the document's pool (bk_get: a chunk of a document
   larger than RAM may go out to the card for them); sp_unload gives
   them back, with the dictionary's banks (the hash, the word lists, the
   cache: overlay 23's sp_drop), before Open (a .docx or an .odt is
   converted in memory) and the exports, and the rows on the screen are
   noted to be looked at again (their marks as drawn stay).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrpath.h>
#include "cwv.h"
#include "../spell.h"
#pragma code-name ("OVERLAY22")
#pragma rodata-name ("OVL22RO")
#pragma bss-name ("OVL22BSS")
#pragma data-name ("OVL22DATA")

unsigned char sp_want[4] = { 0, 0, 0, 0 };   /* (tools/spovl.py writes the build's mark here) */

unsigned int __fastcall__ sw_xfer(unsigned char h, unsigned char b, unsigned int n, unsigned char w);
extern void *sw_xa;
#define PATH ((char *)iobuf + IOBUF - 64)

/* (t_arg 1: loaded) */
void sp_load(void)
{
    static unsigned char h, k, b[4], ok;
    static unsigned int n, a, c, len[4];
    static const char *d;
    t_arg = 0;
    if (bank23) {
        t_arg = 1;
        return;
    }
    d = vr_cd(VR_SYS);
    strcpy(PATH, "/");
    if (d[1])
        strcat(PATH, d);
    strcat(PATH, "/DICT/:SPELL.OVL");
    vr_cdback();
    h = FOpen(PATH, 'r');
    if (h < 2) {
        sp_nofile = 1;
        return;
    }
    ok = FRead(h, iobuf, 8) == 8 && !memcmp(iobuf, "WSPO", 4) && !memcmp(iobuf + 4, sp_want, 4);
    for (k = 0; k < 4; ++k)
        b[k] = 0;
    for (k = 0; k < 4 && ok; ++k) {
        ok = FRead(h, iobuf, 2) == 2;
        n = iobuf[0] | (unsigned int)iobuf[1] << 8;
        if (!ok || n > 8191 || !(b[k] = bk_get()))
            ok = 0;
        else
            ok = sw_xfer(h, b[k], n, 0) == n;
        len[k] = n;
    }
    FClose(h);
    if (!ok) {
        for (k = 0; k < 4; ++k)
            bk_put(b[k]);
        sp_nofile = 1;
        return;
    }
    /* (W60) each overlay's variables after its bytes cleared: the banks
       come from the pool with what they last held (a program started
       again in the same session finds its earlier self's values there) */
    memset(iobuf, 0, IOBUF);
    for (k = 0; k < 4; ++k)
        for (a = len[k]; a < 8192; a += c) {
            c = 8192 - a > IOBUF ? IOBUF : 8192 - a;
            bk_wr(b[k], 0xA000u + a, iobuf, c);
        }
    bank23 = b[0];
    bank24 = b[1];
    bank25 = b[2];
    bank26 = b[3];
    t_arg = 1;
}

void sp_unload(void)
{
    static unsigned char k;
    sp_nofile = 0;                      /* (another try after this) */
    if (!bank23 || spp_on)
        return;
    sp_drop(1);
    bk_put(bank23);
    bk_put(bank24);
    bk_put(bank25);
    bk_put(bank26);
    bank23 = bank24 = bank25 = bank26 = 0;
    for (k = 0; k < vn; ++k)
        vr[k].mk |= 0x80;               /* (looked at again when it is back) */
    sp_pend = 1;
}

/*---------------------------------------------------------------
  (W60) the settings: /VERANDA/.SYSTEM/WRITE.CFG (src/spcfg.c says
  its bytes). Read at the start (ui_main): a file that is not there or
  not one gives the defaults; written when the Tools menu changes them
 ---------------------------------------------------------------*/
static const char cfgname[] = "WRITE.CFG";

static void live(void)
{
    sp_live = !(sp_cfg & (SPC_NOMARK | SPC_ASK));
}

void sp_cfgload(void)
{
    static unsigned char h;
    static unsigned int n;
    n = 0;
    vr_cd(VR_DATA);
    strcpy(PATH, cfgname);              /* (main RAM: the kernel reads it) */
    h = FOpen(PATH, 'r');
    if (h >= 2) {
        n = FRead(h, iobuf, 16);
        FClose(h);
    }
    vr_cdback();
    sp_cfg = sp_cfgparse(iobuf, n > 16 ? 0 : n);
    live();
}

void sp_cfgsave(void)
{
    static unsigned char h, e;
    live();
    e = 1;
    vr_cd(VR_DATA);
    strcpy(PATH, cfgname);
    h = FOpen(PATH, 'w');
    if (h >= 2) {
        sp_cfgmake(iobuf, sp_cfg);
        e = FWrite(h, iobuf, SPC_LEN);
        e |= FClose(h);
    }
    vr_cdback();
    t_arg = e;
}


