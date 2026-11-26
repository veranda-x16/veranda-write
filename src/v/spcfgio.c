/* Veranda Write on Veranda: spelling's settings read and written
   (DECISIONS W60; src/spcfg.c is their bytes): overlay 7, with
   spcfg.c (v/spload.c's, overlay 22, until 1.6's tables wanted its
   room).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrpath.h>
#include "cwv.h"
#include "../spell.h"
#pragma code-name ("OVERLAY7")
#pragma rodata-name ("OVL7RO")
#pragma bss-name ("OVL7BSS")
#define PATH ((char *)iobuf + IOBUF - 64)

/*---------------------------------------------------------------
  (W60) the settings: /VERANDA/.SYSTEM/WRITE.CFG (src/spcfg.c says
  its bytes). Read at the start (ui_main): a file that is not there or
  not one gives the defaults; written when the Tools menu changes them
 ---------------------------------------------------------------*/
static const char cfgname[] = "WRITE.CFG";

static void live(void)
{
    sp_live = !(sp_cfg & (SPC_NOMARK | SPC_ASK));
    /* (W68) the AI menu, the bar's last item, there when AI help is on */
    mbar.flags = HORIZONTAL | UN_CONSTRAINED | (sp_cfg & SPC_NOAI ? NBAR - 1 : NBAR);
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
