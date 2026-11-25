/* Veranda Write on Veranda: spelling's files and banks (overlay 24,
   with the suggestions; called from overlay 23 through v/spell.s).

   The dictionaries are /VERANDA/DICT/PL.DIC and EN.DIC (the system's
   folder: the root's DICT on a card without /VERANDA), read a unit at a
   time into the cache's banks (sp_xfer, main RAM: FRead's buffer is in
   the bank mapped at $A000). The user's words are
   /VERANDA/.SYSTEM/WRITE.UDIC (Veranda's data folder): UTF-8, a word a
   line, so they can be edited on any computer (Writer's USER.DIC);
   written whole when a word is added. The default language (W60): the
   settings' (WRITE.CFG: English unless chosen otherwise); "the
   keyboard's" there: Polish when Veranda's keyboard setting is Polish
   (PREFS_POLISH), else English.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrpath.h>
#include "ovl24.h"                     /* (first: its names for spell.h's) */
#include "cwv.h"
#include "../spell.h"

unsigned int __fastcall__ f_cs_uni(unsigned char c);   /* (glue.s: the SDK's vrcs.s, overlay 10) */

/* the folders, found once (initialised: an overlay's BSS is not
   cleared); a file's path made in iobuf's last 64 bytes (main RAM: the
   kernel reads it, W16) */
static char sp_dir[32] = { 0 };         /* "//VERANDA/DICT/:" */
static char sp_udir[40] = { 0 };        /* "//VERANDA/.SYSTEM/:" */
#define sp_path ((char *)iobuf + IOBUF - 64)
static unsigned char fh = 0;

unsigned int __fastcall__ sw_xfer(unsigned char h, unsigned char b, unsigned int n, unsigned char w);
extern void *sw_xa;                     /* (swapio.c's: where in the bank; main RAM) */

/* "/" + where + "/" + sub + ":" (CMD's path, from curDirName's form) */
static void __fastcall__ mkdir_path(char *out, unsigned char where, const char *sub)
{
    const char *d = vr_cd(where);
    strcpy(out, "/");
    if (d[1])
        strcat(out, d);
    strcat(out, "/");
    strcat(out, sub);
    strcat(out, ":");
    vr_cdback();
}

unsigned char sp_fopen(unsigned char lang)
{
    if (!sp_dir[0])
        mkdir_path(sp_dir, VR_SYS, "DICT/");
    strcpy(sp_path, sp_dir);
    strcat(sp_path, lang == LANG_PL ? "PL.DIC" : "EN.DIC");
    fh = FOpen(sp_path, 'r');
    if (fh < 2) {
        fh = 0;
        return 1;
    }
    return 0;
}

void sp_fclose(void)
{
    if (fh)
        FClose(fh);
    fh = 0;
}

unsigned char FASTCALL sp_fseek(unsigned long off)
{
    return FSeek(fh, (unsigned int)off, (unsigned int)(off >> 16)) != 0;
}

/* (FRead may give less than asked, as MACPTR does: Writer's fio_read
   loop) */
unsigned int sp_fread(unsigned char bank, unsigned int off, unsigned int n)
{
    static unsigned int got, k;
    for (got = 0; got < n; got += k) {
        sw_xa = (void *)(0xA000u + off + got);
        k = sw_xfer(fh, bank, n - got, 0);
        sw_xa = (void *)0xA000;
        if (!k)
            break;
    }
    return got;
}

/* (any 0: a cache's bank, only while one more is left that no chunk
   goes out for: typing splits a long chunk with it, W43; taking the last
   one made typing in a styled paragraph 13 % slower at 512 KB, W59) */
unsigned char FASTCALL sp_bank(unsigned char any)
{
    return any || bk_free_n() > 1 || plat_free() > bk_keep + 2 ? bk_get() : 0;
}

void FASTCALL sp_bfree(unsigned char b)
{
    bk_put(b);
}

/* the cache's banks: a quarter of the free ones, 2 to SP_CBMAX, the
   first one always, the others while there is room (sp_bank: at 512 KB
   with a document open mostly one; at 2 MB 24: 192 KB of the
   dictionary in RAM) */
unsigned char sp_nbanks(void)
{
    unsigned char f = plat_free() >> 2;
    return f < 2 ? 2 : f > SP_CBMAX ? SP_CBMAX : f;
}

unsigned char sp_deflang(void)
{
    if ((sp_cfg & SPC_LANG) != SPC_KEYB)
        return sp_cfg & SPC_LANG;       /* (W60: the settings': English unless chosen otherwise) */
    SysPrefs(PREFS_GET, iobuf);
    return iobuf[6] & PREFS_POLISH ? LANG_PL : LANG_EN;
}

static void upath(void)
{
    if (!sp_udir[0])
        mkdir_path(sp_udir, VR_DATA, "");
    strcpy(sp_path, sp_udir);
    strcat(sp_path, "WRITE.UDIC");
}

/* the user's words: UTF-8, a word a line (a line with a character the
   set has not passed over) */
static unsigned char un, ubad, uprev;
/* (the word: in iobuf, main RAM for bk_wr; not sp_lw, the word being
   looked up when the first lookup reads this) */
#define UREAD 384
#define uw (iobuf + UREAD + 8)

/* (the lower case: spell.c's table is overlay 23's) */
static unsigned char __fastcall__ low(unsigned char c)
{
    static const unsigned char lo[] = { 0xA2, 0xB3, 0xA8, 0xBA, 0xAE, 0xBF, 0xB9, 0xB8, 0xBD, 0xFF };
    static const unsigned char hi[] = { 0xA1, 0xA3, 0xA6, 0xAA, 0xAC, 0xAF, 0xB2, 0xB4, 0xBC, 0xBE };
    static unsigned char k;
    if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE))
        return c + 32;
    for (k = 0; k < sizeof hi; ++k)
        if (hi[k] == c)
            return lo[k];
    return c;
}

static void FASTCALL ubyte(unsigned char b)
{
    if (b == '\n' || b == '\r') {
        if (un && !ubad && sp_ulen + un + 1 <= SP_USERMAX) {
            uw[un] = 0;                 /* (the file's own: no word twice) */
            sp_wr(sp_xb, SP_USER + sp_ulen, uw, un + 1);
            sp_ulen += un + 1;
        }
        un = ubad = uprev = 0;
        return;
    }
    if (uprev) {
        b = (b & 0xC0) == 0x80 ? cs_byte(((unsigned int)(uprev & 0x1F) << 6) | (b & 0x3F)) : 0;
        uprev = 0;
        if (!b)
            ubad = 1;
    } else if (b >= 0xC2 && b <= 0xDF) {
        uprev = b;
        return;
    } else if (b >= 0x80)
        ubad = 1;
    if (un < SP_MAXW)
        uw[un++] = low(b);
    else
        ubad = 1;
}

void sp_uload(void)
{
    static unsigned char h;
    static unsigned int got, i;
    upath();
    h = FOpen(sp_path, 'r');
    if (h < 2)
        return;
    un = ubad = uprev = 0;
    do {
        got = FRead(h, iobuf, UREAD);
        for (i = 0; i < got; ++i)
            ubyte(iobuf[i]);
    } while (got);
    ubyte('\n');
    FClose(h);
}

unsigned char sp_usave(void)
{
    static unsigned char h, c, e;
    static unsigned int k, n, u;
    upath();
    h = FOpen(sp_path, 'w');
    if (h < 2)
        return 1;
    e = 0;
    for (k = 0; k < sp_ulen && !e; k += n) {
        n = sp_ulen - k > sizeof sp_buf ? sizeof sp_buf : sp_ulen - k;
        sp_rd(sp_xb, SP_USER + k, sp_buf, n);
        for (u = 0, c = 0; c < n; ++c) {
            if (!sp_buf[c]) {
                iobuf[u++] = '\n';
                continue;
            }
            if (sp_buf[c] < 0x80)
                iobuf[u++] = sp_buf[c];
            else {
                e = (unsigned char)(f_cs_uni(sp_buf[c]) >> 6);
                iobuf[u++] = 0xC0 | e;
                iobuf[u++] = 0x80 | (f_cs_uni(sp_buf[c]) & 0x3F);
                e = 0;
            }
        }
        if (FWrite(h, iobuf, u))
            e = 1;
    }
    if (FClose(h))
        e = 1;
    return e;
}
