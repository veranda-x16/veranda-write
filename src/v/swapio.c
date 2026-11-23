/* Veranda Write on Veranda: the pager's files (overlay 12; the bytes
   moved by sw_xfer in main RAM, which maps the chunk's bank for FRead
   and FWrite: their buffer at $A000 is in the caller's bank, Veranda's
   V47).

   The swap is a folder in Veranda's data folder, CWSWAP.n in
   /VERANDA/.SYSTEM: a file a slot (S000 ...: 8.3 names, no long name
   entries for the DOS to pass over), each written whole ('w': a new file,
   MCIOUT; an overwrite inside one file is written byte by byte, V55,
   four times slower: measured, DECISIONS W29) and read whole. A clean
   chunk of the document is read from the document's own file, at its
   place (FSeek, then FRead). Every read and write opens, moves and
   closes: no file stays open across the main loop (W12: Alt+Tab).

   At the start, the swap folders of a Veranda Write that did not end
   (a crash, the power, the switcher's "quit") are removed when no other
   Veranda Write is put aside; the documents are as last saved (the
   swap only ever has copies of the text). n is the first folder not in
   use. Quit removes it.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrpath.h>
#include "cwv.h"
#include "ovl12.h"

/* (main RAM: the kernel reads the names: initialised, so DATA, W16) */
static char sw_dir[48] = { 0 };         /* "//VERANDA/.SYSTEM/CWSWAP.0/:" */
static char sw_name[64] = { 0 };        /* a slot's file's path, or the document's */
static char dc_dir[64] = { 0 }, dc_name[64] = { 0 };   /* the document's folder and name */

#pragma code-name (push, "CODE")
/* n bytes between file h and bank b's $A000 (w: write); the bytes moved */
unsigned int __fastcall__ sw_xfer(unsigned char h, unsigned char b, unsigned int n, unsigned char w)
{
    unsigned char keep = *(volatile unsigned char *)0;
    unsigned int k;
    MAPBANK(b);
    if (w)
        k = FWrite(h, (void *)0xA000, n) ? 0 : n;
    else
        k = FRead(h, (void *)0xA000, n);
    MAPBANK(keep);
    return k;
}
#pragma code-name (pop)

static const char hexd[] = "0123456789ABCDEF";
static unsigned char sw_n = 0xFF;       /* the folder's number (0xFF: none yet) */

static void slot_name(unsigned int s)
{
    char *p = sw_name;
    strcpy(p, sw_dir);
    p += strlen(p);
    *p++ = 'S';
    *p++ = hexd[(s >> 8) & 15];
    *p++ = hexd[(s >> 4) & 15];
    *p++ = hexd[s & 15];
    *p = 0;
}

unsigned char FASTCALL plat_sw_write(unsigned int slot, unsigned char bank, unsigned int n)
{
    unsigned char h, e;
    unsigned int k;
    slot_name(slot);
    h = FOpen(sw_name, 'w');
    if (h < 2)
        return 1;
    k = sw_xfer(h, bank, n, 1);
    e = FClose(h);
    return k != n || e;
}

unsigned char FASTCALL plat_sw_read(unsigned int slot, unsigned char bank, unsigned int n)
{
    unsigned char h;
    unsigned int k;
    slot_name(slot);
    h = FOpen(sw_name, 'r');
    if (h < 2)
        return 1;
    k = sw_xfer(h, bank, n, 0);
    FClose(h);
    return k != n;
}

static unsigned char dfar;

/* the document's file opened at off (its path in CMD's syntax,
   "//DOCUMENTS/:name", "//:name"; one too long for a name: its folder
   made current for it); the handle, < 2 not */
unsigned char FASTCALL sw_dopen(pos_t off)
{
    unsigned char h;
    dfar = 0;
    if (strlen(dc_dir) + strlen(dc_name) < 60) {
        strcpy(sw_name, "/");
        if (dc_dir[1])
            strcat(sw_name, dc_dir);
        strcat(sw_name, "/:");
        strcat(sw_name, dc_name);
    } else {
        ChDir(dc_dir);
        strcpy(sw_name, dc_name);
        dfar = 1;
    }
    h = FOpen(sw_name, 'r');
    if (h >= 2 && FSeek(h, (unsigned int)off, (unsigned int)(off >> 16))) {
        FClose(h);
        h = 0;
    }
    if (h < 2 && dfar)
        ChDir((char *)curDirName);
    return h;
}

void FASTCALL sw_dclose(unsigned char h)
{
    FClose(h);
    if (dfar)
        ChDir((char *)curDirName);
}

unsigned char FASTCALL plat_doc_read(pos_t off, unsigned char bank, unsigned int n)
{
    unsigned char h;
    unsigned int k = 0;
    h = sw_dopen(off);
    if (h >= 2) {
        k = sw_xfer(h, bank, n, 0);
        sw_dclose(h);
    }
    return k != n;
}

/* the document's file is name in the current folder now (opened, saved) */
void FASTCALL sw_doc(const char *name)
{
    strcpy(dc_dir, (char *)curDirName);
    strcpy(dc_name, name);
}

unsigned char FASTCALL plat_sw_fail(unsigned char e)
{
    if (e == SE_READ) {
        message("Veranda Write cannot read its text back", "from the card. Put it in, then OK.");
        return 1;
    }
    return yesno("The card takes no more of the text (full?).", "Try again? (No: the change is not made)");
}

/* the folder's files out, the folder too (the data folder current): the
   folder listed, then its files deleted by name, 30 at a time (iobuf:
   the names at 16, DirNext's entry at 440; a pattern, "*", is not
   scratched on the card by R49's CMDR-DOS: the gate found it) */
static void wipe(const char *name)
{
    static unsigned char n, k;
    if (ChDir((void *)name))
        return;
    do {
        n = 0;
        iobuf[0] = '*';
        iobuf[1] = 0;
        if (DirOpen(iobuf) == 0)
            while (DirNext(iobuf + 440) == 0)
                if (iobuf[440] != 'D' && n < 30 && strlen((char *)iobuf + 443) < 13)
                    strcpy((char *)iobuf + 16 + 13 * n++, (char *)iobuf + 443);
        for (k = 0; k < n; ++k)
            FDelete(iobuf + 16 + 13 * k);
    } while (n == 30);
    vr_cd(VR_DATA);
    RmDir((void *)name);
}

static char fname[12] = "CWSWAP.0";     /* (main RAM: the kernel reads it) */

void sw_start(void)
{
    static unsigned char i, others, n, stale;
    static const char *d;
    others = 0;
    for (i = 0; i < TASK_MAX; ++i) {
        iobuf[TR_STATE] = 0;
        TaskCtl(TASK_INFO, i, iobuf);
        if (iobuf[TR_STATE] && !memcmp(iobuf + TR_NAME, "Veranda Write", 13))
            ++others;
    }
    d = vr_cd(VR_DATA);
    n = 0xFF;
    stale = 0;
    for (i = 0; i < 8; ++i) {
        fname[7] = '0' + i;
        if (!ChDir(fname)) {
            vr_cd(VR_DATA);
            if (!others) {
                wipe(fname);            /* (a Veranda Write that did not end) */
                vr_cd(VR_DATA);
                stale = 1;
                if (n == 0xFF)
                    n = i;
            }
        } else if (n == 0xFF)
            n = i;
    }
    if (n == 0xFF)
        n = 7;
    fname[7] = '0' + n;
    MkDir(fname);
    sw_n = n;
    strcpy(sw_dir, "/");
    if (d[1])
        strcat(sw_dir, d);
    strcat(sw_dir, "/");
    strcat(sw_dir, fname);
    strcat(sw_dir, "/:");
    vr_cdback();
#ifdef CW_TEST
    if (stale)
        LogText("CW STALE SWAP REMOVED");
#endif
    if (stale)
        message("Veranda Write did not end the last time:", "its swap was removed (documents as saved).");
}

void sw_end(void)
{
    if (sw_n == 0xFF)
        return;
    fname[7] = '0' + sw_n;
    vr_cd(VR_DATA);
    wipe(fname);
    vr_cdback();
    sw_n = 0xFF;
}
