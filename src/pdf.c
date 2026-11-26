/* Veranda Write: the document as a PDF 1.4 file (DECISIONS W47).

   Written as it goes, a buffer (iobuf, 512 bytes) at a time, through the
   platform's plat_pdf_out: nothing but the cross-reference table's
   offsets is kept, 4 bytes an object in RAM banks of the pool (2,048 a
   bank). The objects: 1 the catalog, 2 the page tree (written last, when
   the pages are known), 3 the encoding, 4-11 the fonts, 12 the
   resources, 13 the information, then three a page: the page, its
   content stream, the stream's length (written after the stream, so it
   need not be known first: the stream goes out as it is made).

   The fonts are PDF's standard 14 (nothing embedded): Helvetica for every
   font but Gate Mono (Courier), each in its bold, oblique and bold
   oblique forms, with an encoding of WinAnsiEncoding and the 40 bytes
   where ISO-8859-16, the Veranda set, differs (/Differences: Ą ą Ł ł Ś
   ś Ź ź Ż ż Ć ć Ń ń Ę ę ...), so the text is selectable and searchable as
   it is, Polish too; the widths for the layout are Helvetica's (pdfw.h,
   tools/afmtab.py).

   The layout is the X16's: the same lines (the line table's, wrapped by
   the screen's fonts), on the same pages (v/pages.c's rule: a line that
   does not fit starts the next page, as does the line after a page
   break), each line where the screen has it (its height, its baseline;
   80 pixels an inch, a pixel 0.9 point), its text in the PDF's font set
   from its paragraph's left edge, centred, right aligned or justified
   (its spaces stretched: Tw) by the PDF font's widths; a line wider than
   its paragraph in the PDF's font is narrowed (the text matrix's
   horizontal scale) rather than run past the margin. Bold, italic and
   underline (a filled rectangle under the text, Helvetica's position and
   thickness); a list's bullet or number; the header and footer in
   Helvetica 9 in the margins.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#ifdef CW_HOST
#include <stdio.h>
#endif
#include "cw.h"
#ifndef CW_HOST
#include "v/cwv.h"                      /* (t_arg, r11, r12: overlay 14's header and footer) */
#endif
#ifndef CW_HOST
#pragma code-name ("OVERLAY15")
#pragma rodata-name ("OVL15RO")
#pragma bss-name ("OVL15BSS")
#endif
#include "pdfw.h"
#ifndef CW_HOST
#define pdf_write e_pdf_write           /* (called through glue.s) */
#endif

#ifdef CW_HOST
unsigned int pdf_pages, pdf_page;       /* (the X16's: main.c, main RAM: pgpanel.c, overlay 16, */
unsigned long pdf_size;                 /* reads and sets them) */
const char *pdf_title;
#endif
#ifdef CW_HOST
unsigned int pdf_first[8192];           /* (the tests: each page's first line) */
static unsigned char tb[128];           /* (the X16's: view.c's lb) */
#define TB tb
#else
extern unsigned char lb[128];           /* (main RAM: cb_read's) */
#define TB lb
#define pdf_hf f_pdf_hf                 /* (pdfhf.c, overlay 14: through glue.s) */
unsigned int FASTCALL f_pdf_hf(unsigned char k);
void bk_wr(unsigned char b, unsigned int a, const unsigned char *src, unsigned int n);
void bk_rd(unsigned char b, unsigned int a, unsigned char *dst, unsigned int n);
static unsigned char xo[4] = { 0 };     /* (DATA, main RAM: an offset to and from its bank;
                                           a BSS pragma does not place a static, W48) */
#endif

unsigned int pdf_obn;                   /* iobuf's bytes (v/pdfnum.s's pdf_esc too) */
#define ob_n pdf_obn
static unsigned char perr;

void pdf_flush(void)
{
    if (ob_n && !perr && plat_pdf_out(ob_n))
        perr = PE_IO;
    pdf_size += ob_n;
    ob_n = 0;
}
#define flush pdf_flush

/* a byte, a string to the PDF (v/pdfnum.s on the X16: cc65's call and
   16-bit index took some 100 cycles a byte) */
#ifdef CW_HOST
static void FASTCALL pc(unsigned char c)
{
    iobuf[ob_n++] = c;
    if (ob_n == IOBUF)
        flush();
}

static void FASTCALL ps(const char *s)
{
    while (*s)
        pc(*s++);
}
#else
#define pc pdf_pc
#define ps pdf_ps
void FASTCALL pdf_pc(unsigned char c);
void FASTCALL pdf_ps(const char *s);
#endif

/* v in decimal */
/* the number pv in decimal into pd, its digits (v/pdfnum.s on the X16:
   cc65's division by 10 was most of the time a number took) */
#ifdef CW_HOST
static unsigned long pv;
static char pd[11];

static unsigned char pdf_dec(void)
{
    return (unsigned char)sprintf(pd, "%lu", pv);
}
#else
extern unsigned long pv;
extern char pd[11];
unsigned char pdf_dec(void);
#endif

static void FASTCALL digits(unsigned long v)
{
    pv = v;
    pdf_dec();
    ps(pd);
}

/* v in decimal, a space after */
static void FASTCALL pn(unsigned long v)
{
    digits(v);
    pc(' ');
}

/* v thousandths of a point as points (three decimals at most), a space
   after */
static void FASTCALL pf(long v)
{
    static unsigned char n, k, e;
    static char f[3];
    if (v < 0) {
        pc('-');
        v = -v;
    }
    pv = v;
    n = pdf_dec();
    if (n > 3)
        for (k = 0; k < n - 3; ++k)
            pc(pd[k]);
    else
        pc('0');
    f[0] = n >= 3 ? pd[n - 3] : '0';
    f[1] = n >= 2 ? pd[n - 2] : '0';
    f[2] = pd[n - 1];
    for (e = 3; e && f[e - 1] == '0'; --e)
        ;
    if (e) {
        pc('.');
        for (k = 0; k < e; ++k)
            pc(f[k]);
    }
    pc(' ');
}

/*---------------------------------------------------------------
  the objects: where each starts (the cross-reference table)
 ---------------------------------------------------------------*/
#define XB 8                            /* banks for offsets: 16,384 objects (5,400 pages) */
static unsigned char xb[XB];
static unsigned int nobj;               /* objects 1 .. nobj - 1 */

/* object n's offset: in bank xb[n / 2048] (taken from the pool when
   first needed), at xa; 1 no bank */
static unsigned char xk;
static unsigned int xa;

static unsigned char FASTCALL where(unsigned int n)
{
    xk = (unsigned char)(n >> 11);
    xa = 0xA000 + (n & 2047) * 4;
    if (xk >= XB)
        return 1;
    if (!xb[xk]) {
        bk_pg = 0;
        xb[xk] = bk_get();
    }
    xk = xb[xk];
    return !xk;
}

static void FASTCALL off(unsigned int n, unsigned long v)
{
    if (where(n)) {
        perr = PE_MEM;
        return;
    }
#ifdef CW_HOST
    memcpy(host_bank[xk] + xa - 0xA000, &v, 4);
#else
    memcpy(xo, &v, 4);
    bk_wr(xk, xa, xo, 4);
#endif
}

static unsigned long FASTCALL off_of(unsigned int n)
{
    static unsigned long v;
    where(n);
#ifdef CW_HOST
    memcpy(&v, host_bank[xk] + xa - 0xA000, 4);
#else
    bk_rd(xk, xa, xo, 4);
    memcpy(&v, xo, 4);
#endif
    return v;
}

static void FASTCALL obj(unsigned int n)
{
    off(n, pdf_size + ob_n);
    if (n >= nobj)
        nobj = n + 1;
    pn(n);
    ps("0 obj\n");
}

/* the encoding object's text (pdfenc.h) in iobuf, its part k: its bytes */
#ifdef CW_HOST
static unsigned int FASTCALL pdf_enc(unsigned char k)
{
    static const char a[] = PDF_ENC0, b[] = PDF_DIFF PDF_ENC1;
    memcpy(iobuf, k ? b : a, k ? sizeof b - 1 : sizeof a - 1);
    return k ? sizeof b - 1 : sizeof a - 1;
}
#else
unsigned int FASTCALL f_pdf_enc(unsigned char k);   /* (v/tbcmd.c, overlay 29: through glue.s) */
#define pdf_enc f_pdf_enc
#endif

static void endobj(void)
{
    ps("endobj\n");
}

/* "n 0 R " */
static void FASTCALL ref(unsigned int n)
{
    pn(n);
    ps("0 R ");
}

/*---------------------------------------------------------------
  the fonts: F0-F7 = (Courier: 4) + (bold: 1) + (oblique: 2)
 ---------------------------------------------------------------*/
static const char *const fsuf[4] = { "", "-Bold", "-Oblique", "-BoldOblique" };
static unsigned char mono[MAXFONT];     /* the document's font f: Courier */

static void fonts(void)
{
    static unsigned char k;
    static const char *n;
    for (k = 0; k < nfont; ++k) {
        mono[k] = 0;
        for (n = fontname[k]; *n; ++n)
            if (!strncmp(n, "Mono", 4))    /* (Gate Mono, any "... Mono") */
                mono[k] = 1;
    }
    obj(3);
    flush();
    ob_n = pdf_enc(0);                  /* (1.6: overlay 29's text on the X16) */
    flush();
    ob_n = pdf_enc(1);
    flush();
    endobj();
    for (k = 0; k < 8; ++k) {
        obj(4 + k);
        ps("<< /Type /Font /Subtype /Type1 /BaseFont /");
        ps(k & 4 ? "Courier" : "Helvetica");
        ps(fsuf[k & 3]);
        ps(" /Encoding 3 0 R >>\n");
        endobj();
    }
    obj(12);
    ps("<< /Font <<");
    for (k = 0; k < 8; ++k) {
        ps(" /F");
        pc((char)('0' + k));
        pc(' ');
        ref(4 + k);
    }
    ps(">> /ProcSet [/PDF /Text] >>\n");
    endobj();
}

/*---------------------------------------------------------------
  a line. Its text is read twice: measured (its width in the PDF's
  fonts, for the alignment), then written. A run is the characters of
  one style between tabs (at most rb's): its width is kept in the font
  table's units (wt: Helvetica's or Helvetica-Bold's widths / 4, Courier
  150) and made thousandths of a point once at its end (* wk: 4 x the
  size), not a 32-bit sum a character. Each run's place: the line's first
  with Tm (its x, the baseline's y, the narrowing's scale), the next ones
  Td from the one before
 ---------------------------------------------------------------*/
static long X0, Y0;                     /* the text's left edge, the page's top (thousandths of a point) */
#define PX 900                          /* a pixel: 0.9 point */
#define TABX (40L * PX)                 /* a tab stop every half inch */
static unsigned char cf, csz;           /* the font and size set (0xFF: none yet on this page) */
static struct line l;
static pos_t lp;                        /* the line's place */
static long lx, xs, wvis, jx, fsc, ybase, lx0;
static unsigned char st, sf, ru, tm;    /* the style, its PDF font (F0-F7: Courier 4 + bold 1 +
                                           oblique 2); the run underlined; Tm next */
static long rx;                         /* the run's x (unscaled, from lx0) */
static unsigned char wk;                /* 4 x the style's size */
/* (pdf_acc's, v/pdfnum.s on the X16: the characters of a run) */
unsigned char pa_mode;                  /* 0 measuring, 1 writing */
unsigned char pa_j, pa_k;               /* TB's next byte, its end */
const unsigned char *pa_wt;             /* the style's widths (0: Courier's) */
unsigned char pa_rb[64], pa_rn;         /* a run's bytes */
unsigned int pa_rw, pa_rv;              /* the run's widths so far, at its last visible character */
unsigned char pa_rs, pa_rvs, pa_rvis;   /* its spaces (justified: + jx each), before that; one there */
unsigned int pa_pend, pa_nsp;           /* spaces since a visible character; the line's between words */
#define mode pa_mode
#define wt   pa_wt
#define rb   pa_rb
#define rn   pa_rn
#define rw   pa_rw
#define rv   pa_rv
#define rs   pa_rs
#define rvs  pa_rvs
#define rvis pa_rvis
#define pend pa_pend
#define nsp  pa_nsp
/* a run's width: pm_r = pm_a x pm_b (v/pdfnum.s on the X16) */
#ifdef CW_HOST
static unsigned int pm_a;
static unsigned char pm_b;
static unsigned long pm_r;

static void pdf_mul(void)
{
    pm_r = (unsigned long)pm_a * pm_b;
}

static void pdf_acc(void)
{
    unsigned char c;
    while (pa_j < pa_k && pa_rw < 0xE000) {
        c = TB[pa_j];
        if (c < ' ' || (pa_mode && pa_rn == sizeof pa_rb))
            break;
        if (pa_mode)
            pa_rb[pa_rn++] = c;
        pa_rw += pa_wt ? pa_wt[CSLOT(c)] : 150;
        ++pa_j;
        if (c == ' ') {
            ++pa_rs;
            ++pa_pend;
        } else {
            pa_nsp += pa_pend;
            pa_pend = 0;
            pa_rv = pa_rw;
            pa_rvs = pa_rs;
            pa_rvis = 1;
        }
    }
}
#else
extern unsigned int pm_a;
extern unsigned char pm_b;
extern unsigned long pm_r;
void pdf_mul(void);
void pdf_acc(void);
#endif

static long FASTCALL scaled(long x)
{
    return fsc == 1000 ? x : x * fsc / 1000;
}

static void FASTCALL setst(unsigned char s)
{
    static const struct cstyle *c;
    c = &sty[s];
    st = s;
    wk = c->size << 2;
    sf = c->flags & (SF_BOLD | SF_ITALIC);
    wt = sf & SF_BOLD ? pw_helvb : pw_helv;
    if (mono[c->font]) {
        sf |= 4;
        wt = 0;
    }
}

/* sizes (< 100), a space after: no 32-bit number */
static void FASTCALL pn8(unsigned char v)
{
    if (v >= 10)
        pc((char)('0' + v / 10));
    pc((char)('0' + v % 10));
    pc(' ');
}

/* pa_rb[pa_e .. pa_rn) into iobuf, '(' ')' '\\' escaped, while two bytes
   fit (v/pdfnum.s on the X16: a call and a test a byte were half the
   time a run took) */
unsigned char pa_e;
#ifdef CW_HOST
static void pdf_esc(void)
{
    unsigned char c;
    while (pa_e < pa_rn && ob_n < IOBUF - 1) {
        c = pa_rb[pa_e++];
        if (c == '(' || c == ')' || c == '\\')
            iobuf[ob_n++] = '\\';
        iobuf[ob_n++] = c;
    }
}
#else
void pdf_esc(void);
#endif

/* the run (its width w): written: its font, its text (after the run
   before, where the font's widths take it; placed with Tm at the line's
   start, after a tab and after an underline); underlined, a rectangle
   under it */
static void FASTCALL run_out(long w)
{
    if (sf != cf || sty[st].size != csz) {
        cf = sf;
        csz = sty[st].size;
        ps("/F");
        pc((char)('0' + sf));
        pc(' ');
        pn8(csz);
        ps("Tf ");
    }
    if (tm) {
        tm = 0;
        if (fsc != 1000) {
            pf(fsc);
            ps("0 0 1 ");
        } else
            ps("1 0 0 1 ");
        pf(lx0 + scaled(rx));
        pf(ybase);
        ps("Tm ");
    }
    pc('(');
    pa_e = 0;
    for (;;) {
        pdf_esc();
        if (pa_e >= rn)
            break;
        flush();
    }
    ps(") Tj\n");
    if (ru) {
        ps("ET ");
        pf(lx0 + scaled(rx));
        pf(ybase - (long)csz * 125);
        pf(scaled(w));
        pf((long)csz * 50);
        ps("re f BT\n");
        tm = 1;
    }
}

/* the run's end: its width into lx (and wvis), written (mode 1) */
static void end_run(void)
{
    static long w;
    if (!rn && !rw)
        return;
    pm_a = rw;
    pm_b = wk;
    pdf_mul();
    w = pm_r;
    if (rs && jx)
        w += rs * jx;
    if (rvis) {
        pm_a = rv;
        pm_b = wk;
        pdf_mul();
        wvis = rx + pm_r;
        if (rvs && jx)
            wvis += rvs * jx;
    }
    if (mode && rn)
        run_out(w);
    lx = rx + w;
    rx = lx;
    rw = rv = 0;
    rs = rvs = rvis = rn = 0;
}

/* the line's bytes, from its start style */
static void scan(void)
{
    static pos_t p, e;
    static unsigned char c;
    p = lp;
    e = lp + l.len;
    setst(l.s);
    rx = lx = 0;
    rw = rv = 0;
    rs = rvs = rvis = rn = 0;
    while (p < e) {
        pa_k = (unsigned char)cb_read(&tx, p, TB, e - p > sizeof TB ? sizeof TB : (unsigned int)(e - p));
        for (pa_j = 0; pa_j < pa_k;) {
            if (mode && !rn)
                ru = (sty[st].flags & SF_ULINE) != 0;
            pdf_acc();                  /* (the characters, as many as it takes) */
            if (pa_j >= pa_k)
                break;
            c = TB[pa_j];
            if (IS_KIND(c)) {
                if (pa_j + 3 > pa_k)
                    break;
                if (c == K_CS) {
                    end_run();
                    setst(TB[pa_j + 1]);
                }
                pa_j += 3;
            } else if (c == CH_TAB) {
                end_run();
                lx = xs + ((lx - xs) / TABX + 1) * TABX;
                wvis = rx = lx;
                tm = 1;                 /* (the next run placed) */
                ++pa_j;
            } else if (c == CH_CR)
                ++pa_j;
            else
                end_run();              /* (rb full, or a long run) */
        }
        if (!pa_j)
            break;
        p += pa_j;
    }
    end_run();
}

static unsigned char lnum;              /* (1.6) a cell's line's list number (0: par_number's) */

static void FASTCALL line(unsigned int i, unsigned long top)
{
    static const struct pstyle *q;
    static unsigned char first;
    static int base, avail;
    static long A, W, x0;
    q = &par[l.p];
    first = (l.f & LF_FIRST) && !IS_LIST(q->kind);
    base = q->left + (first ? q->first : 0);
    avail = (int)wrap_w - q->left - q->right - (first ? q->first : 0);
    if (avail < 40)
        avail = 40;
    A = (long)avail * PX;
    ybase = Y0 - (long)(top + l.a) * PX;
    /* measured */
    mode = 0;
    wvis = 0;
    nsp = pend = 0;
    jx = 0;
    xs = 0;
    scan();
    W = wvis;
    fsc = 1000;
    x0 = (long)base * PX;
    if (W > A)
        fsc = A * 1000 / W;             /* (narrowed to fit) */
    else if (q->align == PA_CENTRE)
        x0 += (A - W) / 2;
    else if (q->align == PA_RIGHT)
        x0 += A - W;
    else if (q->align == PA_JUSTIFY && !(l.f & LF_PARA) && nsp)
        jx = (A - W) / nsp;
    ps("BT\n");
    if (jx) {
        pf(jx);
        ps("Tw\n");
    }
    /* a list's bullet or number, in the line's first style, at its own place */
    if ((l.f & LF_FIRST) && IS_LIST(q->kind)) {
        setst(l.s);
        rn = 0;
        if (q->kind == PK_BULLET)
            rb[rn++] = 0x95;            /* (WinAnsi's bullet) */
        else {
            pv = lnum ? lnum : par_number(i);
            rn = pdf_dec();
            memcpy(rb, pd, rn);
            rb[rn++] = '.';
        }
        ru = 0;
        tm = 1;
        rx = 0;
        lx0 = X0 + (long)(q->left + q->first) * PX;
        W = fsc;
        fsc = 1000;
        run_out(0);
        fsc = W;
    }
    /* written */
    mode = 1;
    lx0 = X0 + x0;
    tm = 1;
    nsp = pend = 0;
    scan();
    ps("ET\n");
    if (jx)
        ps("0 Tw\n");
}

/*---------------------------------------------------------------
  (1.6, W63) a table's row: its borders (lines of half a point, as the
  screen draws them), its cells' lines (table.c's sub-lines), each line
  at its cell's place and in its width, as the screen has them
 ---------------------------------------------------------------*/
#ifdef CW_HOST
extern unsigned char tb_xb[64];
#define TXB tb_xb
#define TB_PDF() tb_pdf()
#else
#define TXB lb
void __fastcall__ callb(unsigned char b, void (*f)(void));
extern unsigned char bank27;
#define TB_PDF() callb(bank27, tb_pdf)
#endif
#define U ((struct sub *)TXB)
static unsigned int xx[TB_MAXC + 1];
static unsigned int rtop, ri;           /* (row's: its line's top on the page, its number) */

static long FASTCALL px(unsigned int v)
{
    return (long)v * PX;
}

/* the row's cells' boxes (the rows' boxes meet: one line between them)
   as PDF content in iobuf: TXB[1] cells, their borders at TXB + 4
   (pixels), at TXB + 24 x0, the boxes' bottom, their height
   (thousandths of a point); its bytes (the X16's: v/tbui.c, overlay 28,
   overlay 15's room) */
#ifdef CW_HOST
static unsigned int pdf_boxes(void)
{
    long v[3];
    unsigned int k, n = 0;
    memcpy(v, TXB + 24, sizeof v);
    n = sprintf((char *)iobuf, "0.5 w\n");
    for (k = 0; k < TXB[1]; ++k)
        n += sprintf((char *)iobuf + n, "%.3f %.3f %.3f %.3f re\n", (v[0] + (long)xx[k] * PX) / 1000.0,
                     v[1] / 1000.0, (long)(xx[k + 1] - xx[k]) * PX / 1000.0, v[2] / 1000.0);
    n += sprintf((char *)iobuf + n, "S\n");
    return n;
}
#else
unsigned int f_pdf_boxes(void);
#define pdf_boxes f_pdf_boxes
#endif

static void row(void)
{
    static struct line rl;
    static pos_t rp;
    static long v[3];
    static unsigned int w, a, b, *q;
    static unsigned char n, k;
    memcpy(&rl, &l, sizeof l);
    rp = lp;
    w = wrap_w;
    tb_p = lp;
    tb_s = l.s;
    tb_is = 0xFF;
    TB_PDF();
    n = TXB[0];
    memcpy(xx, TXB + 4, sizeof xx);
    v[0] = X0;
    v[1] = Y0 - px(rtop + rl.h);
    v[2] = px(rl.h);
    if (!(TXB[2] & TR_NOGRID)) {
        memcpy(TXB + 24, v, sizeof v);
        flush();
        ob_n = pdf_boxes();
        flush();
    }
    for (k = 0; k < n; ++k) {
        tb_is = k;
        TB_PDF();
        memcpy(&l, TXB, sizeof l);
        lp = rp + U->off;
        q = xx + U->c;
        a = q[0];
        b = q[1] - a;
        wrap_w = b > TB_PADX + 8 ? b - TB_PADX : 8;
        X0 = v[0] + px(a + TB_PADL);
        lnum = U->num;
        line(ri, rtop + U->y);
    }
    lnum = 0;
    X0 = v[0];
    wrap_w = w;
    lp = rp;
    memcpy(&l, &rl, sizeof l);
}

/*---------------------------------------------------------------
  the header and footer
 ---------------------------------------------------------------*/
/* the header (w 0) or the footer (1) of this page: its parts made by
   pdfhf.c into iobuf, written out one at a time */
static void FASTCALL hf(unsigned char w)
{
    static unsigned char k;
    flush();
    pdf_hfw = w;
    pdf_hfp = pdf_page;
    pdf_hfn = pdf_pages;
    for (k = 0; k < 3; ++k) {
        ob_n = pdf_hf(k);
        flush();
    }
    cf = 0xFF;
}

/*---------------------------------------------------------------
  the document
 ---------------------------------------------------------------*/
unsigned char pdf_write(void)
{
    static unsigned int i, k;
    static unsigned long top, slen;
    static struct line m;
    static unsigned char brk, d;
    static const char *t;
    ob_n = 0;
    perr = 0;
    pdf_size = 0;
    pdf_page = 0;
    nobj = 0;
    memset(xb, 0, sizeof xb);
    ps("%PDF-1.4\n%\xE2\xE3\xCF\xD3\n");
    obj(1);
    ps("<< /Type /Catalog /Pages 2 0 R >>\n");
    endobj();
    fonts();
    obj(13);
    ps("<< /Producer (Veranda Write) /Creator (Veranda Write) /Title (");
    for (t = pdf_title ? pdf_title : ""; *t; ++t)
        if (*t != '(' && *t != ')' && *t != '\\')
            pc(*t);
    ps(") >>\n");
    endobj();
    Y0 = (long)(pgs.h - pgs.top) * 50;
    i = 0;
    lp = 0;
    while (i < lt_n && !perr) {
        /* a page: its content stream, its length, the page */
        ++pdf_page;
#ifdef CW_HOST
        if (pdf_page < sizeof pdf_first / sizeof pdf_first[0])
            pdf_first[pdf_page] = i;
#endif
        k = 14 + 3 * (pdf_page - 1);
        obj(k + 1);
        ps("<< /Length ");
        ref(k + 2);
        ps(">>\nstream\n");
        slen = pdf_size + ob_n;
        cf = 0xFF;
        X0 = (long)pgs.left * 50;
        top = 0;
        for (;;) {
            lt_get(i, &l);
            if (IS_TR(&l)) {
                ri = i;                 /* (1.6: a table's row) */
                rtop = (unsigned int)top;
                row();
            }
            else if (!(l.f & LF_PB))
                line(i, top);
            top += l.h;
            lp += l.len;
            brk = (l.f & LF_PB) != 0;
            if (++i >= lt_n)
                break;
            lt_get(i, &m);
            if (brk || (top && !(m.f & LF_PB) && top + m.h > pg_th))
                break;
        }
        if (HF_ON(pgs.head, pdf_page))
            hf(0);
        if (HF_ON(pgs.foot, pdf_page))
            hf(1);
        slen = pdf_size + ob_n - slen;
        ps("\nendstream\n");
        endobj();
        obj(k + 2);
        pn(slen);
        ps("\n");
        endobj();
        obj(k);
        ps("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ");
        pf((long)pgs.w * 50);
        pf((long)pgs.h * 50);
        ps("] /Resources 12 0 R /Contents ");
        ref(k + 1);
        ps(">>\n");
        endobj();
        if (plat_pdf_poll() && !perr)
            perr = PE_STOP;
    }
    /* the page tree */
    obj(2);
    ps("<< /Type /Pages /Count ");
    pn(pdf_page);
    ps("/Kids [");
    for (i = 0; i < pdf_page; ++i) {
        ref(14 + 3 * i);
        if ((i & 7) == 7)
            pc('\n');
    }
    ps("] >>\n");
    endobj();
    /* the cross-reference table, the trailer */
    top = pdf_size + ob_n;
    ps("xref\n0 ");
    digits(nobj);
    ps("\n0000000000 65535 f \n");
    for (i = 1; i < nobj && !perr; ++i) {
        pv = off_of(i);
        k = pdf_dec();
        for (d = k; d < 10; ++d)
            pc('0');
        ps(pd);
        ps(" 00000 n \n");
    }
    ps("trailer\n<< /Size ");
    pn(nobj);
    ps("/Root 1 0 R /Info 13 0 R >>\nstartxref\n");
    digits(top);
    ps("\n%%EOF\n");
    flush();
    for (k = 0; k < XB; ++k)
        if (xb[k])
            bk_put(xb[k]);
    return perr;
}
