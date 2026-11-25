/* Veranda Write on Veranda: every record overlay's variables cleared at
   the start (W60), and the gate's hooks for spelling's settings (the test
   build, -DCW_TEST: src/v/test.c's 'L' $7B, $7C); overlay 19, which has
   the room.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#include "../spell.h"
#include <vrpath.h>
#pragma code-name ("OVERLAY19")
#pragma rodata-name ("OVL19RO")
#pragma bss-name ("OVL19BSS")
#pragma data-name ("OVL19DATA")
#define PATH ((char *)iobuf + IOBUF - 64)

/*---------------------------------------------------------------
  (W60) every record overlay's variables cleared at the start (ui_main,
  first): the loader leaves an overlay's BSS as its bank held it (Veranda's
  V37), and the variables of a Veranda Write started again in the same
  session were its earlier self's (the gate's restart: spelling crashed
  when it was first loaded); v/ovlbss.s has each one's place. The loop's
  variables on the C stack: overlay 19's own BSS is cleared too
 ---------------------------------------------------------------*/
extern const unsigned int ovl_bss[23][3];

void ovl_clear(void)
{
    unsigned char k, b;
    unsigned int a, e, c;
    const unsigned int *t = ovl_bss[0];
    memset(iobuf, 0, IOBUF);
    for (k = 0; k < 23; ++k, t += 3) {
        b = *(unsigned char *)t[0];
        a = t[1];
        e = a + t[2];
        if (!b || a == e)
            continue;
        if (b == bank19) {
            memset((void *)a, 0, e - a);        /* (this one: mapped) */
            continue;
        }
        for (; a < e; a += c) {
            c = e - a > IOBUF ? IOBUF : e - a;
            bk_wr(b, a, iobuf, c);
        }
    }
}

#ifdef CW_TEST
/*---------------------------------------------------------------
  (src/v/test.c: 'L' $7A-$7C, t_arg; here for the test overlay's room).
  $7A: Veranda's keyboard setting Polish, or not again (PREFS_POLISH).
  $7B: "CW SPCFG n cfg live loaded marks lang":
  the settings (sp_cfg), marking as you type, spelling's overlays loaded,
  the marks the rows on the screen were drawn with, the document's
  language. $7C: the program quits and starts again (the
  settings kept on the card): CW.PLY deleted, so test_start reads the
  session's second part, CW.PL2; the test desktop (tools/desk) starts the
  program again when nothing was put aside
 ---------------------------------------------------------------*/
static unsigned char cfg_n;

static void tlog(void)
{
    static const char hx[] = "0123456789ABCDEF";
    static unsigned char v[6], i;
    static char *p;
    for (i = v[4] = 0; i < vn; ++i)
        v[4] += vr[i].mk & 0x7F;
    v[0] = ++cfg_n;
    v[1] = sp_cfg;
    v[2] = sp_live;
    v[3] = bank23 != 0;
    v[5] = fmt_lang;
    p = (char *)iobuf;                  /* (main RAM: the kernel reads it) */
    strcpy(p, "CW SPCFG");
    p += 8;
    for (i = 0; i < 6; ++i) {
        *p++ = ' ';
        *p++ = hx[v[i] >> 4];
        *p++ = hx[v[i] & 15];
    }
    *p = 0;
    LogText(iobuf);
}

static void trestart(void)
{
    strcpy(PATH, "CW.PLY");
    FDelete(PATH);
    mouseSource = 0;                    /* (the player is in this program's memory) */
    doc_modified = 0;
    CALL2(f_quit);
}
/* (W61) 'L' $79: a dictionary arrives while Write runs (the nodict
   session: tools/run.sh put dict/EN.DIC as VERANDA/DICT/EN.NEW; renamed) */
static void tdict(void)
{
    strcpy(PATH, "/VERANDA/DICT");
    if (!ChDir(PATH)) {
        strcpy(PATH, "EN.NEW");
        strcpy(PATH + 20, "EN.DIC");
        FRename(PATH, PATH + 20);
    }
    vr_cdback();
    strcpy(PATH, "CW DICT ARRIVED");
    LogText(PATH);
}

void sp_tgate(void)
{
    if (t_arg == 0x79)
        tdict();
    else if (t_arg == 0x7B)
        tlog();
    else if (t_arg == 0x7C)
        trestart();
    else {
        SysPrefs(PREFS_GET, iobuf);
        iobuf[6] ^= PREFS_POLISH;
        SysPrefs(PREFS_SET, iobuf);
        SysPrefs(PREFS_GET, iobuf);     /* (as the kernel has it now) */
        strcpy(PATH, iobuf[6] & PREFS_POLISH ? "CW KEYBOARD POLISH" : "CW KEYBOARD NOT POLISH");
        LogText(PATH);
    }
}
#endif
