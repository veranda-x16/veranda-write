/* Veranda Write: geoWrite documents opened, converted to Veranda
   Write's own (DECISIONS W24-W27; docs/FORMAT.md, "geoWrite documents").

   Written from public descriptions of geoWrite's file format only (no
   code or data of geoWrite's or any other GEOS program's: W1): Michael
   Steil's "Inside geoWrite - 7: File Format and Pagination"
   (pagetable.com, 2020) for the records, the escapes and the ruler;
   Veranda's docs/APPFORMAT.md for the CVT file a GEOS file is on the
   card; Peter Schepers' GEOS.TXT for the info block's fields.

   A geoWrite document is a GEOS record file of class "Write Image V1.x"
   or "V2.x", on the card in CVT form: the directory entry and the
   signature (254 bytes), the info block (254), the record table (254: a
   record's blocks, its bytes in the last block + 1; 0/0 ends it, 0/$FF
   an empty record), then each record padded to whole blocks of 254
   bytes. Records 0-60 are the pages, 61 the header, 62 the footer, 64-126
   the pictures (photo scraps). A page is text: characters $20-$7E, TAB,
   CR (a paragraph's end), PAGE_BREAK ($0C), $00 (the end), and escapes:
   a ruler ($11 and 26 bytes: the left and right margins, 8 tab stops,
   the paragraph's first line's margin, the alignment and the line
   spacing), a font ($17, the font's id word: id << 6 | size, the style
   byte), a picture ($10: its width in cards, its height, its record).

   The file is read once from its start (the X16's stream, the host's
   callback), a page at a time through iobuf; the text goes into the
   document's chunked buffer as format.c's reader puts it there. What
   Veranda Write has no place for is counted (gw_...) and gw_note says
   what was left out.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cw.h"
#ifdef CW_HOST
cw_rfn gw_rf;
#define IN(p, n) gw_rf(p, n)
#else
#include <veranda.h>
#pragma code-name ("OVERLAY10")
#pragma rodata-name ("OVL10RO")
#pragma data-name ("OVL10DATA")
#pragma bss-name ("OVL10BSS")          /* (its variables: none is read from elsewhere) */
#define gw_check e_gw_check
#define gw_load e_gw_load
#define gw_note e_gw_note
#define gw_scan e_gw_scan
#define gw_filter e_gw_filter
static unsigned char fh;
#define IN(p, n) FRead(fh, p, n)
#endif

/* iobuf (main RAM: the kernel, cbuf.c and overlay 2's styles read it):
   the record's bytes, the text on its way into the document, a
   paragraph style and a font name for par_find and font_index */
#define IB   iobuf
#define OB   (iobuf + 256)
#define OBN  128
#define PSB  ((struct pstyle *)(iobuf + 384))
#define FNB  ((char *)iobuf + 400)

static unsigned char tb[254];           /* the record table */
static unsigned char rb[26];            /* an escape's bytes */
static unsigned int left, bi, bn;       /* the record's bytes not read yet; the buffer's */
static unsigned char on, err;
static unsigned char cs, ts;            /* the style written last; the text's now */
static unsigned char nps;               /* the paragraph style in force (the ruler's) */
static unsigned char at_start, brk, pic;

/* what was found and could not be kept (gw_note says it; the host's
   tests read it) */
unsigned char gw_pages, gw_pics, gw_hf, gw_tabs, gw_sty;
unsigned int gw_bad;

/* h: the file's first 508 bytes. 0: not a GEOS file; 1: a GEOS file
   but not a geoWrite document; 2: a geoWrite document */
unsigned char FASTCALL gw_check(const unsigned char *h)
{
    if (memcmp(h + 34, "formatted GEOS file", 19))
        return 0;
    if (h[21] != 1 || memcmp(h + 329, "Write Image V", 13))
        return 1;
    return 2;
}

static void put(unsigned char c)
{
    OB[on++] = c;
    if (on == OBN) {
        if (cb_append(&tx, OB, OBN, BANK_SIZE - 64))
            err = FE_MEM;
        on = 0;
    }
}

static void code(unsigned char k, unsigned char a)
{
    put(k);
    put(a);
    put(k);
}

/* a character, TAB or CR of the text, in style ts: a paragraph's codes
   first (the page break's own paragraph before it; its paragraph style),
   then a style code when the style changed */
static void emit(unsigned char c)
{
    if (pic) {
        pic = 0;
        if (c != CH_CR)
            emit(CH_CR);                /* (a picture is a paragraph of its own) */
    }
    if (at_start) {
        if (brk) {
            code(K_PB, 0);
            put(CH_CR);
        }
        at_start = 0;
        if (nps)
            code(K_PS, nps);
    }
    brk = 0;                            /* (a break inside a paragraph: geoWrite's own, the page full) */
    if (ts != cs) {
        code(K_CS, ts);
        cs = ts;
    }
    put(c);
    if (c == CH_CR)
        at_start = 1;
}

/* the record's next byte; -1 at its end */
static int gb(void)
{
    if (bi == bn) {
        if (!left || err)
            return -1;
        bn = left > 256 ? 256 : left;
        if (IN(IB, bn) != bn) {
            err = FE_IO;
            return -1;
        }
        left -= bn;
        bi = 0;
    }
    return IB[bi++];
}

/* an escape's n bytes into rb; 1: the record ended first */
static unsigned char esc(unsigned char n)
{
    unsigned char i;
    int c;
    for (i = 0; i < n; ++i) {
        if ((c = gb()) < 0)
            return 1;
        rb[i] = (unsigned char)c;
    }
    return 0;
}

#define W(i) (rb[i] | (unsigned int)rb[(i) + 1] << 8)

/* a ruler: the paragraph style of the paragraphs from here (the one it
   starts, or the next one): the margins as indents from the page's
   edges (480 dots wide, 640 when wide), the alignment, the spacing */
static void ruler(void)
{
    static const unsigned char spc[4] = { 2, 3, 4, 2 };
    static unsigned int lm, rm, w, t;
    static int f;
    struct pstyle *p = PSB;
    unsigned char i;
    lm = W(0);
    rm = W(2);
    if (rm <= lm)
        return;                         /* (not a ruler: the one before kept) */
    for (i = 4; i < 20; i += 2) {
        t = W(i) & 0x7FFF;
        if (t > lm && t < rm)
            gw_tabs = 1;                /* (tab stops of its own: Veranda Write's are every half inch) */
    }
    w = rm < 480 ? 479 : 639;
    memset(p, 0, sizeof *p);
    p->align = rb[22] & 3;
    p->spacing = spc[(rb[22] >> 2) & 3];
    p->left = lm > 200 ? 200 : (unsigned char)lm;
    rm = w - rm;
    p->right = rm > 200 ? 200 : (unsigned char)rm;
    f = (int)W(20) - (int)p->left;
    if (f < -(int)p->left)
        f = -(int)p->left;
    if (f > 120)
        f = 120;
    p->first = (signed char)f;
    nps = par_find(p);
    if (nps == 0xFF) {
        nps = 0;
        gw_sty |= 1;
    }
}

/* the font's names by its id (Veranda's fonts; GEOS's first ones, as
   Steil gives them); another: "GEOS font n", drawn in Gate Sans (W7) */
static const char *const fnames[4] = { "System", "University", "California", "Roma" };

static void cardset(void)
{
    static unsigned int id;
    static unsigned char f, fl, sz;
    char *q = FNB;
    sz = rb[0] & 63;
    id = (unsigned int)rb[1] * 4 + (rb[0] / 64);
    if (id < 4)
        strcpy(q, fnames[id]);
    else if (id == 300 || id == 301)
        strcpy(q, id == 300 ? "Gate Sans" : "Gate Mono");
    else {
        strcpy(q, "GEOS font ");
        q += 10;
        if (id >= 1000)
            *q++ = '1';                 /* (ids are 10 bits: up to 1023) */
        if (id >= 100)
            *q++ = (char)('0' + id / 100 % 10);
        if (id >= 10)
            *q++ = (char)('0' + id / 10 % 10);
        *q++ = (char)('0' + id % 10);
        *q = 0;
    }
    f = font_index(FNB);
    if (f == 0xFF) {
        f = 0;
        gw_sty |= 1;
    }
    fl = 0;
    if (rb[2] & 0x48)
        fl |= SF_BOLD;                  /* bold; outline drawn as bold */
    if (rb[2] & 0x10)
        fl |= SF_ITALIC;
    if (rb[2] & 0x80)
        fl |= SF_ULINE;
    if (rb[2] & 0x26)
        gw_sty |= 2;                    /* reverse, superscript, subscript: plain */
    ts = sty_find(f, sz ? sz : DEF_SIZE, fl, 0);
    if (ts == 0xFF) {
        ts = 0;
        gw_sty |= 1;
    }
}

/* a page's text, up to its end ($00 or the record's) */
static void page(void)
{
    static const char ptext[] = "[picture]";
    int c;
    unsigned char i;
    for (;;) {
        c = gb();
        if (c <= 0 || err)
            return;
        if (c == 0x11) {
            if (esc(26))
                return;
            ruler();
        } else if (c == 0x17) {
            if (esc(3))
                return;
            cardset();
        } else if (c == 0x10) {
            if (esc(4))
                return;
            if (!at_start)
                emit(CH_CR);
            for (i = 0; i < 9; ++i)
                emit(ptext[i]);
            pic = 1;
            ++gw_pics;
        } else if (c == 0x0C)
            brk = 1;
        else if (c == CH_TAB || c == CH_CR || (c >= 0x20 && c < 0x7F))
            emit((unsigned char)c);
        else {
            emit('?');                  /* (not a character of geoWrite's text: counted) */
            ++gw_bad;
        }
    }
}

/* the geoWrite document in the file (its first bytes not read yet, on
   the X16 the stream h): the document replaced; 0 done, else FE_ */
unsigned char FASTCALL gw_load(unsigned char h)
{
    static unsigned int n, skip;
    static unsigned char r, b;
#ifndef CW_HOST
    fh = h;
#else
    (void)h;
#endif
    doc_new();
    cb_clear(&tx);
    on = err = 0;
    cs = ts = nps = 0;
    at_start = 1;
    brk = pic = 0;
    gw_pages = gw_pics = gw_tabs = gw_sty = 0;
    gw_bad = 0;
    if (IN(IB, 254) != 254 || IN(IB, 254) != 254 || IN(IB, 254) != 254)
        return FE_IO;
    memcpy(tb, IB, 254);
    gw_hf = (tb[122] ? 1 : 0) | (tb[124] ? 2 : 0);   /* records 61, 62 */
    for (r = 0; r < 61 && !err; ++r) {
        b = tb[2 * r];
        if (!b) {
            if (!tb[2 * r + 1])
                break;                  /* (the table's end) */
            continue;                   /* (an empty record) */
        }
        n = (b - 1) * 254 + (tb[2 * r + 1] ? tb[2 * r + 1] - 1 : 0);
        left = n;
        bi = bn = 0;
        ++gw_pages;
        page();
        /* past what was not read, and the record's padding */
        skip = left + (b * 254 - n);
        while (skip && !err) {
            n = skip > 256 ? 256 : skip;
            if (IN(IB, n) != n)
                err = FE_IO;
            skip -= n;
        }
    }
    if (on && cb_append(&tx, OB, on, BANK_SIZE - 64))
        err = FE_MEM;
    if (!err && tx.total > FMT_MAXTEXT)
        err = FE_BIG;                   /* (the limits of a document: W31) */
    if (err)
        return err;
    fl_flush();
    if (lay_all())
        return FE_MEM;
    doc_modified = 1;                   /* (saved nowhere yet: Untitled) */
    return 0;
}

#ifndef CW_HOST
/*---------------------------------------------------------------
  the Open dialog's list: the folder's geoWrite documents by their class
  (W26). The dialog's filter sees only a name, while the folder is being
  listed, when no file may be opened (CMDR-DOS on the SD card stops a
  listing when a file is opened: Veranda's V16); so the folder is read
  before the dialog opens (its .cvt files' first blocks: the class), and
  the filter shows the .cvt files found to be geoWrite documents
 ---------------------------------------------------------------*/
#define GWN 40
static char gl[GWN][64];                /* (this overlay's bank) */
static unsigned char gln;

static unsigned char is_cvt(const char *s)
{
    unsigned char n = (unsigned char)strlen(s);
    return n > 4 && s[n - 4] == '.' && (s[n - 3] | 0x20) == 'c' && (s[n - 2] | 0x20) == 'v'
           && (s[n - 1] | 0x20) == 't';
}

void gw_scan(void)
{
    unsigned char i, k, h, ok;
    char *nm = (char *)iobuf + 19;      /* (DirNext's entry at iobuf + 16: the name at + 3) */
    gln = 0;
    iobuf[0] = '*';
    iobuf[1] = 0;
    if (DirOpen(iobuf) == 0)
        while (DirNext(iobuf + 16) == 0)
            if (iobuf[16] != 'D' && gln < GWN && is_cvt(nm) && strlen(nm) < 64)
                strcpy(gl[gln++], nm);
    for (i = k = 0; i < gln; ++i) {
        strcpy((char *)iobuf + 256, gl[i]);
        h = FOpen(iobuf + 256, 'r');
        if (h < 2)
            continue;
        ok = FRead(h, iobuf, 508) == 508 && gw_check(iobuf) == 2;
        FClose(h);
        if (ok) {
            if (k != i)
                strcpy(gl[k], gl[i]);
            ++k;
        }
    }
    gln = k;
}

/* the dialog's filter (vr_file_filter, through glue.s): every file but a
   .cvt that is not one of the geoWrite documents found */
unsigned char FASTCALL gw_filter(const char *nm)
{
    unsigned char i;
    if (!is_cvt(nm))
        return 1;
    for (i = 0; i < gln; ++i)
        if (!strcmp(gl[i], nm))
            return 1;
    return 0;
}

void set_status(const char *s);         /* (main.c's) */
void message(const char *a, const char *b);
/* after a conversion: "Converted from geoWrite" in the status bar, and
   what was left out in a box (two short lines: the box is narrow) */
void gw_note(void)
{
    static char b[80];
    static const char *const what[7] = { "header", "footer", "pictures", "tab stops", "sub/superscript",
                                         "chars as ?", "styles" };
    unsigned char m, k, i;
    char *p = b, *q;
    m = (gw_hf & 3) | (gw_pics ? 4 : 0) | (gw_tabs ? 8 : 0) | (gw_sty & 2 ? 16 : 0) | (gw_bad ? 32 : 0)
        | (gw_sty & 1 ? 64 : 0);
    set_status("Converted from geoWrite");
    if (!m)
        return;
    strcpy(p, "Left out: ");
    p += 10;
    for (i = k = 0; i < 7; ++i)
        if (m & (1 << i)) {
            if (k++) {
                *p++ = ',';
                *p++ = ' ';
            }
            strcpy(p, what[i]);
            p += strlen(p);
        }
    /* the first line up to its last ", " before 30 characters */
    q = b + 30;
    if (p > q) {
        while (q > b && !(q[0] == ',' && q[1] == ' '))
            --q;
        *q = 0;
        q += 2;
    } else
        q = p;
    message(b, q);
}
#endif
