/* Veranda Write on Veranda: the font cache (overlay 8): Veranda
   Write's own copies of the fonts it draws (render.s draws them; fonts.c
   keeps the slots), read from the fonts' files as GEOS keeps them (a
   record file of type FONT, a record a point size: Veranda's
   tools/fontcvt.py has the layout; the file VR_FONTSPATH "name.cvt", as
   Veranda's font service finds it).

   A font in the cache: 192 offsets (each glyph's place in a row block:
   their low bytes, then their high bytes), then a row block for each of
   its rows: every glyph's pixels of that row left aligned, (width + 7) /
   8 bytes each (glyphs up to 48 pixels wide, 48 rows high). A glyph a
   slot (CSLOT, cw.h): characters $20-$7E slots 0-94, $A0-$FF slots
   96-191 (the Veranda set, ISO-8859-16: Veranda's V135) from a font
   record that has them (Veranda's V136: its index 193 words, the bitmap
   386 bytes after it); a slot the font has no glyph for (an older font,
   a width of 0) is '?''s. In a font bank (a RAM bank of the pool,
   several fonts a bank); a font larger than a bank (Gate Sans 27: 8.7 KB,
   17 KB with $A0-$FF) in two or three banks of its own, its first rows
   in the first (after the offsets) and the others from the next ones'
   starts (fcl_split, fcl_split2: the first row there; W38).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cwv.h"
#pragma code-name ("OVERLAY8")
#pragma rodata-name ("OVL8RO")
#define fc_load e_fc_load              /* (called through glue.s) */
#define fc_reset e_fc_reset
#define fc_setup e_fc_setup

#define FCB 5                           /* font banks at most (W38: a font in 3; W71: 4 held Gate Sans 27
                                           and 18, not 12 too: each heading's font read again) */
static unsigned char fc_bank[FCB] = { 0 };   /* (DATA: main RAM) */
static unsigned int fc_used[FCB] = { 0 };
#define OFFS (2 * NW)                   /* the offsets' bytes */

#define ROWB  384                       /* iobuf: a row of the file's bitmap (W71: after it */
#define FX_GL 128                       /*   fx_gl, a row's glyphs on their way to the bank) */

/* (W71) v/fcrow.s: a row's glyphs, the window's facts (its variables,
   OVL8BSS) */
extern unsigned int fx_idx[193];        /* each glyph's first pixel in the bitmap's rows */
#define idx fx_idx
extern unsigned char fx_k, fx_n, fx_ng;
extern unsigned int fx_wb, fx_have, fx_setw;
#define setw fx_setw
#define wb fx_wb
#define have fx_have
#define ng fx_ng
unsigned char fc_xrow(void);
static unsigned char fh;

static unsigned char rd(void *p, unsigned int n)
{
    return FRead(fh, p, n) != n;
}

static unsigned char seek(unsigned long o)
{
    return FSeek(fh, (unsigned int)o, (unsigned int)(o >> 16)) != 0;
}

/* the font fcl_name at the point size nearest fcl_size: into the cache;
   0 done (fcl_ the slot's facts, the widths at fcl_w), 1 not */
#pragma bss-name (push, "OVL8BSS")
static unsigned char hd[8], n, bd, d, k, kb, w, nb, kk[3], fb;
static unsigned long o, ob, bo, rp;
static unsigned int i, need, b, ro, fd;
#pragma bss-name (pop)

/* row i's window from byte wb: read (iobuf), a 0 past it */
static unsigned char win(void)
{
    have = setw - wb;
    if (have > ROWB - 2)
        have = ROWB - 2;
    if (seek(rp + wb) || rd(iobuf, have))
        return 1;
    iobuf[have] = iobuf[have + 1] = 0;
    return 0;
}

unsigned char fc_load(void)
{
    unsigned char *gl = iobuf + ROWB;   /* (main RAM: bk_wr reads it with the font bank mapped) */
    unsigned char *t = iobuf;
    char *q = (char *)iobuf;
    fcl_full = 0;
    q = strcpy(q, VR_FONTSPATH) + sizeof VR_FONTSPATH - 1;
    q = strcpy(q, fcl_name) + strlen(fcl_name);
    strcpy(q, ".cvt");
    fh = FOpen(iobuf, 'r');
    if (fh < 2)
        return 1;
    /* the record table: the size nearest the one asked for */
    if (seek(508) || rd(t, 254))
        goto fail;
    o = 762;
    bd = 255;
    for (n = 0; n < 127; ++n) {
        if (!t[2 * n] && !t[2 * n + 1])
            break;
        if (t[2 * n]) {
            d = n > fcl_size ? n - fcl_size : fcl_size - n;
            if (d < bd) {
                bd = d;
                ob = o;
            }
            o += 254UL * t[2 * n];
        }
    }
    if (bd == 255 || seek(ob) || rd(hd, 8))
        goto fail;
    /* its head: the baseline, the set width, the height, the index's
       place, the bitmap's; $A0-$FF too when the bitmap is 386 bytes
       after the index (Veranda's V136) */
    setw = hd[1] | hd[2] << 8;
    fcl_h = hd[3];
    fcl_a = hd[0];
    i = hd[4] | hd[5] << 8;
    b = hd[6] | hd[7] << 8;
    bo = ob + b;
    ng = b - i == 386 ? NW : 96;
    if (fcl_h > 48 || seek(ob + i) || rd(idx, ng == NW ? 386 : 194))
        goto fail;
    /* the widths, each glyph's place in a row block (the offsets: low
       bytes at iobuf, high at + NW; a missing $A0-$FF '?''s) */
    ro = 0;
    for (k = 0; k < NW; ++k) {
        w = k < ng ? (unsigned char)(idx[k + 1] - idx[k]) : 0;
        if (w > 48)
            goto fail;
        b = ro;
        if (k >= 96 && !w) {            /* (none: '?') */
            w = fcl_w['?' - 32];
            b = iobuf['?' - 32] | iobuf[NW + '?' - 32] << 8;
        } else
            ro += (w + 7) >> 3;
        fcl_w[k] = w;
        iobuf[k] = (unsigned char)b;
        iobuf[NW + k] = b >> 8;
    }
    fcl_rowlen = ro;
    /* room: in a font bank (a new one from the pool if none has it); two
       or three new ones for a font larger than a bank (W38) */
    need = OFFS + fcl_h * ro;
    fcl_split = fcl_split2 = 0xFF;
    fcl_bank2 = fcl_bank3 = 0;
    if (need > BANK_SIZE) {
        fcl_split = (unsigned char)((BANK_SIZE - OFFS) / ro);
        d = (unsigned char)(BANK_SIZE / ro);
        nb = 2;
        if (fcl_h - fcl_split > d) {
            nb = 3;
            fcl_split2 = fcl_split + d;
            if (fcl_h - fcl_split2 > d)
                goto fail;              /* (too large) */
        }
        for (kb = n = 0; kb < FCB; ++kb)
            if (!fc_bank[kb] && n < nb)
                kk[n++] = kb;
        if (n < nb)
            goto full;
        for (n = 0; n < nb; ++n) {
            kb = kk[n];
            if (!(fc_bank[kb] = bk_get()))
                goto full;
            fc_used[kb] = BANK_SIZE;
        }
        kb = kk[0];
        fcl_bank2 = fc_bank[kk[1]];
        if (nb == 3)
            fcl_bank3 = fc_bank[kk[2]];
        fc_used[kb] = 0;
        need = BANK_SIZE;               /* (all full once it is in) */
    } else for (kb = 0; kb < FCB; ++kb) {
        if (!fc_bank[kb]) {
            fc_used[kb] = 0;
            if (!(fc_bank[kb] = bk_get()))
                goto full;
        }
        if (fc_used[kb] + need <= BANK_SIZE)
            break;
    }
    if (kb == FCB)
        goto full;
    fcl_bank = fc_bank[kb];
    fcl_base = 0xA000 + fc_used[kb];
    bk_wr(fcl_bank, fcl_base, iobuf, OFFS);
    /* the rows: each glyph's pixels out of the bitmap's row, read a
       window of it at a time (a row with $A0-$FF may be longer than
       iobuf: W38); v/fcrow.s takes them out (W71), fx_gl's bytes go to
       the bank a row's place at a time */
    rp = bo;
    for (i = 0; i < fcl_h; ++i, rp += setw) {
        if (i < fcl_split) {
            fb = fcl_bank;
            fd = fcl_base + OFFS + i * fcl_rowlen;
        } else if (i < fcl_split2) {
            fb = fcl_bank2;
            fd = 0xA000 + (i - fcl_split) * fcl_rowlen;
        } else {
            fb = fcl_bank3;
            fd = 0xA000 + (i - fcl_split2) * fcl_rowlen;
        }
        wb = 0;
        if (win())
            goto fail;
        fx_k = 0;
        do {
            fx_n = 0;
            d = fc_xrow();
            if (fx_n) {
                bk_wr(fb, fd, gl, fx_n);
                fd += fx_n;
            }
            if (d == 2 && win())
                goto fail;
        } while (d);
    }
    FClose(fh);
    fc_used[kb] += need;
    return 0;
full:
    fcl_full = 1;
fail:
    FClose(fh);
    return 1;
}

/* the cache emptied (its banks back to the pool) */
void fc_reset(void)
{
    unsigned char k;
    for (k = 0; k < FCB; ++k) {
        bk_put(fc_bank[k]);
        fc_bank[k] = 0;
        fc_used[k] = 0;
    }
}

/*---------------------------------------------------------------
  style fl_s's font slot (fonts.c's tables, main RAM)
 ---------------------------------------------------------------*/

/* the kernel's handle of font fl_nm at its nearest size (0: the system font) */
static unsigned char kload(unsigned char size)
{
    unsigned char h, i, best, d, bd;
    h = LoadFont(fl_nm, size);
    if (h != FONT_NONE)
        return h;
    memset(iobuf, 0, 64);
#define sizes iobuf
    if (FontSizes(fl_nm, sizes) && sizes[0]) {
        best = sizes[0];
        bd = 255;
        for (i = 0; i < 64 && sizes[i]; ++i) {
            d = sizes[i] > size ? sizes[i] - size : size - sizes[i];
            if (d < bd) {
                bd = d;
                best = sizes[i];
            }
        }
        h = LoadFont(fl_nm, best);
        if (h != FONT_NONE)
            return h;
    }
    return 0;
}

/* a slot for a new font: the next, round (its cache's room is not given
   back: when the cache is full it is emptied, fc_reset) */
static unsigned char slot(void)
{
    unsigned char k = fs_next, c;
    fl_last = 0xFF;                     /* (fonts.c: its last style's slot may go) */
    fs_next = (fs_next + 1) % NFS;
    for (c = 0; c < MAXSTY; ++c)
        if (s_slot[c] == k)
            s_slot[c] = 0xFF;
    fs_kind[k] = 0;
    return k;
}

void fc_setup(void)
{
    unsigned char s = fl_s;
    unsigned char k, size, c, h;
    unsigned int key = 0;
    const char *p;
    size = sty[s].size;
    strcpy(fl_nm, fontname[sty[s].font]);
    for (p = fl_nm; *p; ++p)
        key = key * 31 + (unsigned char)*p;
    for (k = 0; k < NFS; ++k)
        if (fs_kind[k] && fs_key[k] == key && fs_size[k] == size)
            goto found;
    /* the cache: once, and once more after emptying it when it is full */
    for (c = 0; c < 2; ++c) {
        k = slot();
        fcl_name = fl_nm;
        fcl_size = size;
        fcl_w = wcur;
        if (!fc_load() || (!fcl_full && strcmp(fl_nm, def_font) && (strcpy(fl_nm, def_font), !fc_load()))) {
            fs_kind[k] = 1;
            fs_bank[k] = fcl_bank;
            fs_bank2[k] = fcl_bank2;
            fs_split[k] = fcl_split;
            fs_bank3[k] = fcl_bank3;
            fs_split2[k] = fcl_split2;
            fs_base[k] = fcl_base;
            fs_rowlen[k] = fcl_rowlen;
            fs_h[k] = fcl_h;
            fs_a[k] = fcl_a;
            fs_d[k] = fcl_h - fcl_a;
            goto keyed;
        }
        if (!fcl_full)
            break;                      /* (no such font, or too large) */
        fc_reset();
        for (k = 0; k < NFS; ++k)
            if (fs_kind[k] == 1)
                fs_kind[k] = 0;
        fl_flush();
    }
    /* the kernel's */
    strcpy(fl_nm, fontname[sty[s].font]);
    h = kload(size);
    k = slot();
    fs_kind[k] = 2;
    FontInfo(h, 0);
    fs_a[k] = r1H;
    fs_d[k] = r1L - r1H;
    UseFont(h);
    currentMode = 0;
    for (c = 0; c < 96; ++c)
        wcur[c] = GetCharWidth(c + 32);
    for (c = 96; c < NW; ++c)           /* ($A0-$FF: the kernel's '?' when the font has none, V136) */
        wcur[c] = GetCharWidth(c + 64);
    UseFont(0);
keyed:
    bk_wr(rr_bank, WT_ADDR + k * NW, wcur, NW);     /* (W38: the widths in the row bank, */
    if (wbk[wlru ^ 1] == k)             /* and in fonts.c's buffer wcur is) */
        wbk[wlru ^ 1] = 0xFF;
    wbk[wlru] = k;
    fs_key[k] = key;
    fs_size[k] = size;
found:
    s_slot[s] = k;
}

