/* Veranda Write: the document exported as a Word document (.docx) or an
   OpenDocument text (.odt) (DECISIONS W52, W53; office.h).

   A ZIP of stored entries (no compression: Commander Calc's
   src/export/zipw.c's way, MIT, Copyright (c) 2026 Marcin Spoczynski):
   each part made twice, first counted (its bytes and CRC-32: a local
   header comes before the bytes), then written; the central directory
   made at the end from a small table (a part's name, place, size, CRC).
   The XML's text is templates (oparts.c, another overlay on the X16:
   tools/oparts.py makes them) with values made here (val): the page's
   size and margins, the header and the footer with their fields, each
   paragraph with its style's alignment, indents, spacing, a heading's or
   a list's named style, each run with its style (font, size, bold,
   italic, underline), tabs, page breaks; the text read a buffer at a time
   (IBUF) as it is written, in UTF-8.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "office.h"
#include "oparts.h"
#ifndef CW_HOST
#include <veranda.h>
#include <vrwin.h>
extern const struct vrwin win;          /* (view.c's) */
#pragma code-name ("OVERLAY21")
#pragma rodata-name ("OVL21RO")
#pragma bss-name ("OVL21BSS")
#define oe_write e_oe_write
#endif
#define OE_ESC 2                        /* (err: stopped) */

#define IBUF  (iobuf)                   /* the text read */
#define IBN   192
#define OBUF  (iobuf + 256)             /* the part's bytes on their way (OE_N: oparts.c puts them there too) */
#define on oe_on

#ifdef CW_HOST
unsigned long oe_size;                  /* (the X16's: main.c, main RAM) */
#endif

static unsigned char kind, pass, err;
static unsigned long psize;
static char oname[40], odate[24];       /* {name}, {date} (iobuf's at the start: the caller's) */

/*---------------------------------------------------------------
  CRC-32 (the X16's: v/ocrc.s, its table made at the start)
 ---------------------------------------------------------------*/
#ifdef CW_HOST
static unsigned long crc, crc_t[256];

static void crc_init(void)
{
    unsigned long c;
    unsigned int n;
    unsigned char k;
    for (n = 0; n < 256; ++n) {
        c = n;
        for (k = 0; k < 8; ++k)
            c = c & 1 ? 0xEDB88320UL ^ (c >> 1) : c >> 1;
        crc_t[n] = c;
    }
}

static void FASTCALL crc_add(unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n; ++i)
        crc = crc_t[(crc ^ OBUF[i]) & 255] ^ (crc >> 8);
}
#else
void crc_init(void);
void FASTCALL crc_add(unsigned int n);  /* OBUF's first n bytes into oe_crc */
extern unsigned long oe_crc;
#define crc oe_crc
#endif

/*---------------------------------------------------------------
  the bytes: counted or written
 ---------------------------------------------------------------*/
static void flush(void)
{
    if (!on)
        return;
    if (pass)
        err |= plat_oe_out(OBUF, on);
    else
        crc_add(on);
    psize += on;
    on = 0;
}

static void FASTCALL oc(unsigned char c)
{
    OBUF[on++] = c;
    if (on == OE_N)
        flush();
}

/* a number (with dp decimals of its 10^dp-ths: 4, inches); below 65536
   without dividing (the powers of ten taken away: cc65's 32-bit division
   is some 700 cycles a digit) */
static const unsigned int p10[5] = { 10000, 1000, 100, 10, 1 };

static void FASTCALL onum(unsigned long v, unsigned char dp)
{
    static char t[12];
    static unsigned char i, d, lead;
    static unsigned int w;
    if (!dp && v < 0x10000UL) {
        w = (unsigned int)v;
        lead = 0;
        for (i = 0; i < 5; ++i) {
            d = '0';
            while (w >= p10[i]) {
                w -= p10[i];
                ++d;
            }
            if (d != '0' || lead || i == 4) {
                oc(d);
                lead = 1;
            }
        }
        return;
    }
    i = 0;
    do {
        t[i++] = (char)('0' + (unsigned char)(v % 10));
        v /= 10;
        if (i == dp)
            t[i++] = '.';
    } while (v || i <= dp);
    while (i)
        oc(t[--i]);
}

/* twips: as twips (.docx) or inches ("0.9840in", .odt) */
static void FASTCALL otw(int tw)
{
    if (tw < 0) {
        oc('-');
        tw = -tw;
    }
    if (kind == OK_DOCX) {
        onum((unsigned int)tw, 0);
        return;
    }
    onum(((unsigned long)tw * 125 + 9) / 18, 4);
    oc('i');
    oc('n');
}

/* a byte of the text (the set's) as XML: escaped, UTF-8 */
static void FASTCALL och(unsigned char c)
{
    static unsigned int u;
    static const char *e;
    if (c < 0x80) {
        e = c == '&' ? "&amp;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : c == '"' ? "&quot;" : 0;
        if (!e)
            oc(c);
        else
            while (*e)
                oc(*e++);
        return;
    }
    u = cs_uni(c);
    if (u < 0x800)
        oc(0xC0 | (u >> 6));
    else {
        oc(0xE0 | (u >> 12));
        oc(0x80 | ((u >> 6) & 0x3F));
    }
    oc(0x80 | (u & 0x3F));
}

static void FASTCALL ostr(const char *s)
{
    while (*s)
        och((unsigned char)*s++);
}

/*---------------------------------------------------------------
  the templates: a value's byte is val's
 ---------------------------------------------------------------*/
static void FASTCALL val(unsigned char v);

static void FASTCALL tpl(unsigned char t)
{
    static unsigned char v;             /* (a value may be another template: v is used up first) */
    op_open(t);
    while ((v = op_next()) != 0) {
        if (v == 0xFF)
            flush();
        else
            val(v == 31 ? 31 + op_x : v);
    }
}

/*---------------------------------------------------------------
  the X16's: the file, the progress (the status bar), Esc
 ---------------------------------------------------------------*/
#ifndef CW_HOST
extern unsigned char pdf_fh;            /* (main.c: the file pgpanel.c opened) */
extern char sl[64], sr[40];             /* (view.c: main RAM, the status bar's) */
unsigned char cw_esc(void);             /* (glue.s) Esc in the ROM's key queue: taken, 1 */

unsigned char FASTCALL plat_oe_out(const unsigned char *p, unsigned int n)
{
    return FWrite(pdf_fh, (void *)p, n) != 0;
}
#endif

static pos_t rp;
#ifndef CW_HOST
static unsigned char npoll;
#endif

/* (now and then while the text is read) "Exporting 45%..." */
static void poll(void)
{
#ifndef CW_HOST
    static unsigned char pc;
    static char *p;
    if (++npoll & 15)
        return;
    pc = (unsigned char)(pass * 50 + rp * 50 / (tx.total + 1));
    p = sl;
    strcpy(p, "Exporting ");
    p += 10;
    if (pc >= 10)
        *p++ = (char)('0' + pc / 10);
    *p++ = (char)('0' + pc % 10);
    strcpy(p, "%...");
    strcpy(sr, "Esc: stop");
    dispBufferOn = ST_WR_FORE | ST_WR_BACK;
    vr_win_status(&win, sl, sr);
    if (cw_esc())
        err |= OE_ESC;
#endif
}

/*---------------------------------------------------------------
  the text, read a buffer at a time
 ---------------------------------------------------------------*/
static unsigned int ri, rn;

static int rb(void)
{
    if (ri == rn) {
        if (rp >= tx.total)
            return -1;
        rn = cb_read(&tx, rp, IBUF, IBN);
        rp += rn;
        ri = 0;
        if (!rn)
            return -1;
    }
    return IBUF[ri++];
}

/*---------------------------------------------------------------
  the ZIP
 ---------------------------------------------------------------*/
#define NPART 10
static unsigned char np, tk;
static unsigned long p_crc[NPART], p_size[NPART], p_at[NPART];
static unsigned char p_k[NPART];
static unsigned long zat;               /* the file's bytes so far */

static void FASTCALL le(unsigned long v, unsigned char n)
{
    while (n--) {
        oc((unsigned char)v);
        v >>= 8;
    }
}

/* the parts: their templates' numbers (T_CT ... T_META); 7 and 8 the
   .docx's header and footer (T_HF), 9 the .odt's mimetype */
#define K_HDR  7
#define K_FTR  8
#define K_MIME 9
static const char *const pname[14] = {
    "[Content_Types].xml", "_rels/.rels", "word/_rels/document.xml.rels", "word/document.xml",
    "word/styles.xml", "word/settings.xml", "word/numbering.xml", "word/header1.xml", "word/footer1.xml",
    "mimetype", "META-INF/manifest.xml", "content.xml", "styles.xml", "meta.xml"
};

/* a local header or the central directory's record (cd) of part i */
static void FASTCALL zhead(unsigned char i, unsigned char cd)
{
    static const char *nm;
    static unsigned char n;
    nm = pname[p_k[i]];
    n = (unsigned char)strlen(nm);
    le(cd ? 0x02014B50UL : 0x04034B50UL, 4);
    if (cd)
        le(20, 2);                      /* version made by: 2.0 */
    le(20, 2);                          /* version needed: 2.0 */
    le(0, 4);                           /* flags; stored */
    le(0, 4);                           /* time, date */
    le(p_crc[i], 4);
    le(p_size[i], 4);
    le(p_size[i], 4);
    le(n, 2);
    le(0, 2);                           /* no extra field */
    if (cd) {
        le(0, 4);                       /* comment, disk */
        le(0, 2);                       /* attributes */
        le(0, 4);
        le(p_at[i], 4);
    }
    while (*nm)
        oc(*nm++);
}

/* part k: counted, its local header, written */
static void FASTCALL part(unsigned char k)
{
    static unsigned long n;
    if (err || np >= NPART)
        return;
    p_k[np] = k;
    tk = k == K_FTR ? T_HF : k;
    pass = 0;
    on = 0;
    psize = 0;
    crc = 0xFFFFFFFFUL;
    tpl(tk);
    flush();
    PM("p0");
    p_crc[np] = ~crc;
    p_size[np] = n = psize;
    p_at[np] = zat;
    pass = 1;
    zhead(np, 0);
    flush();
    psize = 0;
    tpl(tk);
    flush();
    PM("p1");
    if (psize != n)
        err = 1;                        /* (the two passes not the same: never) */
    zat += 30 + strlen(pname[k]) + n;
    ++np;
}

static void zend(void)
{
    static unsigned char i;
    pass = 1;
    psize = 0;
    for (i = 0; i < np; ++i)
        zhead(i, 1);
    flush();
    le(0x06054B50UL, 4);
    le(0, 4);                           /* the disks */
    le(np, 2);
    le(np, 2);
    le(psize, 4);                       /* (the directory's bytes) */
    le(zat, 4);
    le(0, 2);
    flush();                            /* (psize: the directory's and the end's bytes) */
}

/*---------------------------------------------------------------
  the header and the footer
 ---------------------------------------------------------------*/
static unsigned char hk, hn, fk;        /* 1 the header, 2 the footer; its parts; a field */

static unsigned int tw_w(void)          /* the text's width, twips */
{
    return pgs.w - pgs.left - pgs.right;
}

static const char *hs(void)
{
    return hk == 1 ? pgs.head : pgs.foot;
}

static void hcount(void)
{
    static const char *s;
    for (s = hs(), hn = 1; *s; ++s)
        if (*s == '|' && hn < 3)
            ++hn;
}

/* a field's kind at s ("{page}" 1, "{pages}" 2, "{date}" 3, "{name}" 4) */
static unsigned char FASTCALL fieldk(const char *s)
{
    if (!memcmp(s, "{page}", 6))
        return 1;
    if (!memcmp(s, "{pages}", 7))
        return 2;
    if (!memcmp(s, "{date}", 6))
        return 3;
    if (!memcmp(s, "{name}", 6))
        return 4;
    return 0;
}

static const unsigned char flen[5] = { 0, 6, 7, 6, 6 };

/* the header's or footer's text: its parts apart by tabs, its fields */
static void hftext(void)
{
    static const char *s;
    static unsigned char k, open;
    s = hs();
    k = 1;
    open = 0;
    while (*s) {
        fk = *s == '{' ? fieldk(s) : 0;
        if ((*s == '|' && k < hn) || fk) {
            if (open)
                tpl(T_DRE);
            open = 0;
            if (!fk) {
                ++k;
                tpl(kind == OK_DOCX ? T_DHTAB : T_OTAB);
                ++s;
                continue;
            }
            tpl(kind == OK_DOCX ? T_DFIELD : T_OF1 - 1 + fk);
            s += flen[fk];
            continue;
        }
        if (kind == OK_DOCX && !open) {
            tpl(T_DHRUN);
            open = 1;
        }
        och((unsigned char)*s++);
    }
    if (open)
        tpl(T_DRE);
}

/*---------------------------------------------------------------
  the paragraphs
 ---------------------------------------------------------------*/
static unsigned char cs;                /* the character style in effect */
static unsigned char nnum;              /* (.docx) numbered lists: each run of them its own */
static unsigned char pbn;               /* (.odt) a page break waiting: the next paragraph's */
static unsigned char lk;                /* (.odt) the list open: its kind, 0 none */
static unsigned char ps0;               /* (.odt) the first paragraph's style (its master page) */
static unsigned char firstp;            /* (.odt) the first paragraph not written yet */
static unsigned char ps, pv;            /* the paragraph style written; (.odt) its variant: 1 b, 2 F */
static const struct pstyle *pp;
static unsigned long vn;                /* {n} */

static unsigned char FASTCALL ismono(unsigned char s)
{
    return strstr(fontname[sty[s].font], "Mono") != 0;
}

static void FASTCALL vtpl(unsigned char t, unsigned long n)
{
    vn = n;
    tpl(t);
}

/* the document's paragraphs (document.xml's body, content.xml's text) */
static void body(void)
{
    static int c;
    static unsigned char pb, run, t, k, pk, lastk;
    rp = 0;
    ri = rn = 0;
    cs = 0;
    nnum = 0;
    pbn = lk = lastk = 0;
    firstp = 1;
    c = rb();
    for (;;) {
        poll();
        if (err)
            break;
        /* a paragraph: its codes first */
        ps = pb = 0;
        while (c == K_PS || c == K_PB) {
            t = (unsigned char)c;
            k = (unsigned char)rb();
            rb();
            if (t == K_PS)
                ps = k;
            else
                pb = 1;
            c = rb();
        }
        if (ps >= npar)
            ps = 0;
        pp = &par[ps];
        pk = pp->kind < PK_N ? pp->kind : PK_BODY;
        if (pk == PK_NUMBER && lastk != PK_NUMBER)
            ++nnum;                     /* (a numbered list starts again from 1) */
        lastk = pb ? 0 : pk;
        if (kind == OK_DOCX)
            tpl(pb ? T_DPB : T_DP);
        else if (pb) {
            if (pbn)
                tpl(T_OPB);             /* (two in a row: a page between) */
            pbn = 1;
        } else {
            if (lk && pk != lk) {
                tpl(T_OLE);
                lk = 0;
            }
            if (IS_LIST(pk)) {
                if (!lk)
                    tpl(pk == PK_BULLET ? T_OLB : T_OLN);
                lk = pk;
                tpl(T_OLI);
            }
            pv = firstp ? 2 : pbn;
            firstp = pbn = 0;
            if (IS_HEAD(pk))
                vtpl(T_OH, pk - PK_H1 + 1);
            else
                tpl(T_OP);
        }
        run = 0xFF;
        t = 1;                          /* (.odt: a space now is the paragraph's first) */
        /* its characters, runs of one style */
        while (c >= 0 && c != CH_CR) {
            if (IS_KIND(c)) {
                k = (unsigned char)rb();
                if (c == K_CS && k < nsty)
                    cs = k;
                rb();
                c = rb();
                continue;
            }
            if (!pb) {
                if (run != cs) {
                    if (run != 0xFF)
                        tpl(kind == OK_DOCX ? T_DRE : T_OSPE);
                    run = cs;
                    if (kind == OK_DOCX)
                        vtpl(T_DR, sty[cs].size * 2);
                    else
                        vtpl(T_OSPAN, cs);
                }
                if (c == CH_TAB) {
                    tpl(kind == OK_DOCX ? T_DTAB : T_OTAB);
                    t = 0;
                } else if (c == ' ' && kind == OK_ODT && t)
                    tpl(T_OS);          /* (a space first in the paragraph or after a space) */
                else {
                    och((unsigned char)c);
                    t = c == ' ';
                }
            }
            c = rb();
        }
        if (run != 0xFF)
            tpl(kind == OK_DOCX ? T_DRE : T_OSPE);
        if (kind == OK_DOCX)
            tpl(T_DPE);
        else if (!pb) {
            tpl(IS_HEAD(pk) ? T_OHE : T_OPE);
            if (lk)
                tpl(T_OLIE);
        }
        if (c < 0)
            break;
        c = rb();                       /* (past its CR) */
        if (c < 0) {
            /* (the CR was the last byte: an empty paragraph after it) */
            if (lk)
                tpl(T_OLE);
            lk = 0;
            tpl(kind == OK_DOCX ? T_DPEMPTY : pbn ? T_OPB : T_OPEMPTY);
            pbn = 0;
            break;
        }
    }
    if (lk)
        tpl(T_OLE);
    if (pbn)
        tpl(T_OPB);
}

/* (.odt) paragraph style i's automatic styles: P<i>, P<i>b (a page
   break before), PF (the first paragraph's: its master page) */
static void FASTCALL opstyle(unsigned char i, unsigned char v)
{
    ps = i;
    pv = v;
    pp = &par[i];
    tpl(T_OPS);
}

/* the header's and footer's room in the top and bottom margins */
#define HROOM(m) ((m) / 2)
#define NOFIRST() ((pgs.flags & PS_NOFIRST) && (pgs.head[0] || pgs.foot[0]))

static unsigned char li;                /* (val's loops: val is called again inside them) */

static void FASTCALL val(unsigned char v)
{
    static unsigned char i, f;
    f = sty[cs].flags;
    switch (v) {
    case V_TITLE:
        ostr(oname);
        break;
    case V_BODY:
        body();
        break;
    case V_HREF:
        if (pgs.head[0])
            tpl(T_HREF);
        break;
    case V_FREF:
        if (pgs.foot[0])
            tpl(T_FREF);
        break;
    case V_CTH:
        if (pgs.head[0])
            tpl(T_CTH);
        break;
    case V_CTF:
        if (pgs.foot[0])
            tpl(T_CTF);
        break;
    case V_RLH:
        if (pgs.head[0])
            tpl(T_RLH);
        break;
    case V_RLF:
        if (pgs.foot[0])
            tpl(T_RLF);
        break;
    case V_PW:
        otw(pgs.w);
        break;
    case V_PH:
        otw(pgs.h);
        break;
    case V_MT: case V_OMT1:
        otw(pgs.top);
        break;
    case V_MR:
        otw(pgs.right);
        break;
    case V_MB: case V_OMB1:
        otw(pgs.bottom);
        break;
    case V_ML:
        otw(pgs.left);
        break;
    case V_MH:
        otw(HROOM(pgs.top));
        break;
    case V_MF:
        otw(HROOM(pgs.bottom));
        break;
    case V_TITLEPG:
        if (pgs.flags & PS_NOFIRST)
            tpl(T_TITLEPG);
        break;
    case V_NUMS:
        for (li = 1; li <= nnum; ++li)
            vtpl(T_DNUM, li + 1);
        break;
    case V_HFT:
        tpl(hk == 1 ? T_HDR : T_FTR);
        break;
    case V_HFS:
        tpl(hk == 1 ? T_HEADER : T_FOOTER);
        break;
    case V_DTABC:
        if (hn == 3)
            vtpl(T_DTABC, tw_w() / 2);
        break;
    case V_DTABR: case V_TABR:
        otw(tw_w());
        break;
    case V_TABC:
        otw(tw_w() / 2);
        break;
    case V_HJC:
        if (hn == 1)
            vtpl(T_DJC, PA_CENTRE);
        break;
    case V_HTEXT:
        hftext();
        break;
    case V_N:
        onum(vn, 0);
        break;
    case V_DPS:
        i = pp->kind;
        if (IS_HEAD(i))
            vtpl(T_DHEAD, i - PK_H1 + 1);
        else if (i == PK_QUOTE)
            tpl(T_DQUOTE);
        else if (i == PK_BULLET)
            tpl(T_DBUL);
        else if (i == PK_NUMBER)
            vtpl(T_DNUMB, nnum + 1);
        break;
    case V_BEF:
        otw(pp->before * TW_PX);
        break;
    case V_AFT:
        otw(pp->after * TW_PX);
        break;
    case V_LINE:
        onum(pp->spacing * 120, 0);
        break;
    case V_IL:
        otw(pp->left * TW_PX);
        break;
    case V_IR:
        otw(pp->right * TW_PX);
        break;
    case V_IF:
        otw(pp->first * TW_PX);
        break;
    case V_DFIRST:
        if (pp->first < 0)
            vtpl(T_DHANG, -pp->first * TW_PX);
        else
            vtpl(T_DFIRSTL, pp->first * TW_PX);
        break;
    case V_DJC:
        if (pp->align && pp->align < 4)
            vtpl(T_DJC, pp->align);
        break;
    case V_JC:
        tpl(T_CENTER - 1 + (unsigned char)vn);
        break;
    case V_FONT:
        tpl(ismono(cs) ? T_COURIER : T_ARIAL);
        break;
    case V_RB:
        if (f & SF_BOLD)
            tpl(T_DB);
        break;
    case V_RI:
        if (f & SF_ITALIC)
            tpl(T_DI);
        break;
    case V_RU:
        if (f & SF_ULINE)
            tpl(T_DU);
        break;
    case V_FINSTR:
        tpl(T_FI1 - 1 + fk);
        break;
    case V_FRES:
        if (fk < 3)
            oc('1');
        else
            ostr(fk == 3 ? odate : oname);
        break;
    case V_OSTYLES:
        for (li = 0; li < npar; ++li) {
            opstyle(li, 0);
            opstyle(li, 1);
        }
        opstyle(ps0, 2);
        for (cs = 0; cs < nsty; ++cs)
            vtpl(T_OTS, cs);
        cs = 0;
        break;
    case V_OPAGE:
        tpl(T_OPAGE);
        break;
    case V_OMT:
        otw(pgs.head[0] ? pgs.top - HROOM(pgs.top) : pgs.top);
        break;
    case V_OMB:
        otw(pgs.foot[0] ? pgs.bottom - HROOM(pgs.bottom) : pgs.bottom);
        break;
    case V_OHR:
        otw(pgs.head[0] ? HROOM(pgs.top) : 0);
        break;
    case V_OFR:
        otw(pgs.foot[0] ? HROOM(pgs.bottom) : 0);
        break;
    case V_OHF:
        if (pgs.head[0]) {
            hk = 1;
            tpl(T_OHDR);
        }
        if (pgs.foot[0]) {
            hk = 2;
            tpl(T_OFTR);
        }
        break;
    case V_OHP:
        hcount();
        tpl(hn == 1 ? T_OHP1 : T_OHP);
        break;
    case V_H2:
        if (hn == 2)
            oc('2');
        break;
    case V_OFIRST:
        if (NOFIRST())
            tpl(T_OFIRST);
        break;
    case V_PNAME: case V_PREF:
        if (pv == 2)
            oc('F');
        else {
            onum(ps, 0);
            if (pv)
                oc('b');
        }
        break;
    case V_OPAR:
        i = pp->kind;
        tpl(IS_HEAD(i) ? T_HEAD1 - PK_H1 + i : i == PK_QUOTE ? T_QUOTATIONS : T_STANDARD);
        break;
    case V_OMP:
        if (pv == 2 && NOFIRST())
            tpl(T_OMP);
        break;
    case V_LH:
        onum(pp->spacing * 50, 0);
        break;
    case V_AL:
        i = pp->align & 3;
        tpl(i == PA_LEFT ? T_START : i == PA_CENTRE ? T_CENTER : i == PA_RIGHT ? T_END : T_JUSTIFY);
        break;
    case V_OBRK:
        if (pv == 1)
            tpl(T_OBRK);
        break;
    case V_SZ:
        onum(sty[cs].size, 0);
        break;
    case V_WT:
        tpl(f & SF_BOLD ? T_BOLD : T_NORMAL);
        break;
    case V_ST:
        tpl(f & SF_ITALIC ? T_ITALIC : T_NORMAL);
        break;
    case V_UL:
        tpl(f & SF_ULINE ? T_SOLID : T_NONE);
        break;
    }
}

/* the document as a .docx (k OK_DOCX) or an .odt; iobuf holds {date}
   (at 0) and {name} (at 32) */
unsigned char FASTCALL oe_write(unsigned char k)
{
    kind = k;
    err = 0;
    np = 0;
    zat = 0;
    strncpy(odate, (char *)iobuf, sizeof odate - 1);
    strncpy(oname, (char *)iobuf + 32, sizeof oname - 1);
    crc_init();
    ps0 = 0;
    if (tx.total >= 3 && cb_byte(&tx, 0) == K_PS)
        ps0 = cb_byte(&tx, 1);
    if (ps0 >= npar)
        ps0 = 0;
    if (k == OK_DOCX) {
        part(T_CT);
        part(T_RELS);
        part(T_DREL);
        part(T_DOC);                    /* (first: the numbered lists counted for numbering.xml) */
        part(T_STY);
        part(T_SET);
        part(T_NUM);
        hk = 1;
        hcount();
        if (pgs.head[0])
            part(K_HDR);
        hk = 2;
        hcount();
        if (pgs.foot[0])
            part(K_FTR);
    } else {
        part(K_MIME);
        part(T_MAN);
        part(T_OCON);
        part(T_OSTY);
        part(T_META);
    }
    if (!err)
        zend();
    oe_size = zat + psize;
    return err & OE_ESC ? OE_STOP : err ? OE_IO : 0;
}
