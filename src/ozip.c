/* Veranda Write: a ZIP's parts read and inflated, for opening .docx and
   .odt documents (DECISIONS W49; office.h).

   From Commander Calc's src/import/zip.c and inflate.c (MIT, Copyright
   (c) 2026 Marcin Spoczynski): the central directory found from the end
   of the file (the end record's place, then each entry's name, method,
   sizes and local header), a part read from its local header's data on;
   DEFLATE (RFC 1951) decoded with canonical Huffman tables (a count of
   codes of each length and the symbols in order: no lookup table), the
   32 KB window in four RAM banks. Reworked for Veranda Write: one stream
   at a time, so the state is this file's statics (cc65's code for them is
   a third of a struct's through a pointer); the window written and copied
   by v/owin.s in main RAM (a bank mapped at $A000 while this overlay is
   there too: main RAM's code does it), the literals of a buffer written
   to it together; the file read through ZBUF (main RAM: the kernel's
   FRead).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "office.h"
#include "oimp.h"
#ifdef CW_HOST
unsigned int (*oz_rd)(void *p, unsigned int n);
unsigned char (*oz_seek)(unsigned long off);
unsigned long oz_size;
#define ZREAD(p, n) oz_rd(p, n)
#define ZSEEK(o) oz_seek(o)
#else
#include <veranda.h>
#pragma code-name ("OVERLAY17")
#pragma rodata-name ("OVL17RO")
#pragma bss-name ("OVL17BSS")
#define oz_open e_oz_open
#define oz_part e_oz_part
#define oz_read e_oz_read
#define oz_close e_oz_close
static unsigned char fh;
#define ZREAD(p, n) FRead(fh, p, n)
#define ZSEEK(o) (FSeek(fh, (unsigned int)(o), (unsigned int)((o) >> 16)) != 0)
#endif

/* the window (v/owin.s; the host's below): its banks, the next byte's
   place in it (0 .. 32767); ow_flush writes ow_n bytes from ow_src into
   it, ow_copy copies ow_n bytes from ow_dist back to its end and to
   ow_src */
extern unsigned char ow_bank[4];
extern unsigned int ow_pos, ow_n, ow_dist;
extern unsigned char *ow_src;
void ow_flush(void);
void ow_copy(void);

#ifdef CW_HOST
unsigned char oz_err;                   /* (the X16's: main.c, main RAM: the readers read it) */
#endif

/*---------------------------------------------------------------
  the central directory
 ---------------------------------------------------------------*/
static unsigned long cd_off, z_left, z_at;
static unsigned int cd_n;
static unsigned char z_meth, z_open, wbanks;

static unsigned int le16(const unsigned char *p)
{
    return p[0] | (unsigned int)p[1] << 8;
}

static unsigned long le32(const unsigned char *p)
{
    return le16(p) | (unsigned long)le16(p + 2) << 16;
}

static unsigned char FASTCALL rd_at(unsigned long off, unsigned int n)
{
    return ZSEEK(off) || ZREAD(ZBUF, n) != n;
}

/* the end record: in the last 96 bytes (no comment) or up to 1 KB
   before them (a comment) */
unsigned char FASTCALL oz_open(unsigned char h)
{
    static unsigned long size, at;
    static unsigned char i;
#ifdef CW_HOST
    (void)h;
    size = oz_size;
#else
    fh = h;
    FSize(h);
    size = r0 | (unsigned long)r1 << 16;
#endif
    z_open = 0;
    oz_err = OZ_BAD;
    if (size < 22)
        return OZ_BAD;
    /* (the file's last byte never read: the DOS keeps a stream at its
       end there, a seek after does not read again; the record's last two
       bytes, its comment's length, are not needed) */
    --size;
    at = size > ZBUFN ? size - ZBUFN : 0;
    for (;;) {
        i = (unsigned char)(size - at > ZBUFN ? ZBUFN : size - at);
        if (rd_at(at, i))
            return oz_err = OZ_IO;
        for (i -= 21;; --i) {
            if (ZBUF[i] == 'P' && ZBUF[i + 1] == 'K' && ZBUF[i + 2] == 5 && ZBUF[i + 3] == 6) {
                cd_n = le16(ZBUF + i + 10);
                cd_off = le32(ZBUF + i + 16);
                return oz_err = cd_off < size ? 0 : OZ_BAD;
            }
            if (!i)
                break;
        }
        if (!at || size - at > 1200)
            break;
        at = at > ZBUFN - 20 ? at - (ZBUFN - 20) : 0;   /* (windows overlapping by a record's 20 bytes) */
    }
    return OZ_BAD;
}

/*---------------------------------------------------------------
  inflate's state: the bits, the tables, a block
 ---------------------------------------------------------------*/
static unsigned int zi, zn;             /* ZBUF's bytes: the next, there are */
static unsigned char bb, bc;            /* the bit buffer, its bits */
static unsigned char st, last;          /* the state (below), the last block */
#define ST_HEAD 0
#define ST_STORED 1
#define ST_CODES 2
#define ST_DONE 3
static unsigned int blk;                /* a stored block's bytes left */
static unsigned int clen, cdist;        /* a match not done */
static unsigned char ierr;
static unsigned int lcnt[16], dcnt[16];
/* the symbols in order: a byte each, the literal and length codes' ninth
   bits apart (lhi: 288 bits; the overlay's room) */
static unsigned char lsym[288], lhi[36], dsym[32];
static unsigned char lens[320];

static unsigned char inb(void)
{
    if (zi == zn) {
        zn = z_left > ZBUFN ? ZBUFN : (unsigned int)z_left;
        if (!zn || ZREAD(ZBUF, zn) != zn) {
            ierr = 1;
            zn = 0;
            return 0;
        }
        z_left -= zn;
        zi = 0;
    }
    return ZBUF[zi++];
}

static unsigned int FASTCALL bits(unsigned char n)
{
    static unsigned int v, m;
    v = 0;
    for (m = 1; n; --n, m <<= 1) {
        if (!bc) {
            bb = inb();
            bc = 8;
        }
        if (bb & 1)
            v |= m;
        bb >>= 1;
        --bc;
    }
    return v;
}

/* a code from count c, symbols s (canonical: codes of a length in a row;
   hi: their ninth bits, or none) */
static int dcode(const unsigned int *c, const unsigned char *s, const unsigned char *hi)
{
    static int code, first, index, n;
    static unsigned int k;
    static unsigned char len;
    code = first = index = 0;
    for (len = 1; len < 16; ++len) {
        if (!bc) {
            bb = inb();
            bc = 8;
        }
        code |= bb & 1;
        bb >>= 1;
        --bc;
        n = c[len];
        if (code - first < n) {
            k = index + code - first;
            return hi && (hi[k >> 3] & (1 << (k & 7))) ? s[k] + 256 : s[k];
        }
        index += n;
        first = (first + n) << 1;
        code <<= 1;
    }
    ierr = 1;
    return -1;
}

static void build(unsigned int *c, unsigned char *s, unsigned char *hi, unsigned int base, unsigned int n)
{
    static unsigned int offs[16], i, o;
    static unsigned char l;
    memset(c, 0, 16 * sizeof *c);
    if (hi)
        memset(hi, 0, 36);
    for (i = 0; i < n; ++i)
        ++c[lens[base + i]];
    c[0] = 0;
    offs[1] = 0;
    for (l = 1; l < 15; ++l)
        offs[l + 1] = offs[l] + c[l];
    for (i = 0; i < n; ++i)
        if ((l = lens[base + i]) != 0) {
            o = offs[l]++;
            s[o] = (unsigned char)i;
            if (i > 255)
                hi[o >> 3] |= 1 << (o & 7);
        }
}

static const unsigned char clo[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static void tables(unsigned char dyn)
{
    static unsigned int nl, nd, nc, i, r;
    static int sym;
    static unsigned char v;
    if (!dyn) {
        memset(lens, 8, 144);
        memset(lens + 144, 9, 112);
        memset(lens + 256, 7, 24);
        memset(lens + 280, 8, 8);
        build(lcnt, lsym, lhi, 0, 288);
        memset(lens, 5, 30);
        build(dcnt, dsym, 0, 0, 30);
        return;
    }
    nl = bits(5) + 257;
    nd = bits(5) + 1;
    nc = bits(4) + 4;
    if (nl > 286 || nd > 30) {
        ierr = 1;
        return;
    }
    memset(lens, 0, 19);
    for (i = 0; i < nc; ++i)
        lens[clo[i]] = (unsigned char)bits(3);
    build(dcnt, dsym, 0, 0, 19);
    for (i = 0; i < nl + nd && !ierr;) {
        sym = dcode(dcnt, dsym, 0);
        if (sym < 0)
            return;
        if (sym < 16) {
            lens[i++] = (unsigned char)sym;
            continue;
        }
        v = 0;
        if (sym == 16) {
            if (!i) {
                ierr = 1;
                return;
            }
            v = lens[i - 1];
            r = 3 + bits(2);
        } else if (sym == 17)
            r = 3 + bits(3);
        else
            r = 11 + bits(7);
        if (i + r > nl + nd) {
            ierr = 1;
            return;
        }
        memset(lens + i, v, r);
        i += r;
    }
    build(lcnt, lsym, lhi, 0, nl);
    build(dcnt, dsym, 0, nl, nd);
}

static const unsigned int lbase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
                                        59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const unsigned char lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4,
                                        5, 5, 5, 5, 0 };
static const unsigned int dbase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513,
                                        769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const unsigned char dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10,
                                        11, 11, 12, 12, 13, 13 };

/*---------------------------------------------------------------
  a part
 ---------------------------------------------------------------*/
static void wfree(void)
{
    while (wbanks)
        bk_put(ow_bank[--wbanks]);
}

void oz_close(void)
{
    wfree();
    z_open = 0;
}

/* the entry named name (main RAM) of the central directory: its data's
   place, sizes, method; inflate set up (the window's banks) */
unsigned char FASTCALL oz_part(const char *name)
{
    static unsigned long at;
    static unsigned int i, nl, k, skip;
    static unsigned char hit;
    k = (unsigned int)strlen(name);
    at = cd_off;
    z_open = 0;
    for (i = 0; i < cd_n; ++i) {
        if (rd_at(at, 46) || ZBUF[0] != 'P' || ZBUF[1] != 'K' || ZBUF[2] != 1 || ZBUF[3] != 2)
            return oz_err = OZ_BAD;
        nl = le16(ZBUF + 28);
        skip = 46 + nl + le16(ZBUF + 30) + le16(ZBUF + 32);
        hit = 0;
        if (nl == k) {
            z_meth = ZBUF[10];
            z_left = le32(ZBUF + 20);
            z_at = le32(ZBUF + 42);
            if (k <= ZBUFN && ZREAD(ZBUF, k) == k && !memcmp(ZBUF, name, k))
                hit = 1;
        }
        if (hit)
            break;
        at += skip;
    }
    if (!hit)
        return oz_err = OZ_NONE;
    if (z_meth != 0 && z_meth != 8)
        return oz_err = OZ_BAD;
    /* the local header: its name's and extra field's lengths */
    if (rd_at(z_at, 30) || ZBUF[0] != 'P' || ZBUF[1] != 'K' || ZBUF[2] != 3 || ZBUF[3] != 4)
        return oz_err = OZ_BAD;
    if (ZSEEK(z_at + 30 + le16(ZBUF + 26) + le16(ZBUF + 28)))
        return oz_err = OZ_IO;
    if (z_meth == 8)
        while (wbanks < 4) {
            if (!(ow_bank[wbanks] = bk_get())) {
                wfree();
                return oz_err = OZ_MEM;
            }
            ++wbanks;
        }
    zi = zn = 0;
    bc = 0;
    st = ST_HEAD;
    last = 0;
    clen = 0;
    ierr = 0;
    ow_pos = 0;
    z_open = 1;
    return oz_err = 0;
}

/* the window: the literals since fl (dst's) written to it */
static unsigned char *fl;
static unsigned char *FASTCALL flush(unsigned char *o)
{
    if (o != fl) {
        ow_src = fl;
        ow_n = (unsigned int)(o - fl);
        ow_flush();
    }
    return o;
}

unsigned int FASTCALL oz_read(unsigned char *dst, unsigned int n)
{
    static unsigned char *o, *e;
    static int sym;
    static unsigned int k, x;
    static unsigned char t;
    if (!z_open)
        return 0;
    if (z_meth == 0) {
        if (n > z_left)
            n = (unsigned int)z_left;
        if (!n)
            return 0;
        if (ZREAD(dst, n) != n) {
            oz_err = OZ_IO;
            return 0;
        }
        z_left -= n;
        return n;
    }
    o = fl = dst;
    e = dst + n;
    while (o < e && !ierr && st != ST_DONE) {
        if (clen) {
            /* a match: from the window, to it and to dst */
            o = fl = flush(o);
            k = (unsigned int)(e - o);
            if (k > clen)
                k = clen;
            ow_src = o;
            ow_n = k;
            ow_dist = cdist;
            ow_copy();
            o += k;
            fl = o;
            clen -= k;
            continue;
        }
        if (st == ST_CODES) {
            sym = dcode(lcnt, lsym, lhi);
            if (sym < 256) {
                if (sym >= 0)
                    *o++ = (unsigned char)sym;
                continue;
            }
            if (sym == 256) {
                st = last ? ST_DONE : ST_HEAD;
                continue;
            }
            sym -= 257;
            if (sym >= 29) {
                ierr = 1;
                break;
            }
            clen = lbase[sym] + bits(lext[sym]);
            sym = dcode(dcnt, dsym, 0);
            if (sym < 0 || sym >= 30) {
                ierr = 1;
                break;
            }
            cdist = dbase[sym] + bits(dext[sym]);
            continue;
        }
        if (st == ST_STORED) {
            if (!blk) {
                st = last ? ST_DONE : ST_HEAD;
                continue;
            }
            *o++ = inb();
            --blk;
            continue;
        }
        /* a block's header */
        last = (unsigned char)bits(1);
        t = (unsigned char)bits(2);
        if (t == 0) {
            bc = 0;                     /* (a stored block starts at a byte) */
            blk = inb();
            blk |= inb() << 8;
            x = inb();
            x |= inb() << 8;
            if ((unsigned int)~x != blk)
                ierr = 1;
            st = ST_STORED;
        } else if (t == 3)
            ierr = 1;
        else {
            tables(t == 2);
            st = ST_CODES;
        }
    }
    flush(o);
    if (ierr) {
        oz_err = OZ_BAD;
        z_open = 0;
    }
    return (unsigned int)(o - dst);
}

#ifdef CW_HOST
/* the window on the host: the banks' arrays */
unsigned char ow_bank[4];
unsigned int ow_pos, ow_n, ow_dist;
unsigned char *ow_src;

void ow_flush(void)
{
    while (ow_n--) {
        host_bank[ow_bank[ow_pos >> 13]][ow_pos & 0x1FFF] = *ow_src++;
        ow_pos = (ow_pos + 1) & 0x7FFF;
    }
}

void ow_copy(void)
{
    unsigned int p = (ow_pos - ow_dist) & 0x7FFF;
    unsigned char c;
    while (ow_n--) {
        c = host_bank[ow_bank[p >> 13]][p & 0x1FFF];
        p = (p + 1) & 0x7FFF;
        host_bank[ow_bank[ow_pos >> 13]][ow_pos & 0x1FFF] = c;
        ow_pos = (ow_pos + 1) & 0x7FFF;
        *ow_src++ = c;
    }
}
#endif

