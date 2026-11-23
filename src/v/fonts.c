/* Veranda Write on Veranda: the styles' fonts (main RAM).

   Text is drawn by Veranda Write itself (render.s) from its own copies
   of the fonts (the font cache, fcache.c: read from their files, the
   point size nearest the one asked for), several fonts and sizes on a
   page at once. A font that is not in the fonts folder is drawn in Gate
   Sans (Veranda's own); one the cache cannot hold (too large) is measured
   by Veranda's font service (LoadFont, its DECISIONS V62) and not drawn
   (fcache.c says how large). The style keeps its font's name and size,
   so a document comes back right on a machine that has the font.

   Wrapping measures every character, so each font slot keeps its 192
   widths (NW: $20-$7F and $A0-$FF, CSLOT in cw.h; the cache's, or
   GetCharWidth's): in the row bank (WT_ADDR, after render.s's tables:
   1152 bytes, W38), a slot's copied into one of two buffers (main RAM,
   where word wrap and the drawing read them; the one used less lately)
   when the style's slot is in neither (one buffer: 2 s more to open the
   gate's big document, a copy at each change between two fonts);
   bold draws a pixel wider.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"

static unsigned char wb0[NW], wb1[NW];  /* two slots' widths (W38) */
unsigned char wbk[2] = { 0xFF, 0xFF };  /* which slots they are */
unsigned char wlru;                     /* the one to take next */
unsigned char *wcur;                    /* fc_setup's: the widths it makes (fcache.c) */
unsigned char fs_kind[NFS];             /* 0 free, 1 the cache's, 2 the kernel's */
unsigned int fs_key[NFS];               /* the name's hash */
unsigned char fs_size[NFS];             /* the size asked for */
unsigned char fs_a[NFS], fs_d[NFS], fs_h[NFS], fs_bank[NFS], fs_bank2[NFS], fs_split[NFS], fs_bank3[NFS], fs_split2[NFS];
unsigned int fs_base[NFS], fs_rowlen[NFS];
unsigned char fs_next;
unsigned char s_slot[MAXSTY];           /* each style's slot (0xFF: not known) */
unsigned char fl_s;                     /* (fc_setup's style) */

const unsigned char *fl_w;
unsigned char fl_bold, fl_a, fl_d, fl_slot;

unsigned char fl_last = 0xFF;           /* the style fl_style set last (0xFF: none) */

void fl_flush(void)
{
    memset(s_slot, 0xFF, sizeof s_slot);
    fl_last = 0xFF;
}

char fl_nm[FONT_LEN];                    /* (main RAM: the kernel and fcache.c read it) */
const char *fcl_name;                   /* fcache.c's request and result */
unsigned char fcl_size, fcl_bank, fcl_h, fcl_a, fcl_full, fcl_bank2, fcl_split, fcl_bank3, fcl_split2;
unsigned int fcl_base, fcl_rowlen;
unsigned char *fcl_w;

void FASTCALL fl_style(unsigned char s)
{
    unsigned char k;
    if (s == fl_last)
        return;                         /* (word wrap asks again and again) */
    if ((k = s_slot[s]) == 0xFF) {
        wcur = wlru ? wb1 : wb0;
        wbk[wlru] = 0xFF;
        fl_s = s;
        fc_setup();                     /* (fcache.c, overlay 8) */
        k = s_slot[s];
    }
    if (wbk[0] == k) {                  /* (W38: from the row bank when in neither) */
        fl_w = wb0;
        wlru = 1;
    } else if (wbk[1] == k) {
        fl_w = wb1;
        wlru = 0;
    } else {
        fl_w = wlru ? wb1 : wb0;
        bk_rd(rr_bank, WT_ADDR + k * NW, (unsigned char *)fl_w, NW);
        wbk[wlru] = k;
        wlru ^= 1;
    }
    fl_bold = sty[s].flags & SF_BOLD;
    fl_a = fs_a[k];
    fl_d = fs_d[k];
    fl_slot = k;
    fl_last = s;
}

/* style s for render.s: its font cache and mode; 1: the kernel's font
   (the row is the kernel's to draw) */
unsigned char FASTCALL fl_rr(unsigned char s)
{
    unsigned char k;
    fl_style(s);
    k = fl_slot;
    if (fs_kind[k] != 1)
        return 1;
    rr_fb = fs_bank[k];
    rr_fb2 = fs_bank2[k];
    rr_split = fs_split[k];
    rr_fb3 = fs_bank3[k];
    rr_split2 = fs_split2[k];
    rr_fbase = fs_base[k];
    rr_rowlen = fs_rowlen[k];
    rr_h = fs_h[k];
    rr_a = fs_a[k];
    rr_w = fl_w;
    rr_mode = sty[s].flags & SF_KNOWN;
    return 0;
}

/* n bytes from a of bank b to dst (main RAM) */
void bk_rd(unsigned char b, unsigned int a, unsigned char *dst, unsigned int n)
{
    BANK_SAVE();
    BANK_MAP(b);
    memcpy(dst, (void *)a, n);
    BANK_RESTORE();
}

/* n bytes from src (main RAM) to a of bank b */
void bk_wr(unsigned char b, unsigned int a, const unsigned char *src, unsigned int n)
{
    BANK_SAVE();
    BANK_MAP(b);
    memcpy((void *)a, src, n);
    BANK_RESTORE();
}

/* the platform's banks: Veranda's BankAlloc */
unsigned char FASTCALL plat_bank_run(unsigned char n)
{
    return BankAlloc(n);
}

void FASTCALL plat_bank_free(unsigned char b)
{
    BankFree(b);
}

/* Veranda's free banks (the switcher's summary) */
static unsigned char tsum[TASK_SUM];

unsigned char plat_free(void)
{
    TaskCtl(TASK_INFO, 0xFF, tsum);
    return tsum[TS_FREE];
}

void fonts_init(void)
{
    fl_flush();
    rr_bank = BankAlloc(1);
    rr_tables();
}
