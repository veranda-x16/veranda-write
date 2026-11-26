/* Veranda Write on Veranda: the gate's hooks for spelling's settings
   and dictionaries (the test build, -DCW_TEST: src/v/test.c's 'L'
   $78-$7C); the test overlay (v/ovlclr.c's, overlay 19, until 1.6), with
   the tables' log ($78, tb_tgate).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#include "../spell.h"
#include <vrpath.h>
#pragma code-name ("OVERLAY30")
#pragma rodata-name ("OVL30RO")
#pragma bss-name ("OVL30BSS")
#pragma data-name ("OVL30DATA")
#define PATH ((char *)iobuf + IOBUF - 64)
extern unsigned char lb[128];           /* (view.c: main RAM, cb_read's) */

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

/*---------------------------------------------------------------
  (src/v/test.c's 'L' $78: "CW TABLE n rows cells sum": the table codes
  in order, each with its paragraph's number, summed as tools/cwrd.py's
  tab_sum does; $79-$7C: sp_tgate, above)
 ---------------------------------------------------------------*/
static unsigned char tlog_n;

void tb_tgate(void)
{
    static const char hx[] = "0123456789ABCDEF";
    static unsigned long sum, v;
    static unsigned int rows, cells, i, n, np;
    static pos_t p;
    static unsigned char c, j, s;
    static char *o;
    if (t_arg != 0x78) {
        sp_tgate();
        return;
    }
    sum = rows = cells = np = 0;
    for (p = 0; p < DOC_LEN; p += i) {
        n = cb_read(&tx, p, lb, sizeof lb);
        for (i = 0; i < n;) {
            c = lb[i];
            if (c == CH_CR)
                ++np;
            if (!IS_KIND(c)) {
                ++i;
                continue;
            }
            if (i + 3 > n)
                break;
            if (IS_TKIND(c)) {
                rows += c == K_TROW;
                cells += c == K_TCELL;
                sum = sum * 31 + np * 7 + c * 256 + lb[i + 1];
            }
            i += 3;
        }
        if (!i)
            break;
    }
    o = (char *)iobuf;
    strcpy(o, "CW TABLE ");
    o += 9;
    for (j = 0; j < 4; ++j) {
        v = j == 0 ? ++tlog_n : j == 1 ? rows : j == 2 ? cells : sum;
        for (s = 28;; s -= 4) {
            *o++ = hx[(unsigned char)(v >> s) & 15];
            if (!s)
                break;
        }
        *o++ = ' ';
    }
    *o = 0;
    LogText(iobuf);
}
#endif
