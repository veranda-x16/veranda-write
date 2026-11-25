/* Veranda Write: a .docx's or an .odt's text put into the document
   (DECISIONS W50; office.h): the paragraphs and their styles, page
   breaks, the character styles; the styles a file names (a table of
   their properties, by a hash of the name); the header and the footer.

   The text goes in as format.c's and gwimport.c's readers put it: into
   the chunked buffer at its end, a buffer at a time (EOB); a paragraph's
   CR before the next one (the document's last has none), its paragraph
   code first in it, a character style code where the style changes. A
   page break is a paragraph of its own ([$12 0 $12] and its CR: a break
   inside a paragraph ends it, and its rest starts the next).

   What the file says of a paragraph or a run is a struct prop, each
   field "not said" (P_NO) or a value; the properties in force are the
   paragraph's own, then its style's (with the styles it is based on),
   then the document's defaults. Write's named styles keep their own
   spacing and sizes: a heading or a list paragraph is the named style
   with what the paragraph itself says (its alignment, indents, spacing);
   a body or quotation paragraph also takes its style's.

   The readers (odocx.c, oodt.c: overlays of their own on the X16) hold
   no properties: they set them here (pr_set: a field of the paragraph's,
   the run's, the defaults' or the style being defined), and say which
   style a paragraph or a run is in (em_ps, em_rs: main RAM).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CW_HOST
#pragma code-name ("OVERLAY18")
#pragma rodata-name ("OVL18RO")
#pragma bss-name ("OVL18BSS")
#define em_init e_em_init
#define ox_load e_ox_load
#define em_done e_em_done
#define em_par e_em_par
#define em_need e_em_need
#define em_endpar e_em_endpar
#define em_split e_em_split
#define em_pbreak e_em_pbreak
#define em_run e_em_run
#define em_chars e_em_chars
#define em_char e_em_char
#define em_spaces e_em_spaces
#define em_field e_em_field
#define em_hfbegin e_em_hfbegin
#define em_hfend e_em_hfend
#define em_finish e_em_finish
#define em_page e_em_page
#define pr_set e_pr_set
#define pr_pct e_pr_pct
#define pr_clr e_pr_clr
#define st_find e_st_find
#define st_new e_st_new
#define st_base e_st_base
#define st_auto e_st_auto
#define em_pstyle e_em_pstyle
#define em_mp e_em_mp
#define em_span e_em_span
#define em_unspan e_em_unspan
#endif
#include <string.h>
#include "office.h"
#include "oimp.h"

struct prop em_p, em_r;                 /* the paragraph's own, the run's own */
struct prop em_def;                     /* the document's defaults */
static struct prop dummy;
unsigned char em_hf;                    /* 0: the document; 1 the header, 2 the footer, 3 neither */

static unsigned char first, begun, pb_in, last_pb, on, cs, fsans, fmono, rkey, rst, pbb, pba;
static unsigned char hfn, hft, hfa;
static char hfb[HF_MAX + 1];
static unsigned char err;

/* the styles: each one's name's hash and (an automatic style's) parent
   here, its properties in a RAM bank of their own (16 bytes each: a
   document of LibreOffice's has hundreds); the one being defined in
   cur (put in the bank before any is read) */
static unsigned int st_h[OS_N];
static unsigned char st_par[OS_N];
static unsigned char sty_n, cur_st, cur_d, sbank;
static struct prop cur, sp;
#define SCR ((unsigned char *)EFN)      /* (main RAM: bk_rd's and bk_wr's) */

#ifdef CW_HOST
static void bk_rd(unsigned char b, unsigned int a, unsigned char *d, unsigned int n)
{
    memcpy(d, host_bank[b] + (a - 0xA000), n);
}

static void bk_wr(unsigned char b, unsigned int a, const unsigned char *s, unsigned int n)
{
    memcpy(host_bank[b] + (a - 0xA000), s, n);
}
#else
void bk_wr(unsigned char b, unsigned int a, const unsigned char *src, unsigned int n);
void bk_rd(unsigned char b, unsigned int a, unsigned char *dst, unsigned int n);
#endif

static void flushcur(void)
{
    if (!cur_d)
        return;
    cur_d = 0;
    memcpy(SCR, &cur, sizeof cur);
    bk_wr(sbank, 0xA000 + (cur_st << 4), SCR, sizeof cur);
}

/* style i's properties into sp */
static struct prop *FASTCALL sget(unsigned char i)
{
    flushcur();
    bk_rd(sbank, 0xA000 + (i << 4), SCR, sizeof sp);
    memcpy(&sp, SCR, sizeof sp);
    return &sp;
}
#ifdef CW_HOST
unsigned char em_ps, em_rs, def_ps, pr_t;   /* (the X16's: main.c, main RAM) */
#endif

/*---------------------------------------------------------------
  properties
 ---------------------------------------------------------------*/
static void FASTCALL pr_clear(struct prop *p)
{
    memset(p, P_NO, sizeof *p);
    p->first = P_NOS;
    p->fset = p->fval = 0;
    p->size = 0;
}

/* d's fields not said taken from s */
static void FASTCALL pr_merge(struct prop *d, const struct prop *s)
{
    static unsigned char *a;
    static const unsigned char *b;
    static unsigned char i;
    a = (unsigned char *)d;
    b = (const unsigned char *)s;
    for (i = 0; i < P_BYTES; ++i)
        if (a[i] == P_NO)
            a[i] = b[i];
    if (d->first == P_NOS)
        d->first = s->first;
    d->fval = (d->fval & d->fset) | (s->fval & s->fset & ~d->fset);
    d->fset |= s->fset;
    if (!d->size)
        d->size = s->size;
}

static struct prop *tgt(void)
{
    switch (pr_t) {
    case PT_PAR: return &em_p;
    case PT_RUN: return &em_r;
    case PT_DEF: return &em_def;
    case PT_STY: cur_d = 1; return &cur;
    }
    return &dummy;
}

/* a property of pr_t's: PR(field, value) */
void FASTCALL pr_set(unsigned int fv)
{
    static struct prop *p;
    static unsigned char f, v;
    p = tgt();
    f = (unsigned char)(fv >> 8);
    v = (unsigned char)fv;
    if (f < F_FIRST)
        ((unsigned char *)p)[f] = v;
    else if (f == F_FIRST)
        p->first = (signed char)v;
    else if (f == F_SIZE)
        p->size = v;
    else if (f == F_KIND1) {
        if (p->kind == P_NO)
            p->kind = v;
    } else {
        p->fset |= f & SF_KNOWN;
        if (f & F_ON)
            p->fval |= f & SF_KNOWN;
        else
            p->fval &= ~(f & SF_KNOWN);
    }
}

/* the size: pct per cent of the target's (its parent's: merged), or the defaults' */
void FASTCALL pr_pct(unsigned int pct)
{
    static struct prop *p;
    static unsigned long z;
    p = tgt();
    z = p->size ? p->size : em_def.size ? em_def.size : DEF_SIZE;
    z = (z * pct + 50) / 100;
    p->size = z < 1 ? 1 : z > 127 ? 127 : (unsigned char)z;
}

void FASTCALL pr_clr(unsigned char t)
{
    pr_t = t;
    pr_clear(tgt());
}

/* the style named by hash h: its index (0xFF: none) */
unsigned char FASTCALL st_find(unsigned int h)
{
    static unsigned char i;
    for (i = 0; i < sty_n; ++i)
        if (st_h[i] == h)
            return i;
    return 0xFF;
}

/* a style defined: its index (its properties cleared, the target now),
   0xFF when the table is full (the target none) */
unsigned char FASTCALL st_new(unsigned int h)
{
    static unsigned char i;
    i = st_find(h);
    if (i == 0xFF) {
        if (sty_n >= OS_N) {
            oxc.sty = 1;
            pr_t = PT_NONE;
            return 0xFF;
        }
        i = sty_n++;
    }
    flushcur();
    st_h[i] = h;
    st_par[i] = 0xFF;
    cur_st = i;
    pr_clr(PT_STY);
    return i;
}

/* the style being defined: what style i says, where it says nothing */
void FASTCALL st_base(unsigned char i)
{
    if (i < sty_n && i != cur_st) {
        sget(i);
        pr_merge(&cur, &sp);
        cur_d = 1;
    }
}

void FASTCALL st_auto(unsigned char i)
{
    st_par[cur_st] = i < sty_n ? i : 0xFE;
}

void FASTCALL em_pstyle(unsigned char i)
{
    if (i >= sty_n)
        return;
    if (st_par[i] == 0xFF) {
        em_ps = i;
        return;
    }
    pr_merge(&em_p, sget(i));           /* (what the paragraph's own style says: its own) */
    em_ps = st_par[i] < sty_n ? st_par[i] : def_ps;
}

unsigned char em_mp(void)
{
    return em_p.mp != P_NO ? em_p.mp : em_ps < sty_n ? sget(em_ps)->mp : P_NO;
}

/* (.odt) a span: the run's properties kept, its style's over them */
#define NSPAN 6
static struct prop spans[NSPAN];
static unsigned char nspan;

void FASTCALL em_span(unsigned char i)
{
    static struct prop q;
    if (nspan < NSPAN)
        memcpy(&spans[nspan++], &em_r, sizeof em_r);
    if (i < sty_n) {
        memcpy(&q, sget(i), sizeof q);
        pr_merge(&q, &em_r);
        memcpy(&em_r, &q, sizeof q);
    }
    rkey = 0xFF;
}

void em_unspan(void)
{
    if (nspan)
        memcpy(&em_r, &spans[--nspan], sizeof em_r);
    rkey = 0xFF;
}

/*---------------------------------------------------------------
  the text
 ---------------------------------------------------------------*/
static void FASTCALL put(unsigned char c)
{
    EOB[on++] = c;
    if (on == EOBN) {
        if (cb_append(&tx, EOB, EOBN, BANK_SIZE - 64))
            err = FE_MEM;
        on = 0;
    }
}

static void FASTCALL code(unsigned char k, unsigned char a)
{
    put(k);
    put(a);
    put(k);
}

unsigned char em_init(void)
{
    first = 1;
    begun = pb_in = last_pb = 0;
    on = err = 0;
    cs = 0;
    rkey = 0xFF;
    em_hf = 0;
    sty_n = 0;
    cur_d = 0;
    nspan = 0;
    def_ps = 0xFF;
    pr_clear(&em_def);
    memset(&oxc, 0, sizeof oxc);
    fmono = 0xFF;                       /* (Gate Mono's index: when a run is in it) */
    fsans = 0;                          /* (def_font, sty_reset's) */
    sbank = bk_get();
    return !sbank;
}

void em_done(void)
{
    if (sbank)
        bk_put(sbank);
    sbank = 0;
}

/* a paragraph starts: what the file says of it, from here */
void em_par(void)
{
    pr_clear(&em_p);
    em_ps = def_ps;
    begun = 0;
    pb_in = 0;
    if (em_hf && hfn && hfn < HF_MAX)
        hfb[hfn++] = ' ';               /* (a header's paragraphs: one line) */
}

/* the paragraph's style in the document */
static unsigned char par_style(void)
{
    static struct prop q;
    static unsigned char k;
    static int f;
    struct pstyle *p = EPS;
    memcpy(&q, &em_p, sizeof q);
    if (em_ps != 0xFF)
        pr_merge(&q, sget(em_ps));
    pr_merge(&q, &em_def);
    k = q.kind != P_NO && IS_HEAD(q.kind) ? q.kind : q.list != P_NO && q.list ? q.list : q.kind != P_NO ? q.kind
        : PK_BODY;                      /* (a heading in a list: a heading) */
    if (k >= PK_N)
        k = PK_BODY;
    memcpy(p, &named[k], sizeof *p);
    if (IS_HEAD(k) || IS_LIST(k)) {
        f = q.pbb;
        memcpy(&q, &em_p, sizeof q);    /* (a named style's spacing: only what the paragraph says) */
        q.pbb = (unsigned char)f;
    }
    if (q.align != P_NO)
        p->align = q.align;
    if (!IS_LIST(k) || (q.first != P_NOS && q.first < 0)) {   /* (a list: a hanging indent its own) */
        if (q.left != P_NO)
            p->left = q.left;
        if (q.right != P_NO)
            p->right = q.right;
        if (q.first != P_NOS) {
            f = q.first;
            if (f < -(int)p->left)
                f = -(int)p->left;
            p->first = (signed char)(f > 120 ? 120 : f);
        }
    }
    if (q.before != P_NO)
        p->before = q.before;
    if (q.after != P_NO)
        p->after = q.after;
    if (q.spacing != P_NO)
        p->spacing = q.spacing;
    pbb = q.pbb == 1;
    pba = q.pba == 1;
    hfa = p->align;
    if (em_hf)
        return 0;
    k = par_find(p);
    if (k == 0xFF) {
        oxc.sty = 1;
        k = par_find(&named[p->kind]);  /* (the table full: the named style's, if it is there) */
        if (k == 0xFF)
            k = 0;
    }
    return k;
}

/* the paragraph begun in the document: a page break before it (its
   style's), its CR before, its code */
void em_need(void)
{
    static unsigned char k;
    if (begun)
        return;
    k = par_style();
    begun = 1;
    if (em_hf)
        return;
    if (pbb && !first && !last_pb) {
        em_pbreak();
        begun = 1;
    }
    last_pb = 0;
    if (!first)
        put(CH_CR);
    first = 0;
    if (k)
        code(K_PS, k);
    rkey = 0xFF;
}

/* the paragraph's end (an empty one is a paragraph too; not the rest of
   one a page break ended); a page break after it (its style's) */
void em_endpar(void)
{
    if (!begun && !pb_in)
        em_need();
    begun = 0;
    if (pba && !em_hf) {
        pba = 0;
        em_pbreak();
    }
}

/* a line's end inside a paragraph: its rest the next paragraph */
void em_split(void)
{
    if (!begun)
        em_need();
    begun = 0;
}

void em_pbreak(void)
{
    if (em_hf)
        return;
    if (!first)
        put(CH_CR);
    code(K_PB, 0);
    first = 0;
    begun = 0;
    pb_in = last_pb = 1;
}

/* the run's style: what it says, its character style's, its paragraph's
   style's, the defaults'; a heading bold in its named size unless the
   run says otherwise */
static unsigned char run_style(void)
{
    static struct prop q;
    static unsigned char k, f, z, m;
    memcpy(&q, &em_r, sizeof q);
    if (em_rs != 0xFF)
        pr_merge(&q, sget(em_rs));
    k = em_p.kind;
    if (k == P_NO && em_ps != 0xFF)
        k = sget(em_ps)->kind;
    if (k != P_NO && IS_HEAD(k)) {
        if (!q.size)
            q.size = k == PK_H1 ? 27 : k == PK_H2 ? 18 : 12;
        if (!(q.fset & SF_BOLD)) {
            q.fset |= SF_BOLD;
            q.fval |= SF_BOLD;
        }
    }
    if (em_ps != 0xFF)
        pr_merge(&q, sget(em_ps));
    pr_merge(&q, &em_def);
    f = q.fval & q.fset & SF_KNOWN;
    m = q.mono == 1;
    z = q.size ? q.size : DEF_SIZE;
    if (m)
        z = z <= 11 ? 10 : z <= 14 ? 12 : z <= 21 ? 18 : 27;   /* (Gate Mono has 10 and 12: a heading's kept) */
    else
        z = z <= 10 ? 9 : z <= 14 ? 12 : z <= 21 ? 18 : 27;
    if (m && fmono == 0xFF) {
        strcpy(EFN, "Gate Mono");       /* (main RAM: font_index is overlay 13's) */
        fmono = font_index(EFN);
        if (fmono == 0xFF)
            fmono = fsans;
    }
    k = sty_find(m ? fmono : fsans, z, f, 0);
    if (k == 0xFF) {
        oxc.sty = 1;
        k = 0;
    }
    return k;
}

/* the run changed: its style found again at its next character */
void em_run(void)
{
    rkey = 0xFF;
}

/* n characters of the run (the set's bytes, TAB) */
void FASTCALL em_chars(const char *s, unsigned char n)
{
    static unsigned char c;
    if (!n)
        return;
    em_need();
    if (em_hf) {
        while (n--) {
            c = (unsigned char)*s++;
            if (c == CH_TAB) {
                if (hft < 2) {
                    ++hft;
                    c = '|';
                } else
                    c = ' ';
            }
            if (hfn < HF_MAX)
                hfb[hfn++] = c;
            else
                oxc.hf = 1;
        }
        return;
    }
    if (rkey == 0xFF) {
        rst = run_style();
        rkey = 0;
    }
    if (rst != cs) {
        code(K_CS, rst);
        cs = rst;
    }
    while (n--)
        put((unsigned char)*s++);
}

void FASTCALL em_char(unsigned char c)
{
    static char b[1];
    b[0] = (char)c;
    em_chars(b, 1);
}

void FASTCALL em_spaces(unsigned char n)
{
    while (n--)
        em_char(' ');
}

static const char *const ftext[4] = { "{page}", "{pages}", "{date}", "{name}" };

void FASTCALL em_field(unsigned char k)
{
    em_chars(ftext[k - 1], (unsigned char)strlen(ftext[k - 1]));
}

/* a header (k 1) or footer (2) begins; ends: into the page setup */
void FASTCALL em_hfbegin(unsigned char k)
{
    em_hf = k;
    hfn = hft = 0;
    hfa = PA_LEFT;
    begun = 0;
}

void em_hfend(void)
{
    static char *d;
    static unsigned char n;
    d = em_hf == 1 ? pgs.head : pgs.foot;
    em_hf = 0;
    while (hfn && hfb[hfn - 1] == ' ')
        --hfn;
    hfb[hfn] = 0;
    n = 0;
    /* one part: as its paragraph is aligned (centred: as it is; left: the
       left part; right: the right one) */
    if (hfn && !hft && hfa == PA_RIGHT)
        d[n++] = '|';
    if (hfn > HF_MAX - 1 - n) {
        hfn = HF_MAX - 1 - n;
        oxc.hf = 1;
    }
    memcpy(d + n, hfb, hfn);
    n += hfn;
    if (hfn && !hft && hfa != PA_RIGHT && hfa != PA_CENTRE)
        d[n++] = '|';
    d[n] = 0;
}

/* the text's end; 0 done, else FE_ */
unsigned char em_finish(void)
{
    if (last_pb)
        put(CH_CR);                     /* (a page break last: an empty paragraph after it) */
    if (on && cb_append(&tx, EOB, on, BANK_SIZE - 64))
        err = FE_MEM;
    on = 0;
    if (!err && tx.total > FMT_MAXTEXT)
        err = FE_BIG;
    return err;
}

/* the page setup the file gave (pgs): A4 or Letter when it is one of
   them (within a millimetre), the text's width at most the screen's
   page (the left and right margins widened alike), checked (pg_apply) */
static unsigned char FASTCALL nearly(unsigned int a, unsigned int b)
{
    return a + 60 > b && a < b + 60;
}

void em_page(void)
{
    static unsigned int w, m;
    if (nearly(pgs.w, 11906) && nearly(pgs.h, 16838)) {
        pgs.paper = PP_A4;
        pgs.w = 11906;
        pgs.h = 16838;
    } else if (nearly(pgs.w, 12240) && nearly(pgs.h, 15840)) {
        pgs.paper = PP_LETTER;
        pgs.w = 12240;
        pgs.h = 15840;
    } else
        pgs.paper = PP_OTHER;
    if (pgs.w > pgs.left + pgs.right) {
        w = pgs.w - pgs.left - pgs.right;
        if (w > TXW_MAX * TW_PX) {
            m = (w - TXW_MAX * TW_PX + 1) / 2;
            pgs.left += m;
            pgs.right += m;
        }
    }
    pg_apply();
}

/*---------------------------------------------------------------
  which kind of document: a .docx has word/document.xml, an .odt
  content.xml (and its mimetype says so)
 ---------------------------------------------------------------*/
unsigned char FASTCALL ox_load(unsigned char h)
{
    static unsigned char e;
    doc_new();
    cb_clear(&tx);
    if (oz_open(h))
        return oz_err == OZ_IO ? FE_IO : FE_NOTCW;
    strcpy((char *)EOB, "word/document.xml");
    if (!oz_part((char *)EOB)) {
        e = dx_load();
        oxc.kind = OK_DOCX;             /* (after em_init's clearing) */
    } else {
        strcpy((char *)EOB, "content.xml");
        if (oz_part((char *)EOB)) {
            oz_close();
            return oz_err == OZ_NONE ? FE_NOTCW : oz_err == OZ_MEM ? FE_MEM : FE_BAD;
        }
        e = od_load();
        oxc.kind = OK_ODT;
    }
    em_done();
    oz_close();
    if (e)
        return e;
    MARK("ox>");
    fl_flush();
    if (lay_all())
        return FE_MEM;
    MARK("lay");
    doc_modified = 1;                   /* (saved nowhere yet: Untitled) */
    return 0;
}
