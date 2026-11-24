/* Veranda Write: the document file (docs/FORMAT.md) and plain text.

   A file is a header ("CWRD", the major and minor version, flags) and
   chunks: a tag of 4 letters, a 32-bit length, the bytes. A reader skips
   the chunks it does not know, so a later minor version adds chunks (the
   paragraph styles, the page, objects) that this one passes over; a
   later major version is refused. Version 1.0: INFO, FONT, STYL, TEXT,
   END; 1.1 adds PARA (the paragraph styles) before TEXT; 1.4 PAGE (the
   page setup: the paper, the margins, the header and footer; a file
   without it has the user's default, page.c) before TEXT; 1.3 the text's
   set in INFO (byte 6: 0 Latin-1, older files; 1 ISO-8859-16, Veranda's
   set, V135: what this version writes; a Latin-1 text is converted as it
   is read, the letters the set lacks made '?' and counted). The text is the
   document's byte stream as it is in memory; the reader checks it
   (characters and whole codes only, style codes naming a style there
   is) and drops what is not.

   Through callbacks (cw_wfn, cw_rfn): the X16's streams, the host's
   files in the tests.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cw.h"
#ifndef CW_HOST
#pragma code-name ("OVERLAY2")          /* (with the files: overlay 2) */
#pragma rodata-name ("OVL2RO")
#endif

pos_t fmt_cursor;
unsigned int fmt_lost;
unsigned char fmt_cs;

/* the Latin-1 bytes of iobuf[0 .. n) in the set: the 40 places where
   ISO-8859-16 has another letter made '?' and counted (W38; the X16's in
   v/latin.s, overlay 10, a table of 96: six times the C's speed); one
   byte, l1_byte */
#ifdef CW_HOST
static const unsigned char l1_out[12] = {   /* (a bit each, $A0 first) */
    0x7E, 0xAB, 0x3C, 0xEF, 0x14, 0x00, 0xC5, 0x86, 0x14, 0x00, 0xC5, 0x86
};

static unsigned char FASTCALL l1_byte(unsigned char c)
{
    if (c >= 0xA0 && l1_out[(c - 0xA0) >> 3] & 0x80 >> (c & 7)) {
        ++fmt_lost;
        return '?';
    }
    return c;
}

static void FASTCALL l1_conv(unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n; ++i)
        iobuf[i] = l1_byte(iobuf[i]);
}
#else
void FASTCALL l1_conv(unsigned int n);  /* (glue.s: v/latin.s) */
unsigned char FASTCALL l1_byte(unsigned char c);
#endif

static void put4(unsigned char *p, pos_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static pos_t get4(const unsigned char *p)
{
    return p[0] | (unsigned int)p[1] << 8 | (pos_t)p[2] << 16 | (pos_t)p[3] << 24;
}

static cw_wfn wf;
pos_t fmt_toff;                         /* fmt_save: where the text starts in the file */
static pos_t wcount;

static unsigned char wn(const void *p, unsigned int n)
{
    wcount += n;
    return wf(p, n);
}

static unsigned char chunk(const char *tag, pos_t len)
{
    unsigned char h[8];
    memcpy(h, tag, 4);
    put4(h + 4, len);
    return wn(h, 8);
}

unsigned char fmt_save(cw_wfn w0, pos_t cursor)
{
    unsigned char h[8], i, l, e;
    pos_t n, p;
    unsigned int k;
#define w wn
    wf = w0;
    wcount = 0;
    memcpy(h, CW_MAGIC, 4);
    h[4] = CW_MAJOR;
    h[5] = CW_MINOR;
    h[6] = h[7] = 0;
    if (w(h, 8))
        return FE_IO;
    /* INFO: the cursor, the text's width, the set (1.3) */
    memset(h, 0, 8);
    put4(h, cursor);
    h[4] = (unsigned char)wrap_w;
    h[5] = (unsigned char)(wrap_w >> 8);
    h[6] = CS_L10;
    if (chunk("INFO", 8) || w(h, 8))
        return FE_IO;
    n = 1;
    for (i = 0; i < nfont; ++i)
        n += 1 + strlen(fontname[i]);
    if (chunk("FONT", n) || w(&nfont, 1))
        return FE_IO;
    for (i = 0; i < nfont; ++i) {
        l = (unsigned char)strlen(fontname[i]);
        if (w(&l, 1) || w(fontname[i], l))
            return FE_IO;
    }
    if (chunk("STYL", 1 + 4 * (pos_t)nsty) || w(&nsty, 1) || w(sty, 4 * nsty))
        return FE_IO;
    if (chunk("PARA", 1 + sizeof(struct pstyle) * (pos_t)npar) || w(&npar, 1)
        || w(par, sizeof(struct pstyle) * npar) || chunk("PAGE", PGS_N) || w(&pgs, PGS_N))
        return FE_IO;
    if (chunk("TEXT", tx.total))
        return FE_IO;
    fmt_toff = wcount;
    /* (what has been written goes out of RAM first: a pass, sw_scan) */
    sw_scan = 1;
    e = 0;
    for (p = 0; p < tx.total; p += k) {
        k = cb_read(&tx, p, iobuf, IOBUF);
        if (!k || wf(iobuf, k)) {
            e = FE_IO;
            break;
        }
    }
    sw_scan = 0;
    if (e || chunk("END ", 0))
        return FE_IO;
    return 0;
#undef w
}

/*---------------------------------------------------------------
  reading: fmt_begin reads the file up to its text; fmt_step reads the
  text a piece at a time, each piece wrapped as it comes (lay_fix: the
  piece is an insertion at the end), and after the text the rest of the
  file (fmt_finish); fmt_load does it all. So a big document shows its
  first page while the rest is read (the X16's files.c: a step at a time
  from the main loop; DECISIONS W30). The text's bytes that are the
  file's as they are need no copy in the swap (cb_src: the pager reads
  them back from the file).
 ---------------------------------------------------------------*/
static cw_rfn rf;
static unsigned char ob[64], on;        /* the text's bytes checked, on their way in */
static unsigned char oerr;
unsigned char ld_kind;                  /* the text being read: LD_ */
pos_t ld_left;                          /* its bytes not read yet */
pos_t ld_at;                            /* the file's offset of the next byte read */
pos_t ld_size;                          /* the text's bytes in all (plain text: the file's, if known) */
void (*ld_poll)(void);                  /* (when not 0) after each buffer: sets ld_stop to end the step */
unsigned char ld_stop;
static unsigned char st, kind, arg;     /* a code the last buffer cut: its bytes so far */
static pos_t code_at;                   /* ... and where it starts in the file */
static unsigned char cr, lead, more;    /* plain text: CR seen, a UTF-8 sequence */
static unsigned char lb[4], nl;         /* (W38) its bytes so far */
static unsigned int cu;                 /* its code point so far */


static void out(unsigned char c)
{
    ob[on++] = c;
    if (on == sizeof ob) {
        if (cb_append(&tx, ob, on, BANK_SIZE - 64))
            oerr = 1;
        on = 0;
    }
}

static void out_end(void)
{
    if (on && cb_append(&tx, ob, on, BANK_SIZE - 64))
        oerr = 1;
    on = 0;
}

static unsigned int rdn(void *p, unsigned int n)
{
    n = rf(p, n);
    ld_at += n;
    return n;
}

static unsigned char rd(void *p, unsigned int n)
{
    return rdn(p, n) != n;
}

static unsigned char skip(pos_t n)
{
    unsigned int k;
    while (n) {
        k = n > IOBUF ? IOBUF : (unsigned int)n;
        if (rd(iobuf, k))
            return 1;
        n -= k;
    }
    return 0;
}

/* the text, checked in place in iobuf a buffer at a time: characters
   and whole codes kept (ld_filter: v/ldfilt.s on the X16, the C below on
   the host); a code across two buffers is put together here */
#ifdef CW_HOST
unsigned int ld_from, ld_k, ld_i;

unsigned int ld_filter(void)
{
    unsigned int i = ld_from, o = 0;
    unsigned char c;
    while (i < ld_k) {
        c = iobuf[i];
        if (IS_KIND(c)) {
            if (i + 2 >= ld_k)
                break;                  /* (cut: the caller's) */
            if (iobuf[i + 2] == c && (c != K_CS || iobuf[i + 1] < nsty)
                && (c != K_PS || (iobuf[i + 1] && iobuf[i + 1] < npar))) {
                iobuf[o++] = c;
                iobuf[o++] = iobuf[i + 1];
                iobuf[o++] = c;
            }
            i += 3;
            continue;
        }
        if (IS_CHAR(c) || c == CH_CR)
            iobuf[o++] = c;
        ++i;
    }
    ld_i = i;
    return o;
}
#else
extern unsigned int ld_from, ld_k, ld_i;
unsigned int ld_filter(void);           /* v/ldfilt.s */
#endif

/* up to n bytes of a document's text */
static unsigned char load_text(pos_t n)
{
    static unsigned int k, o, k0;
    static unsigned char code[3];
    static pos_t at;
    while (n) {
        k = n > IOBUF ? IOBUF : (unsigned int)n;
        at = ld_at;
        if (rd(iobuf, k))
            return FE_IO;
        n -= k;
        ld_left -= k;
        ld_from = 0;
        while (st && ld_from < k) {     /* the end of a code the last buffer cut */
            if (st == 1) {
                arg = iobuf[ld_from++];
                st = 2;
            } else {
                st = 0;
                if (iobuf[ld_from++] == kind && (kind != K_CS || arg < nsty)
                    && (kind != K_PS || (arg && arg < npar))) {
                    code[0] = code[2] = kind;
                    code[1] = arg;
                    cb_src = code_at;
                    if (cb_append(&tx, code, 3, BANK_SIZE - 64))
                        return FE_MEM;
                }
            }
        }
        ld_k = k;
        o = ld_filter();
        /* (nothing dropped: the bytes are the file's from at + ld_from) */
        cb_src = o == ld_i - ld_from ? at + ld_from : WH_NONE;
        if (fmt_cs == CS_LATIN1) {      /* (1.0-1.2: Latin-1, W38) */
            k0 = fmt_lost;
            l1_conv(o);
            if (fmt_lost != k0)         /* (changed: not the file's bytes, so the pager */
                cb_src = WH_NONE;       /* keeps them in the swap, never reads them again) */
        }
        if (o && cb_append(&tx, iobuf, o, BANK_SIZE - 64))
            return FE_MEM;
        cb_src = WH_NONE;
        if (ld_i < k) {                 /* a code cut by the buffer's end */
            kind = iobuf[ld_i];
            code_at = at + ld_i;
            st = 1;
            if (ld_i + 1 < k) {
                arg = iobuf[ld_i + 1];
                st = 2;
            }
        }
        if (ld_poll) {
            ld_poll();
            if (ld_stop)
                break;
        }
    }
    return 0;
}

/* up to n bytes of a plain text file: CR LF, LF or CR end a paragraph;
   UTF-8's Latin-1 letters become Latin-1, its other characters '?'; a
   byte of 128-255 that is not UTF-8 is Latin-1 as it is; 1: the file's end */
/* (W38) a byte of 128-255 that is not UTF-8: Latin-1, in the set ('?'
   for the 40 it has not, counted) */
#define l1_one(c) ((c) >= 0xA0 ? out(l1_byte(c)) : (void)0)

/* a broken UTF-8 sequence's bytes: Latin-1 */
static void l1_seq(void)
{
    unsigned char j;
    for (j = 0; j < nl; ++j)
        l1_one(lb[j]);
    more = nl = 0;
}

static unsigned char load_plain(pos_t n)
{
    static unsigned int k, i;
    static unsigned char c;
    while (n) {
        k = rdn(iobuf, n > IOBUF ? IOBUF : (unsigned int)n);
        if (!k)
            return 1;
        n -= k;
        for (i = 0; i < k; ++i) {
            c = iobuf[i];
            if (more) {                 /* (W38: UTF-8 in Veranda's set, its vrcs) */
                if ((c & 0xC0) == 0x80) {
                    lb[nl++] = c;
                    cu = cu << 6 | (c & 0x3F);
                    if (!--more) {
                        c = nl == 4 || cu < 0xA0 ? 0 : cs_byte(cu);
                        if (!c) {
                            c = '?';
                            ++fmt_lost;
                        }
                        out(c);
                        nl = 0;
                    }
                    continue;
                }
                l1_seq();               /* not UTF-8 after all: Latin-1 */
            }
            if (cr) {
                cr = 0;
                if (c == 10)
                    continue;           /* CR LF */
            }
            if (c == 13 || c == 10) {
                cr = c == 13;
                out(CH_CR);
            } else if (c >= 0xC2 && c <= 0xF4) {
                lb[0] = c;
                nl = 1;
                more = c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
                cu = c & (c < 0xE0 ? 0x1F : c < 0xF0 ? 0x0F : 0x07);
            } else if (c >= 0xA0)
                l1_one(c);
            else if (IS_CHAR(c))
                out(c);
        }
        if (oerr || tx.total > FMT_MAXTEXT)
            return 0;
        if (ld_poll) {
            ld_poll();
            if (ld_stop)
                break;
        }
    }
    return 0;
}

/* the end of the text: the chunks after it (to END), the cursor */
static unsigned char finish(void)
{
    static unsigned char h[8];
    ld_kind = LD_NONE;
    for (;;) {
        if (rd(h, 8))
            return FE_BAD;              /* (no END) */
        if (!memcmp(h, "END ", 4))
            break;
        if (skip(get4(h + 4)))
            return FE_IO;
    }
    return 0;
}

static void done(void)
{
    ld_kind = LD_NONE;
    if (fmt_cursor > tx.total)
        fmt_cursor = tx.total;
    doc_modified = 0;
    MARK("layout>");
}

unsigned char fmt_begin(cw_rfn r)
{
    unsigned char h[8], i, n, l, have_sty = 0;
    pos_t len;
    rf = r;
    on = 0;
    oerr = 0;
    st = 0;
    ld_at = 0;
    ld_kind = LD_NONE;
    fmt_cursor = 0;
    fmt_lost = 0;
    fmt_cs = CS_LATIN1;
    if (rd(h, 8) || memcmp(h, CW_MAGIC, 4))
        return FE_NOTCW;
    if (h[4] > CW_MAJOR)
        return FE_NEWER;
    doc_new();
    for (;;) {
        if (rd(h, 8))
            return FE_BAD;              /* (no END) */
        len = get4(h + 4);
        if (!memcmp(h, "END ", 4)) {
            fl_flush();
            lay_all();
            done();
            return 0;
        }
        if (!memcmp(h, "INFO", 4) && len >= 8) {
            if (rd(h, 8) || skip(len - 8))
                return FE_IO;
            fmt_cursor = get4(h);
            fmt_cs = h[6] == CS_L10 ? CS_L10 : CS_LATIN1;
        } else if (!memcmp(h, "FONT", 4) && len >= 1) {
            if (rd(&n, 1))
                return FE_IO;
            --len;
            nfont = 0;
            for (i = 0; i < n; ++i) {
                if (rd(&l, 1) || l >= FONT_LEN || rd(iobuf, l))
                    return FE_BAD;
                len -= 1 + l;
                iobuf[l] = 0;
                if (nfont < MAXFONT) {
                    strcpy(fontname[nfont], (char *)iobuf);
                    ++nfont;
                }
            }
            if (!nfont)
                font_index(def_font);
            if (skip(len))
                return FE_IO;
        } else if (!memcmp(h, "STYL", 4) && len >= 1) {
            if (rd(&n, 1))
                return FE_IO;
            --len;
            if (!n || n > MAXSTY || len < 4 * (pos_t)n)
                return FE_BAD;
            if (rd(sty, 4 * n) || skip(len - 4 * n))
                return FE_IO;
            nsty = n;
            for (i = 0; i < n; ++i) {
                if (sty[i].font >= nfont)
                    sty[i].font = 0;
                if (!sty[i].size)
                    sty[i].size = DEF_SIZE;
            }
            have_sty = 1;
        } else if (!memcmp(h, "PARA", 4) && len >= 1) {
            /* (1.1) the paragraph styles: 8 bytes each; a longer entry
               of a later version: its first 8 */
            if (rd(&n, 1))
                return FE_IO;
            --len;
            if (!n || n > MAXPAR || len < sizeof(struct pstyle) * (pos_t)n)
                return FE_BAD;
            l = (unsigned char)(len / n);
            npar = 0;
            for (i = 0; i < n; ++i) {
                if (rd(&par[i], sizeof(struct pstyle)) || skip(l - sizeof(struct pstyle)))
                    return FE_IO;
                if (par[i].kind >= PK_N)
                    par[i].kind = PK_BODY;
                if (par[i].align > PA_JUSTIFY)
                    par[i].align = PA_LEFT;
                if (par[i].spacing < 2 || par[i].spacing > 4)
                    par[i].spacing = 2;
                ++npar;
            }
            if (skip(len - (pos_t)l * n))
                return FE_IO;
        } else if (!memcmp(h, "PAGE", 4) && len >= PGS_N) {
            /* (1.4) the page setup; a later version's longer: its first bytes */
            if (rd(&pgs, PGS_N) || skip(len - PGS_N))
                return FE_IO;
            pg_apply();
        } else if (!memcmp(h, "TEXT", 4)) {
            if (!have_sty)
                return FE_BAD;
            if (len > FMT_MAXTEXT)
                return FE_BIG;
            fl_flush();
            ld_kind = LD_CWRD;
            ld_left = ld_size = len;
            MARK("text<");
            return 0;
        } else if (skip(len))
            return FE_IO;
    }
}

unsigned char fmt_tbegin(cw_rfn r)
{
    rf = r;
    doc_new();
    on = 0;
    oerr = 0;
    ld_at = 0;
    cr = lead = more = nl = 0;
    fmt_cursor = 0;
    fmt_lost = 0;
    fmt_cs = CS_L10;
    fl_flush();
    ld_kind = LD_PLAIN;
    return 0;
}

#define LD_PIECE 16384u                  /* read, then wrapped: input looked at between */

unsigned char fmt_step(cw_rfn r, pos_t n)
{
    static pos_t p0, k;
    static unsigned char e, end;
    rf = r;
    ld_stop = 0;
    e = end = 0;
    while (n && !e && !end && !ld_stop) {
        k = n > LD_PIECE ? LD_PIECE : n;
        n -= k;
        p0 = tx.total;
        if (ld_kind == LD_CWRD) {
            if (k > ld_left)
                k = ld_left;
            e = load_text(k);
            if (!e && !ld_left)
                end = 1;
        } else if (ld_kind == LD_PLAIN) {
            end = load_plain(k);
            if (end && more)
                l1_seq();
            out_end();
            if (oerr)
                e = FE_MEM;
            else if (tx.total > FMT_MAXTEXT)
                e = FE_BIG;
        }
        if (!e && tx.total != p0) {
            /* the new bytes wrapped: an insertion at the end */
            lay_dirty(p0, tx.total - p0, 0);
            if (lay_fix() == 2)
                e = FE_LINES;
        }
    }
    if (e) {
        ld_kind = LD_NONE;
        return e;
    }
    if (end) {
        if (ld_kind == LD_CWRD && (e = finish()) != 0)
            return e;
        done();
    }
    return 0;
}

unsigned char fmt_load(cw_rfn r)
{
    unsigned char e = fmt_begin(r);
    while (!e && ld_kind)
        e = fmt_step(r, 0xFFFFFFFFUL);
    return e;
}

unsigned char fmt_text(cw_rfn r)
{
    unsigned char e = fmt_tbegin(r);
    while (!e && ld_kind)
        e = fmt_step(r, 0xFFFFFFFFUL);
    return e;
}
