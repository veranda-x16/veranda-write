/* Veranda Write: the document model's unit tests, on the host (make test).

   1. cbuf: random insertions and deletions, small and over several banks,
      against a plain array.
   2. the document: random edits (typing in styles, deleting, formatting,
      undo, redo, typing runs, paragraph styles: alignment, indents,
      spacing, the named styles with their characters) against a model, a
      list of (character, style) pairs and a list of the paragraphs'
      styles, with a copy for each step; after each, the document's
      characters, their styles and its paragraphs' styles as the model's,
      its codes whole and in their places (a paragraph code first in its
      paragraph, never style 0), the line table as a fresh layout of the
      whole text makes it, each line's width and its text's place (the
      alignment, the indents, justified spaces) as measured.
   3. the file: saved and loaded back (the same characters and styles);
      damaged and foreign files refused or cleaned; plain text with its
      three line ends and UTF-8.
   4. a big document (200 KB, more than the X16's main RAM): typed in
      pieces, edited at both ends, saved, loaded.
   5-6. paging; the pages (v/pages.c) with page breaks against counting
      every line.
   7. stage 3 (W44-W48): the header's and footer's fields; the page setup
      (CWRD 1.4's PAGE: defaults, checks, saved and read back, older
      files); page breaks typed (their lines, the pages they start, the
      view's page starts); the PDF (pdf.c: its pages as the pages',
      its cross-reference table, its text; build/host's PDF files, which
      tools/check_pdf.py checks after with qpdf and pdftotext).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cbpriv.h"
#include "host.h"
#include "../src/spell.h"

static int fails, checks;
#define CHECK(c, ...) do { ++checks; if (!(c)) { ++fails; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); if (fails > 20) exit(1); } } while (0)

static unsigned long rnd_s = 12345;
static unsigned int rnd(unsigned int n)
{
    rnd_s = rnd_s * 1103515245UL + 12345UL;
    return (unsigned int)((rnd_s >> 8) % n);
}

/*---------------------------------------------------------------
  1. cbuf
 ---------------------------------------------------------------*/
static void test_cbuf(void)
{
    static unsigned char ref[200000], tmp[20000];
    static unsigned char bk[60];
    static unsigned int ln[60];
    struct cbuf c;
    unsigned long n = 0, at, k, i;
    int step;
    cb_init(&c, bk, ln, sizeof bk);
    for (step = 0; step < 3000; ++step) {
        if (rnd(3) || n < 100) {
            k = rnd(10) ? rnd(300) + 1 : rnd(20000) + 1;
            if (n + k > sizeof ref)
                continue;
            at = rnd((unsigned int)n + 1);
            for (i = 0; i < k; ++i)
                tmp[i] = (unsigned char)rnd(256);
            if (cb_insert(&c, at, tmp, (unsigned int)k))
                continue;               /* (out of chunks) */
            memmove(ref + at + k, ref + at, n - at);
            memcpy(ref + at, tmp, k);
            n += k;
        } else {
            at = rnd((unsigned int)n);
            k = rnd(10) ? rnd(200) + 1 : rnd(30000) + 1;
            if (k > n - at)
                k = n - at;
            cb_delete(&c, at, k);
            memmove(ref + at, ref + at + k, n - at - k);
            n -= k;
        }
        CHECK(c.total == n, "cbuf total %lu, want %lu", c.total, n);
        if (step % 50 == 0 || step > 2990) {
            static unsigned char back[200000];
            unsigned long sum = 0;
            for (i = 0; i < c.n; ++i) {
                CHECK(ln[i] > 0 && ln[i] <= BANK_SIZE, "chunk %lu length %u", i, ln[i]);
                sum += ln[i];
            }
            CHECK(sum == n, "chunks sum %lu", sum);
            CHECK(cb_read(&c, 0, back, (unsigned int)(n > 65535 ? 65535 : n)) == (n > 65535 ? 65535 : n), "read");
            CHECK(!memcmp(back, ref, n > 65535 ? 65535 : n), "cbuf bytes differ at step %d", step);
            if (n > 10) {
                at = rnd((unsigned int)n);
                CHECK(cb_byte(&c, at) == ref[at], "byte at %lu", at);
            }
        }
    }
    /* overwrite and append */
    if (n > 5000) {
        memset(tmp, 'w', 3000);
        cb_write(&c, 1000, tmp, 3000);
        memset(ref + 1000, 'w', 3000);
        CHECK(cb_byte(&c, 2500) == 'w' && cb_byte(&c, 999) == ref[999] && cb_byte(&c, 4000) == ref[4000], "write");
    }
    cb_clear(&c);
    CHECK(c.total == 0 && c.n == 0, "cleared");
    for (i = 0; i < 20000; ++i)
        tmp[i] = (unsigned char)i;
    CHECK(!cb_append(&c, tmp, 20000, 7000) && c.n == 3 && ln[0] == 7000 && ln[2] == 6000, "append fills chunks");
    cb_clear(&c);
    printf("cbuf: ok\n");
}

/*---------------------------------------------------------------
  2. the document against a model
 ---------------------------------------------------------------*/
struct mc { unsigned char c, font, size, flags; };
#define MMAX 6000
struct model { int n; struct mc a[MMAX]; int np; struct pstyle p[MMAX + 1]; };
static struct model cur_m;
static struct model hist[64];          /* the states before each step, for undo */
static int nhist, nredo, hist_full;
static struct model redo_m[64];

/* the document decoded: its characters, their styles, their places */
static struct mc dec[MMAX + 10];
static pos_t decpos[MMAX + 10];
static int ndec;
static struct pstyle decp[MMAX + 10];
static int ndecp;
static unsigned char raw[MMAX * 8];
static const char *dec_why;
static int dead_ok = 1;                 /* (codes that do nothing: allowed) */

static int decode(void)
{
    pos_t n = DOC_LEN, i, pstart = 0;
    unsigned char s = 0, c;
    ndec = 0;
    ndecp = 1;
    decp[0] = par[0];
    dec_why = "";
    if (n > sizeof raw)
        return 0;
    cb_read(&tx, 0, raw, (unsigned int)n);
    for (i = 0; i < n;) {
        c = raw[i];
        if (IS_KIND(c)) {
            if (i + 2 >= n || raw[i + 2] != c) {
                dec_why = "a broken code";
                return 0;
            }
            if (c == K_CS) {
                if (raw[i + 1] >= nsty)
                    return 0;
                if (raw[i + 1] == s && dead_ok == 0) {
                    dec_why = "a style code that does nothing (the style in effect)";
                    return 0;
                }
                if ((i + 3 >= n || raw[i + 3] == K_CS) && dead_ok == 0) {
                    dec_why = "a style code that does nothing (at the end, or before another)";
                    return 0;
                }
                s = raw[i + 1];
            }
            if (c == K_PS) {
                if (i != pstart) {
                    dec_why = "a paragraph code not first in its paragraph";
                    return 0;
                }
                if (!raw[i + 1] || raw[i + 1] >= npar) {
                    dec_why = "a paragraph code of style 0 or none";
                    return 0;
                }
                decp[ndecp - 1] = par[raw[i + 1]];
            }
            i += 3;
            continue;
        }
        if (c == CH_CR) {
            pstart = i + 1;
            decp[ndecp++] = par[0];
        }
        if (!IS_CHAR(c) && c != CH_CR)
            return 0;
        dec[ndec].c = c;
        dec[ndec].font = sty[s].font;
        dec[ndec].size = sty[s].size;
        dec[ndec].flags = sty[s].flags;
        decpos[ndec] = i;
        ++ndec;
        ++i;
    }
    decpos[ndec] = n;
    return 1;
}

static void check_layout(const char *what)
{
    static unsigned char a[200000], b[200000];
    unsigned int n1 = lt_n, i;
    pos_t sum = 0;
    struct line l;
    CHECK(lt.total == (pos_t)lt_n * LT_SZ, "%s: table size", what);
    if (lt.total > sizeof a)
        return;
    cb_read(&lt, 0, a, (unsigned int)lt.total);
    for (i = 0; i < lt_n; ++i) {
        lt_get(i, &l);
        sum += l.len;
    }
    CHECK(sum == DOC_LEN, "%s: lines sum %lu, text %lu", what, sum, DOC_LEN);
    lay_all();
    cb_read(&lt, 0, b, (unsigned int)lt.total);
    if (lt_n != n1 || memcmp(a, b, lt.total)) {
        printf("text:");
        for (i = 0; i < DOC_LEN; ++i) printf(" %02X", doc_byte(i));
        printf("\nincremental:");
        for (i = 0; i < n1; ++i) printf(" [%d h%d a%d s%d f%d]", a[i*6] | a[i*6+1] << 8, a[i*6+2], a[i*6+3], a[i*6+4], a[i*6+5]);
        printf("\nfresh:      ");
        for (i = 0; i < lt_n; ++i) printf(" [%d h%d a%d s%d f%d]", b[i*6] | b[i*6+1] << 8, b[i*6+2], b[i*6+3], b[i*6+4], b[i*6+5]);
        printf("\n");
    }
    CHECK(lt_n == n1, "%s: %u lines, a fresh layout %u", what, n1, lt_n);
    if (lt_n == n1)
        for (i = 0; i < n1 * LT_SZ; ++i)
            if (a[i] != b[i]) {
                CHECK(0, "%s: line %u differs from a fresh layout (byte %u: %d, %d)", what, i / LT_SZ, i % LT_SZ, a[i], b[i]);
                break;
            }
}

/* each line's width (its characters to the last that is not a space)
   and its text's place, measured here */
static void check_lines(const char *what)
{
    static unsigned char b[4096];
    unsigned int i, j, x, vis, avail, nsp, pend;
    struct line l;
    struct pstyle *ps;
    pos_t p = 0;
    unsigned char s, c, first;
    int q = -1;
    unsigned int num = 0;
    for (i = 0; i < lt_n; p += l.len, ++i) {
        lt_get(i, &l);
        if (l.f & LF_FIRST) {
            /* a numbered list's number: 1 + the numbered paragraph before */
            ++q;
            num = par[l.p].kind != PK_NUMBER ? 0 : q > 0 && ndecp > q && decp[q - 1].kind == PK_NUMBER ? num + 1 : 1;
        }
        CHECK(par_number(i) == num, "%s: line %u's number %u, want %u", what, i, par_number(i), num);
        if (l.len > sizeof b)
            continue;
        cb_read(&tx, p, b, l.len);
        s = l.s;
        fl_style(s);
        x = vis = nsp = pend = 0;
        for (j = 0; j < l.len; ++j) {
            c = b[j];
            if (IS_KIND(c)) {
                if (c == K_CS)
                    fl_style(s = b[j + 1]);
                j += 2;
                continue;
            }
            if (c == CH_CR)
                break;
            if (c == CH_TAB) {
                x += TABW - x % TABW;
                vis = x;
            } else if (c == ' ') {
                x += CHW(c);
                ++pend;
            } else {
                x += CHW(c);
                vis = x;
                nsp += pend;
                pend = 0;
            }
        }
        CHECK(l.w == vis, "%s: line %u's width %u, measured %u", what, i, l.w, vis);
        ps = &par[l.p];
        first = (l.f & LF_FIRST) && !IS_LIST(ps->kind);
        avail = wrap_w - ps->left - ps->right - (first ? ps->first : 0);
        line_geom(&l, p);
        if (vis >= avail || ps->align == PA_LEFT
            || (ps->align == PA_JUSTIFY && ((l.f & LF_PARA) || p + l.len >= DOC_LEN || !nsp)))
            CHECK(lg_x0 == (unsigned)(ps->left + (first ? ps->first : 0)) && !lg_nsp, "%s: line %u's place", what, i);
        else if (ps->align == PA_CENTRE)
            CHECK(lg_x0 == (unsigned)(ps->left + (first ? ps->first : 0)) + (avail - vis) / 2, "%s: line %u centred", what, i);
        else if (ps->align == PA_RIGHT)
            CHECK(lg_x0 + vis == (unsigned)ps->left + (first ? ps->first : 0) + avail, "%s: line %u right", what, i);
        else
            CHECK(lg_nsp == nsp && lg_ex * nsp + lg_rem + vis == avail && lg_rem < nsp,
                  "%s: line %u justified (%u spaces, %u + %u, width %u of %u)", what, i, lg_nsp, lg_ex, lg_rem, vis, avail);
    }
}

static void check_doc(const char *what)
{
    int i;
    CHECK(decode(), "%s: the text is not well formed (%s)", what, dec_why);
    CHECK(ndec == cur_m.n, "%s: %d characters, the model %d", what, ndec, cur_m.n);
    if (ndec == cur_m.n)
        for (i = 0; i < ndec; ++i)
            if (memcmp(&dec[i], &cur_m.a[i], sizeof dec[i])) {
                CHECK(0, "%s: character %d: '%c' %d/%d/%d, the model '%c' %d/%d/%d", what, i,
                      dec[i].c, dec[i].font, dec[i].size, dec[i].flags,
                      cur_m.a[i].c, cur_m.a[i].font, cur_m.a[i].size, cur_m.a[i].flags);
                break;
            }
    CHECK(ndecp == cur_m.np, "%s: %d paragraphs, the model %d", what, ndecp, cur_m.np);
    if (ndecp == cur_m.np)
        for (i = 0; i < ndecp; ++i)
            if (memcmp(&decp[i], &cur_m.p[i], sizeof decp[i])) {
                CHECK(0, "%s: paragraph %d: kind %d align %d left %d, the model kind %d align %d left %d", what, i,
                      decp[i].kind, decp[i].align, decp[i].left, cur_m.p[i].kind, cur_m.p[i].align, cur_m.p[i].left);
                break;
            }
    check_layout(what);
    check_lines(what);
}

/* the model's paragraphs: the one character k is in */
static int m_par(int k)
{
    int i, p = 0;
    for (i = 0; i < k && i < cur_m.n; ++i)
        if (cur_m.a[i].c == CH_CR)
            ++p;
    return p;
}

/* n paragraphs after paragraph p, of style ps */
static void m_par_ins(int p, int n, const struct pstyle *ps)
{
    int i;
    memmove(cur_m.p + p + 1 + n, cur_m.p + p + 1, sizeof cur_m.p[0] * (cur_m.np - p - 1));
    for (i = 0; i < n; ++i)
        cur_m.p[p + 1 + i] = *ps;
    cur_m.np += n;
}

/* a typing at k of s: its CRs' new paragraphs */
static void m_typed(int k, const unsigned char *s, int n)
{
    int i, m = 0, p = m_par(k);
    struct pstyle ps = cur_m.p[p];
    for (i = 0; i < n; ++i)
        m += s[i] == CH_CR;
    if (!m)
        return;
    if (IS_HEAD(ps.kind) && (k == cur_m.n || cur_m.a[k].c == CH_CR))
        ps = par[0];
    m_par_ins(p, m, &ps);
}

/* characters k .. k2 - 1 deleted: their paragraphs joined to the first */
static void m_deleted(int k, int k2)
{
    int i, m = 0, p = m_par(k);
    for (i = k; i < k2; ++i)
        m += cur_m.a[i].c == CH_CR;
    memmove(cur_m.p + p + 1, cur_m.p + p + 1 + m, sizeof cur_m.p[0] * (cur_m.np - p - 1 - m));
    cur_m.np -= m;
}

static struct pstyle m_map(struct pstyle c, unsigned char op, unsigned char v)
{
    int k;
    switch (op) {
    case PF_NAMED: c = named[v]; break;
    case PF_ALIGN: c.align = v; break;
    case PF_INDENT: k = c.left + (signed char)v; c.left = k < 0 ? 0 : k > 240 ? 240 : k; break;
    case PF_SPACING: c.spacing = v; break;
    case PF_AFTER: c.after = v; break;
    }
    return c;
}

static unsigned char model_style(int k)
{
    /* the style typing at visible place k gets: as the document's rule */
    if (k == 0 || cur_m.a[k - 1].c == CH_CR) {
        if (k < cur_m.n)
            return sty_find(cur_m.a[k].font, cur_m.a[k].size, cur_m.a[k].flags, 0);
        if (k > 0)
            return sty_find(cur_m.a[k - 1].font, cur_m.a[k - 1].size, cur_m.a[k - 1].flags, 0);
        return 0;
    }
    return sty_find(cur_m.a[k - 1].font, cur_m.a[k - 1].size, cur_m.a[k - 1].flags, 0);
}

static pos_t decpos_end(void)
{
    return pos_norm(DOC_LEN);
}

static void push_hist(void)
{
    if (nhist == 64) {
        hist_full = 1;
        memmove(hist, hist + 1, sizeof hist[0] * 63);
        --nhist;
    }
    hist[nhist++] = cur_m;
    nredo = 0;
}

static int par_full;

static void test_doc(void)
{
    int step, k, k2, i, n, op;
    unsigned char s[40], t, v;
    char what[64];
    pos_t dpos;
    doc_new();
    ul_init();
    wrap_w = 100;
    cur_m.n = 0;
    cur_m.np = 1;
    cur_m.p[0] = par[0];
    nhist = nredo = 0;
    font_index("Gate Mono");
    for (step = 0; step < 4000; ++step) {
        if (step % 1000 == 0 && step) {
            /* a new document now and then: the tables (128 styles, 32
               paragraph styles) are not filled by the test alone */
            ul_reset();
            doc_new();
            font_index("Gate Mono");
            cur_m.n = 0;
            cur_m.np = 1;
            cur_m.p[0] = par[0];
            nhist = nredo = 0;
            hist_full = 0;
        }
        op = rnd(100);
        decode();
        sprintf(what, "step %d op %d", step, op);
        if (op < 35 && cur_m.n < MMAX - 50) {                   /* typing in a style */
            k = rnd(cur_m.n + 1);
            n = rnd(8) + 1;
            for (i = 0; i < n; ++i)
                s[i] = rnd(12) == 0 ? CH_CR : rnd(6) == 0 ? ' ' : (unsigned char)('a' + rnd(26));
            if (rnd(3)) {
                t = style_typing(decpos[k]);
                CHECK(t == model_style(k), "%s: the typing style", what);
            } else
                t = sty_find((unsigned char)rnd(2), (unsigned char)(9 + 3 * rnd(3)), (unsigned char)rnd(8), 0);
            push_hist();
            ul_step(U_CMD, decpos[k], decpos[k]);
            doc_insert(decpos[k], s, (unsigned int)n, t);
            m_typed(k, s, n);
            memmove(cur_m.a + k + n, cur_m.a + k, sizeof cur_m.a[0] * (cur_m.n - k));
            for (i = 0; i < n; ++i) {
                cur_m.a[k + i].c = s[i];
                cur_m.a[k + i].font = sty[t].font;
                cur_m.a[k + i].size = sty[t].size;
                cur_m.a[k + i].flags = sty[t].flags;
            }
            cur_m.n += n;
        } else if (op < 45 && cur_m.n < MMAX - 50) {           /* a typing run: one step */
            pos_t p;
            k = rnd(cur_m.n + 1);
            n = rnd(10) + 1;
            p = decpos[k];
            t = style_typing(p);
            push_hist();
            for (i = 0; i < n; ++i) {
                s[0] = (unsigned char)('A' + rnd(26));
                ul_step(U_TYPE, p, p);
                cb_split = 1;           /* (as ui.c types: W43's split) */
                p = doc_insert(p, s, 1, t);
                cb_split = 0;
                memmove(cur_m.a + k + 1, cur_m.a + k, sizeof cur_m.a[0] * (cur_m.n - k));
                cur_m.a[k].c = s[0];
                cur_m.a[k].font = sty[t].font;
                cur_m.a[k].size = sty[t].size;
                cur_m.a[k].flags = sty[t].flags;
                ++cur_m.n;
                ++k;
            }
            ul_break();
        } else if (op < 62 && cur_m.n > 0) {                     /* delete */
            k = rnd(cur_m.n);
            k2 = k + 1 + (rnd(4) ? rnd(5) : rnd(80));
            if (k2 > cur_m.n)
                k2 = cur_m.n;
            push_hist();
            ul_step(U_CMD, decpos[k], decpos[k2]);
            dpos = doc_delete(decpos[k], decpos[k2]);
            m_deleted(k, k2);
            memmove(cur_m.a + k, cur_m.a + k2, sizeof cur_m.a[0] * (cur_m.n - k2));
            cur_m.n -= k2 - k;
            /* (W41) the place it returns: character k's now, the end
               after the last (never past it: a code before k taken out
               too) */
            decode();
            CHECK(k < cur_m.n ? dpos == decpos[k] : dpos == DOC_LEN, "%s: delete's place %lu, want %lu (%lu bytes)",
                  what, (unsigned long)dpos, (unsigned long)(k < cur_m.n ? decpos[k] : DOC_LEN), (unsigned long)DOC_LEN);
        } else if (op < 70) {                                    /* a paragraph's style */
            unsigned char pop;
            int p0, p1;
            k = rnd(cur_m.n + 1);
            k2 = k + (rnd(3) ? 0 : rnd(120));
            if (k2 > cur_m.n)
                k2 = cur_m.n;
            pop = (unsigned char)(rnd(6) == 0 ? PF_NAMED : 1 + rnd(4));
            v = pop == PF_NAMED ? (unsigned char)rnd(PK_N) : pop == PF_ALIGN ? (unsigned char)rnd(4)
                : pop == PF_INDENT ? (unsigned char)(rnd(2) ? 16 : -16) : pop == PF_SPACING ? (unsigned char)(2 + rnd(3))
                : (unsigned char)(rnd(2) * 6);
            p0 = m_par(k);
            p1 = m_par(k2 > k ? k2 - 1 : k);
            push_hist();
            ul_step(U_CMD, decpos[k], decpos[k2]);
            tk_a = decpos[k];
            tk_b = decpos[k2];
            if (pop == PF_NAMED && rnd(2)) {
                /* with the characters: doc_named */
                static const unsigned char nsz[PK_N] = { 0, 27, 18, 12, 0, 0, 0 };
                static const unsigned char nfl[PK_N] = { 0, 1, 1, 1, 2, 0, 0 };
                int pp = 0;
                if (getenv("CWDBG") && step == atoi(getenv("CWDBG"))) {
                    printf("named %d over chars %d..%d (paragraphs %d..%d), text:", v, k, k2, p0, p1);
                    for (i = 0; i < (int)DOC_LEN; ++i) printf(" %02X", doc_byte(i));
                    printf("\n");
                }
                doc_named(decpos[k], decpos[k2], v);
                if (getenv("CWDBG") && step == atoi(getenv("CWDBG"))) {
                    printf("after:");
                    for (i = 0; i < (int)DOC_LEN; ++i) printf(" %02X", doc_byte(i));
                    printf("\n");
                }
                CHECK(!doc_err || doc_err == DE_PAR, "%s: named style failed (%d)", what, doc_err);
                for (i = 0; !doc_err && i <= cur_m.n && pp <= p1; ++i) {
                    if (pp >= p0 && i < cur_m.n) {
                        unsigned char o = cur_m.p[pp].kind;
                        cur_m.a[i].flags &= ~nfl[o];
                        if (IS_HEAD(v))
                            cur_m.a[i].size = nsz[v];
                        else if (IS_HEAD(o))
                            cur_m.a[i].size = DEF_SIZE;
                        cur_m.a[i].flags |= nfl[v];
                    }
                    if (i < cur_m.n && cur_m.a[i].c == CH_CR)
                        ++pp;
                }
            } else {
                doc_para(decpos[k], decpos[k2], pop, v);
                CHECK(!doc_err || doc_err == DE_PAR, "%s: paragraph style failed (%d)", what, doc_err);
            }
            if (doc_err)
                ++par_full;
            for (i = p0; i <= p1 && !doc_err; ++i)
                cur_m.p[i] = m_map(cur_m.p[i], pop, v);
            decode();
            CHECK(pos_norm(tk_a) == decpos[k] && pos_norm(tk_b) == decpos[k2], "%s: the places kept", what);
        } else if (op < 82 && cur_m.n > 0) {                     /* format */
            unsigned char fop;
            k = rnd(cur_m.n);
            k2 = k + 1 + rnd(60);
            if (k2 > cur_m.n)
                k2 = cur_m.n;
            fop = (unsigned char)rnd(5);
            v = fop == FMT_FONT ? (unsigned char)rnd(2) : fop == FMT_SIZE ? (unsigned char)(9 + 3 * rnd(4))
                : (unsigned char)(1 << rnd(3));
            push_hist();
            ul_step(U_CMD, decpos[k], decpos[k2]);
            doc_format(decpos[k], decpos[k2], fop, v);
            CHECK(!doc_err, "%s: format failed", what);
            for (i = k; i < k2; ++i)
                switch (fop) {
                case FMT_SET: cur_m.a[i].flags |= v; break;
                case FMT_CLR: cur_m.a[i].flags &= ~v; break;
                case FMT_FONT: cur_m.a[i].font = v; break;
                case FMT_SIZE: cur_m.a[i].size = v; break;
                case FMT_PLAIN: cur_m.a[i].flags = 0; break;
                }
            decode();
            CHECK(tp_a == decpos[k] && tp_b == decpos[k2], "%s: the selection after formatting", what);
        } else if (op < 93) {                                    /* undo */
            if (nhist) {
                CHECK(ul_can_undo(), "%s: undo possible", what);
                CHECK(ul_undo(), "%s: undo", what);
                redo_m[nredo++] = cur_m;
                cur_m = hist[--nhist];
            } else
                CHECK(hist_full || !ul_can_undo(), "%s: nothing to undo", what);
        } else {                                                 /* redo */
            if (nredo) {
                CHECK(ul_redo(), "%s: redo", what);
                hist[nhist++] = cur_m;
                cur_m = redo_m[--nredo];
            } else
                CHECK(!ul_can_redo(), "%s: nothing to redo", what);
        }
        check_doc(what);
        if (fails)
            break;
    }
    printf("document: ok (%d characters, %d paragraphs, %u lines, %u styles, %u paragraph styles, "
           "%d refused: the table full)\n", cur_m.n, cur_m.np, lt_n, nsty, npar, par_full);
}

/*---------------------------------------------------------------
  2b. find and replace against the model: every match from the start
      (case, whole words, a match across style codes), Replace all (the
      model's characters and styles; one undo step back, redo)
 ---------------------------------------------------------------*/
static unsigned char m_fold(unsigned char c, int fcase)
{
    if (!fcase && ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7)))
        return c + 32;
    return c;
}

static int m_word(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'
           || (c >= 0xC0 && c != 0xD7 && c != 0xF7);
}

static int m_match_at(int i, const unsigned char *pat, int len, int flags)
{
    int j;
    if (i + len > cur_m.n)
        return 0;
    for (j = 0; j < len; ++j)
        if (cur_m.a[i + j].c == CH_CR || m_fold(cur_m.a[i + j].c, flags & FF_CASE) != m_fold(pat[j], flags & FF_CASE))
            return 0;
    if (flags & FF_WORD) {
        if (i > 0 && m_word(cur_m.a[i - 1].c))
            return 0;
        if (i + len < cur_m.n && m_word(cur_m.a[i + len].c))
            return 0;
    }
    return 1;
}

static long tot_found, tot_repl;


static void test_find(void)
{
    static int at[MMAX];
    static struct model before;
    int round, i, j, len, nm, flags, k, rn;
    unsigned char pat[FD_MAX + 1], rep[FD_MAX + 1];
    unsigned int got;
    pos_t p;
    char what[64];
    for (round = 0; round < 400; ++round) {
        sprintf(what, "find round %d", round);
        /* a document: the model's text, from test 2's way of typing */
        if (round % 40 == 0) {
            ul_reset();
            doc_new();
            font_index("Gate Mono");
            cur_m.n = 0;
            cur_m.np = 1;
            cur_m.p[0] = par[0];
            for (i = 0; i < 60; ++i) {
                static const char *const w[] = { "the", "The", "THE", "then", "other", "cat", "Cat", "scat",
                                                 "a", "A", "\xe9t\xe9", "\xc9T\xc9", "the_cat", "x1" };
                const char *t = w[rnd(14)];
                unsigned char st = sty_find((unsigned char)rnd(2), (unsigned char)(9 + 3 * rnd(3)),
                                            (unsigned char)rnd(4), 0);
                int n0 = cur_m.n;
                doc_insert(decpos_end(), (const unsigned char *)t, (unsigned int)strlen(t), st);
                decode();
                for (j = 0; t[j]; ++j) {
                    cur_m.a[n0 + j].c = (unsigned char)t[j];
                    cur_m.a[n0 + j].font = sty[st].font;
                    cur_m.a[n0 + j].size = sty[st].size;
                    cur_m.a[n0 + j].flags = sty[st].flags;
                }
                cur_m.n = n0 + j;
                k = rnd(8);
                doc_insert(decpos_end(), (const unsigned char *)(k == 0 ? "\r" : k == 1 ? "\t" : " "), 1, st);
                cur_m.a[cur_m.n].c = k == 0 ? CH_CR : k == 1 ? CH_TAB : ' ';
                cur_m.a[cur_m.n].font = sty[st].font;
                cur_m.a[cur_m.n].size = sty[st].size;
                cur_m.a[cur_m.n].flags = sty[st].flags;
                ++cur_m.n;
                if (k == 0)
                    cur_m.p[cur_m.np++] = par[0];
            }
            check_doc(what);
        }
        decode();
        /* a pattern: a piece of the text (no CR), its case changed now and then */
        do {
            i = rnd(cur_m.n);
            len = 1 + rnd(6);
            for (j = 0; j < len && i + j < cur_m.n && cur_m.a[i + j].c != CH_CR; ++j)
                pat[j] = cur_m.a[i + j].c;
            len = j;
        } while (!len);
        if (rnd(3) == 0)
            for (j = 0; j < len; ++j)
                pat[j] = pat[j] >= 'a' && pat[j] <= 'z' ? pat[j] - 32 : pat[j];
        pat[len] = 0;
        flags = rnd(4);
        /* the model's matches, left to right, not overlapping */
        nm = 0;
        for (i = 0; i <= cur_m.n - len;)
            if (m_match_at(i, pat, len, flags)) {
                at[nm++] = i;
                i += len;
            } else
                ++i;
        find_set((const char *)pat, (unsigned char)flags);
        p = 0;
        for (k = 0; find_from(p, DOC_LEN); ++k) {
            if (k >= nm || fd_lo != decpos[at[k]] || pos_norm(fd_hi) != decpos[at[k] + len]) {
                if (getenv("CWDBG")) {
                    printf("text:");
                    for (i = 0; i < 80 && i < (int)DOC_LEN; ++i) printf(" %02X", doc_byte(i));
                    printf("\nmodel:");
                    for (i = 0; i < 20 && i < cur_m.n; ++i) printf(" %02X@%lu", cur_m.a[i].c, decpos[i]);
                    printf("\n");
                }
                CHECK(0, "%s: match %d at %lu, the model's %d at %lu (pattern \"%s\", flags %d)", what, k, fd_lo,
                      k < nm ? at[k] : -1, k < nm ? decpos[at[k]] : 0, pat, flags);
                break;
            }
            CHECK(find_here(fd_lo, fd_hi), "%s: find_here", what);
            p = fd_hi;
        }
        tot_found += k;
        CHECK(k == nm, "%s: %d matches, the model %d (pattern \"%s\", flags %d)", what, k, nm, pat, flags);
        if (fails)
            return;
        /* replace all (a third of the rounds): one step, undone, redone */
        if (round % 3 == 0) {
            rn = rnd(5);
            for (j = 0; j < rn; ++j)
                rep[j] = (unsigned char)('a' + rnd(26));
            before = cur_m;
            dead_ok = 0;
            check_doc("before replace all");
            dead_ok = 1;
            ul_step(U_CMD, 0, 0);
            got = replace_all(rep, (unsigned char)rn);
            tot_repl += got;
            dead_ok = 0;                /* (replace all leaves none) */
            CHECK(got == (unsigned int)nm && !doc_err, "%s: replaced %u, the model %d", what, got, nm);
            for (k = nm - 1; k >= 0; --k) {
                struct mc first = cur_m.a[at[k]];
                memmove(cur_m.a + at[k] + rn, cur_m.a + at[k] + len, sizeof cur_m.a[0] * (cur_m.n - at[k] - len));
                for (j = 0; j < rn; ++j) {
                    cur_m.a[at[k] + j] = first;
                    cur_m.a[at[k] + j].c = rep[j];
                }
                cur_m.n += rn - len;
            }
            check_doc(what);
            dead_ok = 1;
            CHECK(ul_undo(), "%s: undo", what);
            {
                struct model after = cur_m;
                cur_m = before;
                check_doc("replace all undone");
                CHECK(ul_redo(), "%s: redo", what);
                cur_m = after;
                check_doc("replace all redone");
            }
            if (fails)
                return;
        }
    }
    /* the counts */
    decode();
    {
        unsigned long w = 0, c = 0, ns = 0, pp = 1;
        int in = 0;
        for (i = 0; i < cur_m.n; ++i) {
            unsigned char ch = cur_m.a[i].c;
            if (ch == CH_CR) { ++pp; in = 0; continue; }
            ++c;
            if (ch == ' ' || ch == CH_TAB) { in = 0; continue; }
            ++ns;
            if (!in) { ++w; in = 1; }
        }
        doc_count(0, DOC_LEN);
        CHECK(cn_words == w && cn_chars == c && cn_nonsp == ns && cn_paras == pp,
              "counts: %lu words %lu characters %lu %lu paragraphs; the model %lu %lu %lu %lu",
              cn_words, cn_chars, cn_nonsp, cn_paras, w, c, ns, pp);
    }
    printf("find: ok (%d characters, %ld matches found, %ld replaced)\n", cur_m.n, tot_found, tot_repl);
}

/*---------------------------------------------------------------
  3. the file
 ---------------------------------------------------------------*/
static unsigned char file[700000];
static unsigned long flen, fpos;
static cw_rfn ld_rd;                    /* the reader of a document being opened a step at a time */
static int ld_needs;

void FASTCALL ld_need(pos_t p)
{
    if (ld_safe(p))
        return;
    ++ld_needs;
    while (ld_kind)
        fmt_step(ld_rd, 0xFFFFFFFFUL);
}

static unsigned char wr(const void *p, unsigned int n)
{
    if (flen + n > sizeof file)
        return 1;
    memcpy(file + flen, p, n);
    flen += n;
    return 0;
}

static unsigned int rd(void *p, unsigned int n)
{
    if (n > flen - fpos)
        n = (unsigned int)(flen - fpos);
    memcpy(p, file + fpos, n);
    fpos += n;
    return n;
}

static void test_file(void)
{
    static struct model saved;
    unsigned char e;
    int i;
    /* the document of test 2, saved and loaded */
    decode();
    saved = cur_m;
    flen = 0;
    CHECK(fmt_save(wr, decpos[ndec / 2]) == 0, "save");
    CHECK(!memcmp(file, "CWRD\1\6", 6), "the header (1.6)");
    CHECK(!memcmp(file + 8, "INFO", 4) && file[22] == CS_L10, "INFO: the set ISO-8859-16 (1.3)");
    CHECK(file[23] == fmt_lang, "INFO: the language (1.5)");
    doc_new();
    fpos = 0;
    e = fmt_load(rd);
    CHECK(e == 0, "load: %d", e);
    cur_m = saved;
    check_doc("loaded");
    CHECK(fmt_cursor == decpos[ndec / 2], "the cursor kept");
    CHECK(!doc_modified, "not modified after loading");
    /* an unknown chunk is skipped (a later minor version) */
    {
        unsigned long at = 8;
        static unsigned char t[400000];
        memcpy(t, file, flen);
        memcpy(file + at, "XTRA\5\0\0\0hello", 13);
        memcpy(file + at + 13, t + at, flen - at);
        flen += 13;
        file[5] = 7;                    /* minor 7 */
        fpos = 0;
        CHECK(fmt_load(rd) == 0, "an unknown chunk skipped");
        check_doc("loaded past XTRA");
        file[4] = 2;                    /* major 2 */
        fpos = 0;
        CHECK(fmt_load(rd) == FE_NEWER, "a newer major version refused");
    }
    fpos = 0;
    memcpy(file, "GEOS", 4);
    CHECK(fmt_load(rd) == FE_NOTCW, "not a document");
    /* a damaged text: a broken code and control bytes dropped */
    doc_new();
    flen = 0;
    {
        static const unsigned char txt[] = { 'a', 1, 'b', K_CS, 0, 'x', 'c', K_CS, 0, K_CS, 'd', K_CS, 99, K_CS, 'e' };
        unsigned char sty1 = 1;
        wr("CWRD\1\0\0\0", 8);
        wr("FONT\13\0\0\0\1\11Gate Sans", 19);
        wr("STYL\5\0\0\0", 8);
        wr(&sty1, 1);
        wr("\0\14\0\0", 4);
        wr("TEXT\17\0\0\0", 8);
        wr(txt, sizeof txt);
        wr("END \0\0\0\0", 8);
        fpos = 0;
        CHECK(fmt_load(rd) == 0, "load a damaged text");
        decode();
        CHECK(ndec == 5 && dec[0].c == 'a' && dec[1].c == 'b' && dec[2].c == 'c' && dec[3].c == 'd' && dec[4].c == 'e',
              "the damaged text cleaned (%d characters)", ndec);
    }
    /* 1.0's file with a paragraph code but no PARA: the code dropped;
       1.1's PARA with entries longer than 8 bytes (a later minor
       version): their first 8 kept, the codes naming them kept */
    {
        static const unsigned char t10[] = { 'a', CH_CR, K_PS, 1, K_PS, 'b' };
        static const unsigned char t11[] = { K_PS, 2, K_PS, 'h', CH_CR, K_PS, 1, K_PS, 'q', CH_CR, 'x' };
        static const unsigned char pe[3][10] = {
            { PK_BODY, 0, 0, 0, 0, 0, 0, 2, 0xEE, 0xEE },
            { PK_QUOTE, PA_CENTRE, 40, 40, 0, 4, 4, 3, 0xEE, 0xEE },
            { PK_H1, PA_RIGHT, 0, 0, 0, 16, 6, 2, 0xEE, 0xEE } };
        unsigned char one = 1, three = 3;
        doc_new();
        flen = 0;
        wr("CWRD\1\0\0\0", 8);
        wr("FONT\13\0\0\0\1\11Gate Sans", 19);
        wr("STYL\5\0\0\0", 8);
        wr(&one, 1);
        wr("\0\14\0\0", 4);
        wr("TEXT\6\0\0\0", 8);
        wr(t10, sizeof t10);
        wr("END \0\0\0\0", 8);
        fpos = 0;
        CHECK(fmt_load(rd) == 0 && DOC_LEN == 3 && npar == 1, "1.0: a paragraph code without PARA dropped");
        flen = 0;
        wr("CWRD\1\5\0\0", 8);
        wr("FONT\13\0\0\0\1\11Gate Sans", 19);
        wr("STYL\5\0\0\0", 8);
        wr(&one, 1);
        wr("\0\14\0\0", 4);
        wr("PARA\37\0\0\0", 8);
        wr(&three, 1);
        wr(pe, sizeof pe);
        wr("TEXT\13\0\0\0", 8);
        wr(t11, sizeof t11);
        wr("END \0\0\0\0", 8);
        fpos = 0;
        CHECK(fmt_load(rd) == 0 && DOC_LEN == sizeof t11 && npar == 3, "1.5: PARA read (%lu bytes, %u styles)", DOC_LEN, npar);
        decode();
        CHECK(ndecp == 3 && decp[0].kind == PK_H1 && decp[0].align == PA_RIGHT && decp[1].kind == PK_QUOTE
              && decp[1].spacing == 3 && decp[2].kind == PK_BODY, "1.5: the paragraphs' styles");
        /* saved again: 1.1, PARA, read back the same */
        flen = 0;
        CHECK(fmt_save(wr, 0) == 0 && file[5] == CW_MINOR, "saved as 1.%d", CW_MINOR);
        fpos = 0;
        CHECK(fmt_load(rd) == 0 && DOC_LEN == sizeof t11 && npar == 3, "1.1 read back");
    }
    /* plain text: CR LF, LF, CR; UTF-8; a control byte */
    {
        static const char t[] = "one\r\ntwo\nthree\rfour \xc3\xa9t\xc3\xa9 \xe2\x82\xac \xe9!\x01";
        memcpy(file, t, sizeof t - 1);
        flen = sizeof t - 1;
        fpos = 0;
        CHECK(fmt_text(rd) == 0, "a text file");
        decode();
        {
            static const char want[] = "one\rtwo\rthree\rfour \xe9t\xe9 \xa4 \xe9!";
            CHECK(ndec == (int)strlen(want), "text: %d characters", ndec);
            for (i = 0; i < ndec && i < (int)strlen(want); ++i)
                if (dec[i].c != (unsigned char)want[i]) {
                    CHECK(0, "text: character %d is %02X, want %02X", i, dec[i].c, (unsigned char)want[i]);
                    break;
                }
        }
        CHECK(lt_n >= 4, "text: its lines (%u)", lt_n);
    }
    /* (W38) Polish: UTF-8 to the set, saved as 1.3 and read back; the
       same file as a 1.2 one (Latin-1): converted, the 40 bytes '?' */
    {
        static const char t[] = "Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84\n"
            "ZA\xc5\xbb\xc3\x93\xc5\x81\xc4\x86 G\xc4\x98\xc5\x9aL\xc4\x84 JA\xc5\xb9\xc5\x83 \xf1\xe9 \xc3\xb1 \xf0\x9f\x98\x80";
        /* ISO-8859-16 */
        static const char want[] = "Za\xbf\xf3\xb3\xe5 g\xfd\xf7l\xa2 ja\xae\xf1\r"
            "ZA\xaf\xd3\xa3\xc5 G\xdd\xd7L\xa1 JA\xac\xd1 ?\xe9 ? ?";
        static unsigned char keep[64];
        unsigned int n = (unsigned int)strlen(want), lost;
        memcpy(file, t, sizeof t - 1);
        flen = sizeof t - 1;
        fpos = 0;
        CHECK(fmt_text(rd) == 0 && fmt_cs == CS_L10, "Polish: a UTF-8 text file");
        lost = fmt_lost;
        CHECK(lost == 3, "Polish: 3 characters the set has not (%u)", lost);
        decode();
        CHECK(ndec == (int)n, "Polish: %d characters, want %u", ndec, n);
        for (i = 0; i < ndec && i < (int)n; ++i)
            if (dec[i].c != (unsigned char)want[i]) {
                CHECK(0, "Polish: character %d is %02X, want %02X", i, dec[i].c, (unsigned char)want[i]);
                break;
            }
        flen = 0;
        CHECK(fmt_save(wr, 0) == 0 && file[5] == CW_MINOR && file[22] == CS_L10, "Polish: saved as 1.6, ISO-8859-16");
        /* (1.5, W57) the language kept: saved, read back; a 1.4 file's: not said */
        fmt_lang = LANG_PL;
        flen = 0;
        CHECK(fmt_save(wr, 0) == 0 && file[23] == LANG_PL, "the language saved (1.5)");
        fmt_lang = 0;
        fpos = 0;
        CHECK(fmt_load(rd) == 0 && fmt_lang == LANG_PL, "the language read back: %u", fmt_lang);
        file[23] = 0;
        file[5] = 4;
        fpos = 0;
        CHECK(fmt_load(rd) == 0 && fmt_lang == 0, "a 1.4 file: no language");
        doc_new();
        fpos = 0;
        CHECK(fmt_load(rd) == 0 && fmt_cs == CS_L10 && !fmt_lost, "Polish: read back (set %u, %u lost)", fmt_cs, fmt_lost);
        decode();
        for (i = 0; i < ndec && i < (int)n; ++i)
            if (dec[i].c != (unsigned char)want[i]) {
                CHECK(0, "Polish read back: character %d is %02X, want %02X", i, dec[i].c, (unsigned char)want[i]);
                break;
            }
        CHECK(ndec == (int)n, "Polish read back: %d characters", ndec);
        /* the same file marked 1.2 (INFO's byte 6 0): Latin-1 */
        file[5] = 2;
        file[22] = CS_LATIN1;
        for (i = 0; i < (int)n; ++i)
            keep[i] = (unsigned char)want[i] >= 0xA0 && !cs_byte((unsigned char)want[i]) ? '?' : (unsigned char)want[i];
        fpos = 0;
        CHECK(fmt_load(rd) == 0 && fmt_cs == CS_LATIN1, "Polish: a 1.2 (Latin-1) file read");
        decode();
        CHECK(ndec == (int)n, "Latin-1: %d characters", ndec);
        for (i = 0; i < ndec && i < (int)n; ++i)
            if (dec[i].c != keep[i]) {
                CHECK(0, "Latin-1: character %d is %02X, want %02X", i, dec[i].c, keep[i]);
                break;
            }
        CHECK(fmt_lost == 16, "Latin-1: %u of its letters made '?', want 16", fmt_lost);
    }
    printf("file: ok\n");
}

/*---------------------------------------------------------------
  5. geoWrite documents (gwimport.c), built here from the format's
     description (docs/FORMAT.md, "geoWrite documents")
 ---------------------------------------------------------------*/
static unsigned char gs[8192];          /* a record being made */
static unsigned int gn;

static void g_b(unsigned char c) { gs[gn++] = c; }
static void g_w(unsigned int w) { g_b((unsigned char)w); g_b((unsigned char)(w >> 8)); }
static void g_s(const char *t) { while (*t) g_b((unsigned char)*t++); }

/* a ruler: margins, one tab stop (the others at the right margin), the
   first line's margin, the alignment and spacing byte */
static void g_ruler(unsigned int lm, unsigned int rm, unsigned int tab, unsigned int pm, unsigned char js)
{
    int i;
    g_b(0x11);
    g_w(lm);
    g_w(rm);
    g_w(tab);
    for (i = 1; i < 8; ++i)
        g_w(rm);
    g_w(pm);
    g_b(js);
    g_b(0);
    g_b(0);
    g_b(0);
}

static void g_font(unsigned int id, unsigned char size, unsigned char style)
{
    g_b(0x17);
    g_w(id << 6 | size);
    g_b(style);
}

/* a CVT file of class cls with records rec[0 .. n - 1] (len 0: none) */
static unsigned char grec[127][8192];
static unsigned int glen[127];

static void g_cvt(const char *cls, unsigned char vlir)
{
    unsigned int r, k, b;
    flen = 0;
    memset(file, 0, 762);
    file[0] = 0x83;
    memcpy(file + 3, "Test\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0", 16);
    file[21] = vlir;
    file[22] = 7;                       /* APPL_DATA */
    memcpy(file + 30, "PRG formatted GEOS file V1.0", 28);
    file[254] = 3;
    file[255] = 0x15;
    file[256] = 0xBF;
    memcpy(file + 329, cls, strlen(cls));
    memcpy(file + 369, "Some writer V2.1", 16);
    flen = 762;
    for (r = 0; r < 127; ++r) {
        if (!glen[r]) {
            file[508 + 2 * r] = 0;
            file[509 + 2 * r] = 0xFF;
            continue;
        }
        b = (glen[r] + 253) / 254;
        file[508 + 2 * r] = (unsigned char)b;
        file[509 + 2 * r] = (unsigned char)(glen[r] - (b - 1) * 254 + 1);
        memcpy(file + flen, grec[r], glen[r]);
        for (k = glen[r]; k < b * 254; ++k)
            file[flen + k] = 0xA5;      /* (padding: never read as text) */
        flen += b * 254;
    }
    /* the table ends after the last record */
    for (r = 127; r > 0 && !glen[r - 1]; --r)
        ;
    if (r < 127) {
        file[508 + 2 * r] = 0;
        file[509 + 2 * r] = 0;
    }
    fpos = 0;
}

static void g_put(int r)
{
    memcpy(grec[r], gs, gn);
    glen[r] = gn;
    gn = 0;
}

static unsigned int gw_rd(void *p, unsigned int n)
{
    return rd(p, n);
}

static void test_gw(void)
{
    static const char want[] = "Hello bold world\rCentred para\riuosup\r\tx\r\rSecond page\r[picture]\rafter\r"
                               "??? end of para continues\r";
    static unsigned char back[4096];
    unsigned int i, nb, npb;
    pos_t len;
    memset(glen, 0, sizeof glen);
    /* page 1: two rulers, fonts and styles, a page break after a paragraph */
    gn = 0;
    g_ruler(0, 479, 100, 0, 0);
    g_font(2, 12, 0);
    g_s("Hello ");
    g_font(2, 12, 0x40);
    g_s("bold");
    g_font(2, 12, 0);
    g_s(" world\r");
    g_ruler(40, 439, 439, 60, 0x01 | 0x08);  /* centred, double */
    g_s("Centred para\r");
    g_font(2, 12, 0x90);                /* italic, underline */
    g_s("iu");
    g_font(2, 12, 0x08);                /* outline: bold */
    g_s("o");
    g_font(2, 12, 0x04);                /* superscript: plain */
    g_s("sup\r\tx\r");
    g_b(0x0C);
    g_put(0);
    /* page 2: its ruler and font again (as geoWrite starts every page), a
       picture, characters geoWrite's text has not, a page full inside a
       paragraph */
    g_ruler(0, 479, 479, 0, 0);
    g_font(2, 12, 0);
    g_s("Second page");
    g_b(0x10);
    g_b(10);
    g_w(50);
    g_b(64);
    g_s("after\r");
    g_b(0x7F);
    g_b(0x05);
    g_b(0xE9);
    g_s(" end of");
    g_b(0x0C);
    g_put(1);
    /* page 3: Gate Mono; the text's end ($00) and bytes after it */
    g_ruler(0, 479, 479, 0, 0);
    g_font(301, 10, 0);
    g_s(" para continues\r");
    g_b(0);
    g_s("not text");
    g_put(2);
    g_ruler(0, 479, 479, 0, 0);
    g_s("A header\r");
    g_put(61);
    g_s("A footer\r");
    g_put(62);
    for (i = 0; i < 300; ++i)
        g_b((unsigned char)i);
    g_put(64);
    g_cvt("Write Image V2.1", 1);
    CHECK(gw_check(file) == 2, "geoWrite: a geoWrite document");
    gw_rf = gw_rd;
    CHECK(gw_load(0) == 0, "geoWrite: converted");
    CHECK(gw_pages == 3 && gw_pics == 1 && gw_hf == 3 && gw_bad == 3 && gw_tabs == 1 && gw_sty == 2,
          "geoWrite: counted (pages %u pictures %u header/footer %u bad %u tabs %u styles %u)", gw_pages, gw_pics,
          gw_hf, gw_bad, gw_tabs, gw_sty);
    dead_ok = 0;
    CHECK(decode(), "geoWrite: the text well formed (%s)", dec_why);
    dead_ok = 1;
    CHECK(ndec == (int)strlen(want), "geoWrite: %d characters, want %d", ndec, (int)strlen(want));
    for (i = 0; i < (unsigned int)ndec && i < strlen(want); ++i)
        if (dec[i].c != (unsigned char)want[i]) {
            CHECK(0, "geoWrite: character %u is %02X, want %02X", i, dec[i].c, (unsigned char)want[i]);
            break;
        }
    CHECK(!strcmp(fontname[dec[0].font], "California") && dec[0].size == 12 && dec[0].flags == 0, "geoWrite: the font");
    CHECK(dec[6].flags == SF_BOLD && dec[10].flags == 0, "geoWrite: bold");
    CHECK(dec[30].flags == (SF_ITALIC | SF_ULINE) && dec[32].flags == SF_BOLD && dec[33].flags == 0,
          "geoWrite: italic, underline, outline, superscript (%x %x %x)", dec[30].flags, dec[32].flags, dec[33].flags);
    i = ndec - 3;
    CHECK(!strcmp(fontname[dec[i].font], "Gate Mono") && dec[i].size == 10, "geoWrite: Gate Mono");
    CHECK(ndecp == 10, "geoWrite: %d paragraphs", ndecp);
    CHECK(decp[0].align == PA_LEFT && decp[0].left == 0 && decp[0].right == 0 && decp[0].spacing == 2,
          "geoWrite: a plain ruler is Body");
    CHECK(decp[1].align == PA_CENTRE && decp[1].left == 40 && decp[1].right == 40 && decp[1].first == 20
          && decp[1].spacing == 4, "geoWrite: the ruler (align %d left %d right %d first %d spacing %d)",
          decp[1].align, decp[1].left, decp[1].right, decp[1].first, decp[1].spacing);
    CHECK(decp[2].align == PA_CENTRE && decp[3].align == PA_CENTRE && decp[4].align == PA_LEFT
          && decp[5].align == PA_LEFT, "geoWrite: a ruler until the next");
    /* the page break: a paragraph of its own, [K_PB 0 K_PB] CR */
    len = DOC_LEN;
    cb_read(&tx, 0, back, (unsigned int)len);
    for (i = npb = 0; i + 3 < len; ++i)
        if (back[i] == K_PB) {
            ++npb;
            CHECK(back[i + 1] == 0 && back[i + 2] == K_PB && back[i + 3] == CH_CR && back[i - 1] == CH_CR,
                  "geoWrite: the page break's paragraph");
            i += 2;
        }
    CHECK(npb == 1, "geoWrite: one page break (%u)", npb);
    check_layout("geoWrite");
    CHECK(doc_modified, "geoWrite: modified (saved nowhere)");
    /* saved as a document (this version) and read back the same */
    nb = (unsigned int)len;
    flen = 0;
    CHECK(fmt_save(wr, 0) == 0 && file[5] == CW_MINOR, "geoWrite: saved as 1.%d", CW_MINOR);
    fpos = 0;
    CHECK(fmt_load(rd) == 0 && DOC_LEN == nb, "geoWrite: read back (%lu bytes)", DOC_LEN);
    {
        static unsigned char b2[4096];
        cb_read(&tx, 0, b2, nb);
        CHECK(!memcmp(b2, back, nb), "geoWrite: read back the same");
    }
    /* the page break taken out when a paragraph's end next to it goes
       (doc_tidy: a break is a paragraph of its own) */
    {
        pos_t q = 0;
        for (q = 0; q < DOC_LEN && doc_byte(q) != K_PB; ++q)
            ;
        ul_step(U_CMD, q + 3, q + 3);
        doc_delete(q + 3, q + 4);       /* (Delete on the break's line) */
        for (q = 0; q < DOC_LEN && doc_byte(q) != K_PB; ++q)
            ;
        CHECK(q == DOC_LEN, "geoWrite: the break gone with its paragraph's end");
        CHECK(ul_undo(), "geoWrite: undo");
        cb_read(&tx, 0, back + 2048, nb);
        CHECK(DOC_LEN == nb && !memcmp(back + 2048, back, nb), "geoWrite: the break back");
        check_layout("geoWrite, undone");
    }
    /* not geoWrite: another GEOS document, a sequential file, not GEOS */
    g_cvt("Paint Image V1.1", 1);
    CHECK(gw_check(file) == 1, "geoWrite: a GEOS file of another class");
    g_cvt("Write Image V2.1", 0);
    CHECK(gw_check(file) == 1, "geoWrite: not a record file");
    memset(file, 'x', 600);
    CHECK(gw_check(file) == 0, "geoWrite: not a GEOS file");
    /* a font geoWrite's descriptions do not name; a hanging first line */
    memset(glen, 0, sizeof glen);
    g_ruler(100, 300, 300, 20, 0x02);
    g_font(1023, 14, 0x20);             /* (reverse: plain) */
    g_s("z\r");
    g_put(0);
    g_cvt("Write Image V1.1", 1);
    CHECK(gw_load(0) == 0, "geoWrite: V1.1");
    decode();
    CHECK(ndec == 2 && !strcmp(fontname[dec[0].font], "GEOS font 1023") && dec[0].size == 14 && dec[0].flags == 0
          && gw_sty == 2, "geoWrite: font 1023 (%s)", fontname[dec[0].font]);
    CHECK(decp[0].left == 100 && decp[0].right == 179 && decp[0].first == -80 && decp[0].align == PA_RIGHT,
          "geoWrite: margins (%d %d %d)", decp[0].left, decp[0].right, decp[0].first);
    /* cut short */
    memset(glen, 0, sizeof glen);
    g_ruler(0, 479, 479, 0, 0);
    g_s("short");
    g_put(0);
    g_cvt("Write Image V2.0", 1);
    flen -= 300;
    CHECK(gw_load(0) == FE_IO, "geoWrite: a file cut short");
    /* a real geoWrite document, if one is named (CW_GW_SAMPLE=path) */
    {
        const char *sp = getenv("CW_GW_SAMPLE");
        FILE *f = sp ? fopen(sp, "rb") : 0;
        if (f) {
            unsigned int w = 0, inw = 0, k;
            char first[80];
            flen = fread(file, 1, sizeof file, f);
            fclose(f);
            fpos = 0;
            CHECK(gw_check(file) == 2 && gw_load(0) == 0, "the sample: converted");
            decode();
            for (k = 0; k < (unsigned int)ndec && k < sizeof first - 1 && dec[k].c != CH_CR; ++k)
                first[k] = (char)dec[k].c;
            first[k] = 0;
            for (k = 0; k < (unsigned int)ndec; ++k) {
                if (dec[k].c == ' ' || dec[k].c == CH_CR || dec[k].c == CH_TAB)
                    inw = 0;
                else if (!inw) {
                    inw = 1;
                    ++w;
                }
            }
            printf("the sample: %u pages, %d characters, %u words, first line \"%s\" (%s %u), left out: %s%s%s\n",
                   gw_pages, ndec, w, first, fontname[dec[0].font], dec[0].size,
                   gw_hf ? "header/footer " : "", gw_pics ? "pictures " : "", gw_bad ? "characters" : "nothing");
        } else
            printf("no CW_GW_SAMPLE: the real-document check skipped\n");
    }
    printf("geoWrite: ok\n");
}

/*---------------------------------------------------------------
  7. paging: the text and the line table on a card (a simulated
  swap: tests/host.c), with a cache of a few banks
 ---------------------------------------------------------------*/
static unsigned char hogs[256];
static int nhog;

/* the pool: keep banks free and no more runs from the system (the
   rest of the cache: what the text and the line table hold now) */
static void cache(unsigned int keep)
{
    host_freeze();
    while (bk_free_n() > keep)
        hogs[nhog++] = bk_get();
}

static void uncache(void)
{
    while (nhog)
        bk_put(hogs[--nhog]);
    host_bank_limit = 250;
}

static unsigned char pref[700000], pback[700000], pdoc[700000], pdoc2[400000];

static int same_text(const unsigned char *ref, pos_t n, const char *what)
{
    pos_t q;
    unsigned int k;
    CHECK(tx.total == n, "%s: %lu bytes, want %lu", what, tx.total, n);
    if (tx.total != n)
        return 0;
    for (q = 0; q < n; q += k) {
        k = cb_read(&tx, q, pback, n - q > 60000 ? 60000 : (unsigned int)(n - q));
        if (!k || memcmp(pback, ref + q, k)) {
            CHECK(0, "%s: the text differs near %lu", what, q);
            return 0;
        }
    }
    return 1;
}

static void chunks_ok(const char *what)
{
    unsigned int i;
    pos_t sum = 0;
    for (i = 0; i < tx.n; ++i) {
        CHECK(tx.len[i] <= BANK_SIZE, "%s: chunk %u length %u", what, i, tx.len[i]);
        sum += tx.len[i];
    }
    CHECK(sum == tx.total, "%s: chunks sum %lu, total %lu", what, sum, tx.total);
}

static void test_page(void)
{
    static unsigned char tmp[20000];
    unsigned long n = 0, at, k, i, r0, w0;
    unsigned int step, refused = 0;
    /* A. the text's cbuf against an array, 5 banks free (a cache of 40 KB
       for up to 600 KB): every read and edit pages */
    doc_new();
    uncache();
    cache(5);
    r0 = sw_reads;
    w0 = sw_writes;
    cb_clear(&tx);
    for (step = 0; step < 3000; ++step) {
        unsigned int op = rnd(10);
        if (op < 5 || n < 1000) {
            k = rnd(10) ? rnd(300) + 1 : rnd(20000) + 1;
            if (n + k > 600000)
                continue;
            at = rnd((unsigned int)(n + 1));
            for (i = 0; i < k; ++i)
                tmp[i] = (unsigned char)rnd(256);
            if (cb_insert(&tx, at, tmp, (unsigned int)k)) {
                ++refused;
                continue;
            }
            memmove(pref + at + k, pref + at, n - at);
            memcpy(pref + at, tmp, k);
            n += k;
        } else if (op < 8) {
            at = rnd((unsigned int)n);
            k = rnd(10) ? rnd(200) + 1 : rnd(30000) + 1;
            if (k > n - at)
                k = n - at;
            cb_delete(&tx, at, k);
            memmove(pref + at, pref + at + k, n - at - k);
            n -= k;
        } else if (op == 8) {
            at = rnd((unsigned int)n);
            k = rnd(3000) + 1;
            if (k > n - at)
                k = n - at;
            memset(tmp, (int)rnd(256), k);
            cb_write(&tx, at, tmp, (unsigned int)k);
            memcpy(pref + at, tmp, k);
        } else {
            at = rnd((unsigned int)n);
            CHECK(cb_byte(&tx, at) == pref[at], "paged: byte at %lu", at);
        }
        CHECK(tx.total == n, "paged: total %lu, want %lu", tx.total, n);
        if (step % 100 == 0 || step == 2999) {
            chunks_ok("paged");
            if (!same_text(pref, n, "paged"))
                break;
        }
    }
    CHECK(sw_reads - r0 > 100 && sw_writes - w0 > 100, "paged: read %lu, written %lu chunks", sw_reads - r0, sw_writes - w0);
    CHECK(refused < 20, "paged: %u insertions refused", refused);
    printf("paged cbuf: ok (%lu bytes, %u chunks, %lu chunks read, %lu written, %u refused)\n", n, tx.n,
           sw_reads - r0, sw_writes - w0, refused);
    /* B. appended from a document's file (cb_src): what is the file's as
       it is is never written to the swap, and is read back from the file */
    for (i = 0; i < 500000; ++i)
        pdoc[i] = (unsigned char)(i * 13 + (i >> 9));
    host_doc = pdoc;
    host_doc_len = 500000;
    cb_clear(&tx);
    w0 = sw_writes;
    r0 = sw_reads;
    for (at = 0, n = 0; at < 480000; at += k) {
        k = rnd(700) + 1;
        if (rnd(200)) {
            memcpy(tmp, pdoc + at, k);
            cb_src = at;
        } else {
            memset(tmp, 'x', k);        /* (bytes not the file's: a damaged code dropped) */
            cb_src = WH_NONE;
        }
        CHECK(!cb_append(&tx, tmp, (unsigned int)k, BANK_SIZE - 64), "append from the file");
        memcpy(pref + n, tmp, k);
        n += k;
    }
    k = sw_writes - w0;
    CHECK(k < 16, "append: %lu chunks written (only the ones with bytes not the file's)", k);
    same_text(pref, n, "appended");
    same_text(pref, n, "appended, again");
    CHECK(sw_reads - r0 > 100, "append: read back %lu", sw_reads - r0);
    printf("paged from a file: ok (%lu bytes, %lu chunks written, %lu read)\n", n, k, sw_reads - r0);
    cb_clear(&tx);
    uncache();
    doc_new();
}

/* the big document's session (test_big) again with 6 banks free: the
   same text and line table as with all the banks it wants */
static unsigned char big_t[600000], big_l[120000];
static pos_t big_n;
static unsigned int big_ln;

static void big_keep(void)
{
    big_n = tx.total;
    big_ln = lt_n;
    cb_read(&tx, 0, big_t, (unsigned int)(big_n > 65535 ? 65535 : big_n));
    if (big_n > 65535) {
        pos_t q;
        unsigned int k;
        for (q = 65535; q < big_n; q += k)
            k = cb_read(&tx, q, big_t + q, 60000);
    }
    cb_read(&lt, 0, big_l, (unsigned int)lt.total);
}


/*---------------------------------------------------------------
  4. a big document
 ---------------------------------------------------------------*/
static void test_big(unsigned int keep)
{
    static unsigned char para[600];
    unsigned int i, j, n;
    pos_t p, total;
    unsigned char b, it;
    unsigned long r0 = sw_reads, w0 = sw_writes;
    doc_new();
    if (keep)
        cache(keep);
    ul_reset();
    wrap_w = 520;
    b = sty_find(0, 12, SF_BOLD, 0);
    it = sty_find(0, 18, SF_ITALIC, 0);
    p = 0;
    for (i = 0; DOC_LEN < 200000; ++i) {
        n = 0;
        for (j = 0; j < 500; ++j)
            para[n++] = j % 7 == 6 ? ' ' : (unsigned char)('a' + (i + j) % 26);
        para[n++] = CH_CR;
        ul_step(U_CMD, p, p);
        p = doc_insert(p, para, n, i % 5 == 0 ? b : i % 7 == 0 ? it : 0);
        CHECK(!doc_err, "big: insert %u", i);
        if (doc_err)
            return;
    }
    total = DOC_LEN;
    CHECK(total >= 200000, "big: %lu bytes", total);
    check_layout("big");
    /* edits at both ends and the middle */
    ul_step(U_CMD, 0, 0);
    doc_insert(0, (const unsigned char *)"Start. ", 7, b);
    ul_step(U_CMD, DOC_LEN / 2, DOC_LEN / 2);
    doc_delete(pos_norm(DOC_LEN / 2), pos_next(pos_next(pos_norm(DOC_LEN / 2))));
    ul_step(U_CMD, DOC_LEN, DOC_LEN);
    doc_insert(DOC_LEN, (const unsigned char *)" The end.", 9, it);
    doc_format(pos_norm(1000), pos_norm(150000), FMT_SET, SF_ULINE);
    CHECK(!doc_err, "big: format 150 KB");
    check_layout("big, edited");
    CHECK(ul_undo() && ul_undo(), "big: undo twice");
    check_layout("big, undone");
    flen = 0;
    CHECK(fmt_save(wr, 0) == 0, "big: save (%lu bytes)", flen);
    n = lt_n;
    {
        static unsigned char a[260000];
        pos_t t = DOC_LEN, q;
        unsigned int k;
        for (q = 0; q < t; q += k)
            k = cb_read(&tx, q, a + q, 4096);
        doc_new();
        fpos = 0;
        memcpy(pdoc2, file, flen);      /* (the file the pager reads the text back from) */
        host_doc = pdoc2;
        host_doc_len = flen;
        w0 = sw_writes;
        CHECK(fmt_load(rd) == 0 && lt_n == n, "big: load (%u lines)", lt_n);
        CHECK(sw_writes - w0 <= lt.n + 2, "big: opened, %lu chunks written (the line table has %u)", sw_writes - w0, lt.n);
        CHECK(DOC_LEN == t, "big: loaded %lu bytes, saved %lu", DOC_LEN, t);
        for (q = 0; q < t; q += k) {
            static unsigned char b[4096];
            k = cb_read(&tx, q, b, 4096);
            if (memcmp(a + q, b, k)) {
                CHECK(0, "big: the text loaded differs near %lu", q);
                break;
            }
        }
    }
    if (!keep) {
        big_keep();
        printf("big: ok (%lu bytes, %u lines, %u banks)\n", DOC_LEN, lt_n, bk_owned);
        /* opened a step at a time: an edit well before the end of what is
           in needs nothing more; one near it reads the rest first */
        doc_new();
        fpos = 0;
        ld_rd = rd;
        ld_needs = 0;
        CHECK(fmt_begin(rd) == 0 && ld_kind == LD_CWRD, "steps: begin");
        CHECK(fmt_step(rd, 20000) == 0 && ld_kind == LD_CWRD && DOC_LEN >= 19900 && DOC_LEN <= 20000, "steps: 20 KB (%lu)", DOC_LEN);
        check_layout("steps: 20 KB");
        ul_step(U_CMD, 100, 100);
        doc_insert(pos_norm(100), (const unsigned char *)"x", 1, 0);
        CHECK(!ld_needs && ld_kind == LD_CWRD, "steps: an edit at the start read everything");
        CHECK(fmt_step(rd, 30000) == 0 && ld_kind == LD_CWRD, "steps: 30 KB more");
        p = DOC_LEN - 2;
        ul_step(U_CMD, p, p);
        CHECK(ld_needs == 1 && !ld_kind && DOC_LEN > 190000, "steps: an edit at the end of what is in (%d)", ld_needs);
        doc_insert(pos_norm(p), (const unsigned char *)"y", 1, 0);
        check_layout("steps: all");
        CHECK(ul_undo() && ul_undo(), "steps: undone");
        same_text(big_t, big_n, "steps");
        return;
    }
    /* with a cache of a few banks: the same as with all it wanted */
    same_text(big_t, big_n, "big, paged");
    CHECK(lt_n == big_ln, "big, paged: %u lines, want %u", lt_n, big_ln);
    {
        static unsigned char l2[60000];
        cb_read(&lt, 0, l2, (unsigned int)lt.total);
        CHECK(!memcmp(l2, big_l, lt.total), "big, paged: the line table differs");
    }
    for (i = 0; i < ul.n; ++i)
        CHECK(bk_inlo(ul.bank[i]), "big, paged: the undo log's bank %u not in the first runs", ul.bank[i]);
    for (i = 0; i < tx.n; ++i)
        CHECK(!tx.bank[i] || !bk_inlo(tx.bank[i]) || i < 8, "big, paged: (text in the first runs)");
    /* the undo log's banks to the card and back (put aside): undo still works */
    {
        static unsigned char ref[120000];
        pos_t rn = DOC_LEN;
        cb_read(&tx, 0, ref, 60000);
        ul_step(U_CMD, 50, 50);
        doc_insert(pos_norm(50), (const unsigned char *)"aside", 5, 0);
        sw_ulout();
        for (i = 0; i < ul.n; ++i)
            CHECK(!ul.bank[i], "undo log out: bank %u still there", i);
        sw_ulin();
        CHECK(ul.n && ul.bank[0], "undo log back");
        CHECK(ul_undo() && DOC_LEN == rn, "undo after the log came back");
        cb_read(&tx, 0, ref + 60000, 60000);
        CHECK(!memcmp(ref, ref + 60000, 60000), "undo after the log came back: the text");
    }
    printf("big, paged (%u banks free): ok (%lu chunks read, %lu written)\n", keep, sw_reads - r0, sw_writes - w0);
    /* C. saved: every chunk the new file's (sw_saved), then the old file
       gone: all of it read back from the new one */
    ul_step(U_CMD, DOC_LEN / 3, DOC_LEN / 3);
    doc_insert(pos_norm(DOC_LEN / 3), (const unsigned char *)"saved here", 10, 0);
    big_keep();
    flen = 0;
    CHECK(fmt_save(wr, 0) == 0, "big, paged: save");
    memcpy(pdoc, file, flen);
    host_doc = pdoc;
    host_doc_len = flen;
    sw_saved(fmt_toff);
    for (i = 0; i < tx.n; ++i)
        CHECK(!tx.bank[i] || !sw_bd[tx.bank[i]], "saved: chunk %u not clean", i);
    memset(pdoc2, 0xEE, sizeof pdoc2);
    w0 = sw_writes;
    same_text(big_t, big_n, "saved, the old file gone");
    same_text(big_t, big_n, "saved, the old file gone, again");
    CHECK(sw_writes == w0, "saved: nothing written to the swap (%lu)", sw_writes - w0);
    /* a save that fails half way: nothing changes */
    flen = sizeof file - 50000;
    CHECK(fmt_save(wr, 0) != 0, "a save that fails");
    same_text(big_t, big_n, "after a failed save");
    /* D. the card takes nothing more (full, or out): small edits go on
       while chunks with a copy on it make room; one that needs more
       banks than that is refused and changes nothing */
    {
        static unsigned char blk[60000];
        unsigned int done = 0;
        pos_t before, q;
        host_wfail = 1;
        host_fail_ok = 1;
        for (i = 0; i < 4; ++i) {
            q = pos_norm(rnd(20000));
            ul_step(U_CMD, q, q);
            doc_err = 0;
            doc_insert(q, (const unsigned char *)"full card typing ", 17, 0);
            done += !doc_err;
        }
        CHECK(done == 4, "card full: small edits refused");
        memset(blk, 'k', sizeof blk);
        before = DOC_LEN;
        big_keep();
        ul_step(U_CMD, DOC_LEN / 2, DOC_LEN / 2);
        doc_err = 0;
        doc_insert(pos_norm(DOC_LEN / 2), blk, sizeof blk, 0);
        CHECK(doc_err == DE_MEM && DOC_LEN == before, "card full: a big insertion not refused (%d)", doc_err);
        host_wfail = 0;
        host_fail_ok = 0;
        same_text(big_t, big_n, "card full: refused");
        check_layout("card full");
        printf("card full: ok (%u small edits done, a big one refused, %d times asked)\n", done, host_fails);
    }
    uncache();
}


/*---------------------------------------------------------------
  6. the pages (src/v/pages.c): opened a step at a time, edited here and
  there, against counting every line
 ---------------------------------------------------------------*/
unsigned int pg_arg, pg_pc, pg_lc, pg_tot, pg_line, pv_pg;
unsigned char pg_est, pg_buf[36 * LT_SZ];
#define VMAX_T 40
unsigned char pv_st[(VMAX_T + 7) / 8];
void e_pg_at(void);
unsigned char e_pg_idle(void);
void e_pg_first(void);
void e_pg_view(void);

/* a page break's paragraph before the paragraph at p (pgview.c's
   t_pbreak: a Return first when p is inside one); where the cursor goes */
static const unsigned char pb_bytes[4] = { K_PB, 0, K_PB, CH_CR };

static pos_t ins_pbreak(pos_t p)
{
    pos_t q = par_start(p);
    if (pos_norm(q) != p) {
        p = doc_insert(p, (const unsigned char *)"\r", 1, style_typing(p));
        q = par_start(p);
    }
    tx_ins(q, pb_bytes, 4);
    lay_fix();
    return pos_norm(q + 4);
}
static unsigned int bp[40000], bl[40000], btot;

static void brute(void)
{
    unsigned int i, h, acc = 0, page = 1, ln = 1;
    struct line l, m;
    for (i = 0; i < lt_n; ++i) {
        bp[i] = page;
        bl[i] = ln;
        lt_get(i, &l);
        h = acc + l.h;
        if (i + 1 < lt_n) {
            lt_get(i + 1, &m);
            if ((l.f & LF_PB) || (h && !(m.f & LF_PB) && h + m.h > pg_th)) {
                ++page;
                acc = 0;
                ln = 1;
                continue;
            }
        }
        acc = h;
        ++ln;
    }
    btot = page;
}

static void pg_check(const char *w, int exact)
{
    unsigned int k, ln;
    brute();
    for (k = 0; k < 40; ++k) {
        ln = rnd(lt_n);
        pg_arg = ln;
        e_pg_at();
        if (!(pg_est & 1))
            CHECK(pg_pc == bp[ln] && pg_lc == bl[ln], "pages, %s: line %u on page %u line %u, want %u, %u",
                  w, ln, pg_pc, pg_lc, bp[ln], bl[ln]);
    }
    if (exact)
        CHECK(!(pg_est & 2) && pg_tot == btot, "pages, %s: %u pages (a guess: %d), want %u", w, pg_tot, pg_est, btot);
    /* the view's lines (pg_view): which start a page, the first's page */
    for (k = 0; k < 6; ++k) {
        unsigned int j;
        ln = rnd(lt_n);
        pg_arg = ln;
        e_pg_view();
        CHECK(pv_pg == bp[ln], "pages, %s: the view from line %u on page %u, want %u", w, ln, pv_pg, bp[ln]);
        for (j = 0; j < VMAX_T && ln + j < lt_n; ++j)
            CHECK(!!(pv_st[j >> 3] & (1 << (j & 7))) == (ln + j && bl[ln + j] == 1),
                  "pages, %s: the view's line %u (%u): a page start %d, want %d", w, j, ln + j,
                  !!(pv_st[j >> 3] & (1 << (j & 7))), bl[ln + j] == 1);
    }
}

/* the idle walk to its end (a walk that never ends: a failure) */
static void pg_idle_all(const char *w)
{
    unsigned long n = 0;
    while (e_pg_idle())
        if (++n > 100000) {
            CHECK(0, "pages, %s: the walk does not end", w);
            break;
        }
}

static void test_pages(void)
{
    static unsigned char para[600];
    unsigned int i, j, n, k;
    unsigned char b, big;
    pos_t p = 0, q;
    doc_new();
    b = sty_find(0, 12, SF_BOLD, 0);
    big = sty_find(0, 27, 0, 0);
    for (i = 0; DOC_LEN < 300000; ++i) {
        n = 0;
        for (j = 0; j < 40 + (i * 37) % 400; ++j)
            para[n++] = j % 7 == 6 ? ' ' : (unsigned char)('a' + (i + j) % 26);
        para[n++] = CH_CR;
        ul_step(U_CMD, p, p);
        p = doc_insert(p, para, n, i % 5 == 0 ? b : i % 11 == 0 ? big : 0);
        if (i % 23 == 22)
            p = ins_pbreak(p);          /* (W45: page breaks here and there) */
    }
    flen = 0;
    fmt_save(wr, 0);
    memcpy(pdoc2, file, flen);
    host_doc = pdoc2;
    host_doc_len = flen;
    doc_new();
    fpos = 0;
    fmt_begin(rd);
    fmt_step(rd, 24576);
    pg_arg = 0;
    e_pg_at();
    while (ld_kind) {
        fmt_step(rd, 32768);
        pg_arg = 0;
        e_pg_at();
        pg_check("opening", 0);
    }
    pg_idle_all("idle");
    pg_check("opened", 1);
    for (k = 0; k < 300; ++k) {         /* edits here and there, the status after each */
        p = pos_norm(rnd((unsigned int)DOC_LEN));
        ul_step(U_CMD, p, p);
        switch (rnd(5)) {
        case 4: ins_pbreak(p); break;
        case 0: doc_insert(p, (const unsigned char *)"some words typed here ", 22, 0); break;
        case 1: doc_insert(p, (const unsigned char *)"\r", 1, 0); break;
        case 2:
            q = p + rnd(3000);
            q = pos_norm(q < DOC_LEN ? q : DOC_LEN);
            if (q > p)
                doc_delete(p, q);
            break;
        default: doc_insert(p, (const unsigned char *)"Big", 3, big); break;
        }
        lt_find(p);
        pg_arg = lf_i;
        e_pg_at();
        if (k % 7 == 0)
            pg_check("edited", 0);
        if (k % 50 == 0) {
            pg_idle_all("idle");
            pg_check("edited, counted", 1);
        }
    }
    pg_idle_all("idle");
    pg_check("at the end", 1);
    pg_arg = 10;
    e_pg_first();
    for (i = 0; i < lt_n && bp[i] < 10; ++i)
        ;
    CHECK(pg_line == i, "pages: page 10's first line %u, want %u", pg_line, i);
    printf("pages: ok (%u lines, %u pages)\n", lt_n, btot);
}

/* test_cw gw in.cvt out.cwr: a geoWrite document converted (the gate
   compares it with tools/gwimport.py's) */
static int gw_file(const char *in, const char *out)
{
    FILE *f = fopen(in, "rb");
    unsigned char e;
    if (!f)
        return 2;
    flen = fread(file, 1, sizeof file, f);
    fclose(f);
    fpos = 0;
    ul_init();
    gw_rf = gw_rd;
    if (gw_check(file) != 2 || (e = gw_load(0)) != 0)
        return 1;
    flen = 0;
    if (fmt_save(wr, 0) || !(f = fopen(out, "wb")))
        return 1;
    fwrite(file, 1, flen, f);
    fclose(f);
    return 0;
}

/* a converted geoWrite document larger than the cache: 58 pages of about
   7.5 KB (some 440 KB of text) with 6 banks free: its text, new, goes to
   the swap as it is made; the same text and line table as with all the
   banks it wants, saved and read back */
static void gw_big_make(void)
{
    unsigned int pg, i, w;
    memset(glen, 0, sizeof glen);
    for (pg = 0; pg < 58; ++pg) {
        gn = 0;
        g_ruler(pg % 3 ? 0 : 40, 479, 479, 0, pg % 4 == 1 ? 0x01 : 0);
        g_font(2, 12, 0);
        for (w = 0; gn < 7500; ++w) {
            if (w % 9 == 4)
                g_font(2, 12, (unsigned char)(w % 18 == 4 ? 0x40 : 0x80));
            for (i = 0; i < 3 + (w * 7 + pg) % 8; ++i)
                g_b((unsigned char)('a' + (w + i + pg) % 26));
            if (w % 9 == 4)
                g_font(2, 12, 0);
            g_b(w % 40 == 39 ? '\r' : ' ');
        }
        g_b('\r');
        if (pg % 10 == 9)
            g_b(0x0C);
        g_put(pg);
    }
    g_cvt("Write Image V2.1", 1);
}

static void test_gw_big(void)
{
    static unsigned char l1[200000];
    pos_t n;
    unsigned int ln;
    unsigned long w0 = sw_writes, r0 = sw_reads;
    gw_big_make();
    gw_rf = gw_rd;
    CHECK(gw_load(0) == 0, "geoWrite, big: converted with all the banks");
    big_keep();
    n = big_n;
    ln = lt_n;
    cb_read(&lt, 0, l1, (unsigned int)lt.total);
    CHECK(n > 400000, "geoWrite, big: %lu bytes", n);
    doc_new();
    cache(6);
    gw_big_make();
    CHECK(gw_load(0) == 0, "geoWrite, big: converted with 6 banks free");
    same_text(big_t, big_n, "geoWrite, big, paged");
    CHECK(lt_n == ln, "geoWrite, big, paged: %u lines, want %u", lt_n, ln);
    {
        static unsigned char l2[200000];
        cb_read(&lt, 0, l2, (unsigned int)lt.total);
        CHECK(!memcmp(l1, l2, lt.total), "geoWrite, big, paged: the line table differs");
    }
    check_layout("geoWrite, big, paged");
    CHECK(sw_writes - w0 > 40, "geoWrite, big, paged: only %lu chunks written", sw_writes - w0);
    /* saved as a document and opened again, still with 6 banks free */
    flen = 0;
    CHECK(fmt_save(wr, 0) == 0, "geoWrite, big: saved");
    memcpy(pdoc, file, flen);
    host_doc = pdoc;
    host_doc_len = flen;
    sw_saved(fmt_toff);
    doc_new();
    fpos = 0;
    CHECK(fmt_load(rd) == 0 && lt_n == ln, "geoWrite, big: opened again (%u lines)", lt_n);
    same_text(big_t, big_n, "geoWrite, big, opened again");
    uncache();
    printf("geoWrite, big: ok (%lu bytes, %u lines, %u pages; with 6 banks free %lu chunks written, %lu read)\n",
           n, ln, gw_pages, sw_writes - w0, sw_reads - r0);
}

/*---------------------------------------------------------------
  (W38) a 1.2 document's Latin-1, paged: the chunks the conversion
  changed are not the file's (the pager keeps them in the swap, never
  reads them from the file again); the others are read back from the
  file as they are; a 1.3 file's text is never converted
 ---------------------------------------------------------------*/
static void test_latin_paged(void)
{
    static unsigned char para[400], want[260000], got[4096];
    unsigned int i, j, n, k;
    pos_t q, t;
    unsigned long r0, w0;
    int v, bad;
    doc_new();
    ul_reset();
    wrap_w = 520;
    for (i = 0; DOC_LEN < 240000; ++i) {
        n = 0;
        for (j = 0; j < 300; ++j)       /* every 9th paragraph a few Latin-1 letters */
            para[n++] = i % 9 == 4 && j % 50 == 7 ? (j % 100 == 7 ? 0xF1 : 0xE9)   /* ñ (not in the set), é */
                        : j % 7 == 6 ? ' ' : (unsigned char)('a' + (i + j) % 26);
        para[n++] = CH_CR;
        doc_insert(DOC_LEN, para, n, 0);
        CHECK(!doc_err, "latin: insert %u", i);
        if (doc_err)
            return;
    }
    t = DOC_LEN;
    for (q = 0; q < t; q += k)
        k = cb_read(&tx, q, want + q, 4096);
    flen = 0;
    CHECK(fmt_save(wr, 0) == 0 && file[22] == CS_L10, "latin: saved");
    memcpy(pref, file, flen);           /* (the file as saved: 1.3) */
    for (v = 0; v < 2; ++v) {
        unsigned long lost = 0;
        memcpy(pdoc2, pref, flen);
        if (!v) {                       /* as a 1.2 file: Latin-1 */
            pdoc2[5] = 2;
            pdoc2[22] = CS_LATIN1;
        }
        host_doc = pdoc2;
        host_doc_len = flen;
        doc_new();
        cache(6);                       /* (a few banks: the text paged) */
        memcpy(file, pdoc2, flen);
        fpos = 0;
        r0 = sw_reads;
        w0 = sw_writes;
        CHECK(fmt_load(rd) == 0 && DOC_LEN == t, "latin %d: loaded (%lu bytes)", v, DOC_LEN);
        bad = 0;
        for (q = 0; q < t && !bad; q += k) {
            k = cb_read(&tx, q, got, 4096);
            for (j = 0; j < k; ++j) {
                unsigned char w = want[q + j];
                if (!v && w == 0xF1) {
                    w = '?';
                    ++lost;
                }
                if (got[j] != w) {
                    CHECK(0, "latin %d: byte %lu is %02X, want %02X", v, q + j, got[j], w);
                    bad = 1;
                    break;
                }
            }
        }
        for (q = 0; q < t && !bad; q += k) {     /* again: the chunks paged back in */
            k = cb_read(&tx, q, got, 4096);
            for (j = 0; j < k; ++j)
                if (got[j] != (!v && want[q + j] == 0xF1 ? '?' : want[q + j])) {
                    CHECK(0, "latin %d, again: byte %lu is %02X", v, q + j, got[j]);
                    bad = 1;
                    break;
                }
        }
        CHECK(!v ? fmt_lost == lost && lost > 0 && fmt_cs == CS_LATIN1 : !fmt_lost && fmt_cs == CS_L10,
              "latin %d: %u made '?' (want %lu)", v, fmt_lost, lost);
        CHECK(sw_reads - r0 > 20, "latin %d: paged (%lu chunks read, %lu written)", v, sw_reads - r0, sw_writes - w0);
        printf("latin %d: ok (%lu bytes paged: %lu chunks read, %lu written; %u made '?')\n", v, t,
               sw_reads - r0, sw_writes - w0, fmt_lost);
        cb_clear(&tx);
        uncache();
        doc_new();
    }
}


/*---------------------------------------------------------------
  7. stage 3: the page setup, page breaks, the header and footer, PDF
 ---------------------------------------------------------------*/
static void test_hf(void)
{
    hf_name = "Report";
    strcpy(hf_date, "23 November 2026");
    hf_make("Page {page} of {pages}", 3, 12);
    CHECK(!hf_part[0][0] && !strcmp(hf_part[1], "Page 3 of 12") && !hf_part[2][0],
          "hf: centred: [%s] [%s] [%s]", hf_part[0], hf_part[1], hf_part[2]);
    hf_make("{name}|{date}", 1, 1);
    CHECK(!strcmp(hf_part[0], "Report") && !hf_part[1][0] && !strcmp(hf_part[2], "23 November 2026"),
          "hf: left|right: [%s] [%s] [%s]", hf_part[0], hf_part[1], hf_part[2]);
    hf_make("a|{page}|c|d {x} {pag", 7, 9);
    CHECK(!strcmp(hf_part[0], "a") && !strcmp(hf_part[1], "7") && !strcmp(hf_part[2], "c|d {x} {pag"),
          "hf: three parts: [%s] [%s] [%s]", hf_part[0], hf_part[1], hf_part[2]);
    hf_make("{pages}{pages}{pages}{pages}{pages}{pages}{pages}{pages}{pages}{pages}{pages}{pages}"
            "{pages}{pages}{pages}{pages}{pages}{pages}{pages}{pages}{pages}{pages}", 1, 65535);
    CHECK(strlen(hf_part[1]) == HF_OUT - 1, "hf: a long line: %u bytes", (unsigned int)strlen(hf_part[1]));
    hf_make("", 1, 1);
    CHECK(!hf_part[0][0] && !hf_part[1][0] && !hf_part[2][0], "hf: an empty line");
    printf("header and footer fields: ok\n");
}

static unsigned char fl2[4000];
static unsigned long fl2n;

static void test_pagesetup(void)
{
    struct pgset a;
    unsigned char e, i;
    unsigned long k;
    /* the defaults: A4, 2.5 cm */
    doc_new();
    CHECK(pgs.paper == PP_A4 && pgs.w == 11906 && pgs.h == 16838 && pgs.left == 1417,
          "page: the default %u %u x %u, %u", pgs.paper, pgs.w, pgs.h, pgs.left);
    CHECK(wrap_w == 504 && pg_th == 778, "page: A4 2.5 cm: the text %u x %u, want 504 x 778", wrap_w, pg_th);
    /* Letter, 1 inch: 6.5 x 9 inches */
    pgs_paper(PP_LETTER);
    pgs.left = pgs.right = pgs.top = pgs.bottom = 1440;
    pg_apply();
    CHECK(wrap_w == 520 && pg_th == 720, "page: Letter 1 in: the text %u x %u, want 520 x 720", wrap_w, pg_th);
    /* nonsense mended: margins past the paper, a paper of nothing, too wide */
    pgs.left = 9000;
    pgs.right = 9000;
    pg_apply();
    CHECK(pgs.left == 1440 && pgs.right == 1440 && wrap_w == 520, "page: margins past the paper: %u %u", pgs.left, pgs.right);
    pgs.w = 100;
    pgs.paper = 7;
    pg_apply();
    CHECK(pgs.paper == PP_A4 && pgs.w == 11906, "page: no paper: %u %u", pgs.paper, pgs.w);
    pgs.left = pgs.right = 0;
    pg_apply();
    CHECK(wrap_w == TXW_MAX, "page: no margins: the text %u (the screen's %u at most)", wrap_w, TXW_MAX);
    memset(pgs.head, 'x', sizeof pgs.head);
    pg_apply();
    CHECK(strlen(pgs.head) == HF_MAX, "page: a header not ended: %u", (unsigned int)strlen(pgs.head));
    /* saved (1.4, PAGE before TEXT) and read back */
    doc_new();
    doc_insert(0, (const unsigned char *)"Page one\r", 9, 0);
    pgs_paper(PP_LETTER);
    pgs.top = 1800;
    pgs.bottom = 1080;
    pgs.left = 1296;
    pgs.right = 1584;
    pgs.flags = PS_NOFIRST;
    strcpy(pgs.head, "{name}|Draft");
    strcpy(pgs.foot, "Page {page} of {pages}");
    pg_apply();
    memcpy(&a, &pgs, PGS_N);
    flen = 0;
    e = fmt_save(wr, 0);
    CHECK(!e && file[4] == 1 && file[5] == CW_MINOR, "page: saved %u, version %u.%u", e, file[4], file[5]);
    for (k = 8, i = 0; k + 8 <= flen; k += 8 + (file[k + 4] | (unsigned long)file[k + 5] << 8)) {
        if (!memcmp(file + k, "PAGE", 4)) {
            CHECK(file[k + 4] == PGS_N && file[k + 5] == 0, "page: PAGE's length %u", file[k + 4]);
            CHECK(!memcmp(file + k + 8, &a, PGS_N), "page: PAGE's bytes");
            i |= 1;
        }
        if (!memcmp(file + k, "TEXT", 4)) {
            CHECK(i & 1, "page: PAGE after TEXT");
            i |= 2;
        }
    }
    CHECK(i == 3, "page: the chunks PAGE and TEXT: %u", i);
    memcpy(fl2, file, flen);
    fl2n = flen;
    doc_new();
    CHECK(pgs.paper == PP_A4 && wrap_w == 504, "page: New: the default again");
    fpos = 0;
    e = fmt_load(rd);
    CHECK(!e && !memcmp(&pgs, &a, PGS_N) && wrap_w == (12240 - 1296 - 1584) / 18 && pg_th == (15840 - 1800 - 1080) / 18,
          "page: read back: %u, the text %u x %u", e, wrap_w, pg_th);
    /* a 1.3 file (no PAGE): the default for new documents (the user's) */
    pgs_def.paper = PP_LETTER;
    pgs_def.w = 12240;
    pgs_def.h = 15840;
    pgs_def.left = pgs_def.right = pgs_def.top = pgs_def.bottom = 1440;
    for (k = 8; k + 8 <= flen; k += 8 + (file[k + 4] | (unsigned long)file[k + 5] << 8))
        if (!memcmp(file + k, "PAGE", 4)) {
            memcpy(file + k, "PAGX", 4);        /* (a chunk a 1.3 reader does not know) */
            break;
        }
    file[5] = 3;
    fpos = 0;
    e = fmt_load(rd);
    CHECK(!e && pgs.paper == PP_LETTER && pgs.left == 1440 && !pgs.head[0] && wrap_w == 520,
          "page: a 1.3 file: %u, paper %u, the text %u", e, pgs.paper, wrap_w);
    memcpy(&pgs_def, &a, 2);
    pgs_def.paper = PP_A4;                      /* (the built-in default again) */
    pgs_def.w = 11906;
    pgs_def.h = 16838;
    pgs_def.left = pgs_def.right = pgs_def.top = pgs_def.bottom = 1417;
    pgs_def.flags = 0;
    /* a later version's longer PAGE: its first 142 bytes */
    memcpy(file, fl2, fl2n);
    flen = fl2n;
    for (k = 8; k + 8 <= flen; k += 8 + (file[k + 4] | (unsigned long)file[k + 5] << 8))
        if (!memcmp(file + k, "PAGE", 4)) {
            memmove(file + k + 8 + PGS_N + 10, file + k + 8 + PGS_N, flen - (k + 8 + PGS_N));
            memset(file + k + 8 + PGS_N, 0x5A, 10);
            file[k + 4] = PGS_N + 10;
            flen += 10;
            break;
        }
    file[5] = 9;
    fpos = 0;
    e = fmt_load(rd);
    CHECK(!e && !memcmp(&pgs, &a, PGS_N), "page: a longer PAGE (1.9): %u", e);
    printf("page setup: ok (A4 %ux%u, Letter %ux%u; saved as 1.4 and read back; 1.3 and 1.9 files)\n",
           504, 778, 520, 720);
    doc_new();
}

/* the page breaks: their lines, the pages they start */
static void test_breaks(void)
{
    unsigned int i, n = 0;
    struct line l;
    pos_t c;
    doc_new();
    c = doc_insert(0, (const unsigned char *)"First page.\rSecond", 18, 0);
    c = ins_pbreak(c);                          /* (at "Second"'s end: a Return first) */
    c = doc_insert(c, (const unsigned char *)"Third page.", 11, 0);
    c = ins_pbreak(pos_norm(9));                /* (inside "First page.": split) */
    c = ins_pbreak(0);                          /* (at the start: a page break first) */
    for (i = 0; i < lt_n; ++i) {
        lt_get(i, &l);
        if (l.f & LF_PB) {
            ++n;
            CHECK(l.len == 4 && !l.w, "breaks: a break's line %u: %u bytes, width %u", i, l.len, l.w);
        }
    }
    CHECK(n == 3, "breaks: %u page breaks' lines, want 3", n);
    brute();
    pg_idle_all("breaks");
    pg_arg = 0;
    e_pg_at();
    CHECK(pg_tot == 4 && btot == 4, "breaks: %u pages (counted %u), want 4", pg_tot, btot);
    /* typing into a break's paragraph: it does nothing (no LF_PB) */
    doc_insert(0, (const unsigned char *)"x", 1, 0);
    lt_get(0, &l);
    CHECK(!(l.f & LF_PB), "breaks: typed into: still a break's line");
    brute();
    CHECK(btot == 3, "breaks: typed into: %u pages, want 3", btot);
    /* the file keeps them */
    flen = 0;
    fmt_save(wr, 0);
    fpos = 0;
    fmt_load(rd);
    for (i = 0, n = 0; i < lt_n; ++i) {
        lt_get(i, &l);
        n += (l.f & LF_PB) != 0;
    }
    CHECK(n == 2, "breaks: read back: %u breaks' lines, want 2", n);
    printf("page breaks: ok\n");
}

/* the PDF: written into a file (build/host/NAME.pdf) and kept for the checks */
static unsigned char *pdf_buf;
static unsigned long pdf_n, pdf_cap;
static unsigned int pdf_stop_at;
static int pdf_list;

unsigned char FASTCALL plat_pdf_out(unsigned int n)
{
    if (pdf_n + n > pdf_cap) {
        pdf_cap = (pdf_n + n) * 2;
        pdf_buf = realloc(pdf_buf, pdf_cap);
    }
    memcpy(pdf_buf + pdf_n, iobuf, n);
    pdf_n += n;
    return 0;
}

unsigned char plat_pdf_poll(void)
{
    return pdf_stop_at && pdf_page >= pdf_stop_at;
}

extern unsigned int pdf_first[8192];

/* the document as a PDF: its pages the pages' (pdf_first: each one's
   first line), the cross-reference table's offsets at their objects,
   startxref at the table; written to build/host/name.pdf */
static unsigned int pdf_check(const char *name)
{
    char path[80], *q;
    unsigned long x, k, o;
    unsigned int i, n, bad = 0;
    FILE *f;
    pg_idle_all("pdf");
    pg_arg = 1;
    e_pg_first();
    pdf_pages = pg_tot;
    pdf_title = name;
    hf_name = name;
    strcpy(hf_date, "23 November 2026");
    pdf_n = 0;
    CHECK(pdf_write() == 0, "pdf %s: not written", name);
    CHECK(pdf_n == pdf_size, "pdf %s: %lu bytes written, %lu counted", name, pdf_n, pdf_size);
    CHECK(pdf_page == pg_tot, "pdf %s: %u pages, the document %u", name, pdf_page, pg_tot);
    brute();
    for (i = 1, n = 1; i <= pdf_page && i < 8192; ++i) {
        while (n < lt_n && bp[n] < i)
            ++n;
        if (i == 1)
            n = 0;
        if (pdf_first[i] != n && bad++ < 3)
            CHECK(0, "pdf %s: page %u starts at line %u, the pages' %u", name, i, pdf_first[i], n);
    }
    /* the trailer: startxref, the table */
    pdf_buf[pdf_n] = 0;
    q = strstr((char *)pdf_buf + pdf_n - 40, "startxref\n");
    CHECK(q != 0, "pdf %s: no startxref", name);
    x = q ? strtoul(q + 10, 0, 10) : 0;
    CHECK(x < pdf_n && !memcmp(pdf_buf + x, "xref\n0 ", 7), "pdf %s: startxref %lu not at the table", name, x);
    n = (unsigned int)strtoul((char *)pdf_buf + x + 7, 0, 10);
    q = strchr((char *)pdf_buf + x + 7, '\n') + 1;
    for (i = 0, bad = 0; i < n; ++i, q += 20) {
        CHECK(q[18] == ' ' && q[19] == '\n', "pdf %s: xref entry %u not 20 bytes", name, i);
        if (!i)
            continue;
        o = strtoul(q, 0, 10);
        k = (unsigned long)sprintf(path, "%u 0 obj", i);
        if ((o + k > pdf_n || memcmp(pdf_buf + o, path, k)) && bad++ < 3)
            CHECK(0, "pdf %s: object %u's offset %lu is not its", name, i, o);
    }
    sprintf(path, "build/host/%s.pdf", name);
    f = fopen(path, "wb");
    if (f) {
        fwrite(pdf_buf, 1, pdf_n, f);
        fclose(f);
    }
    f = fopen("build/host/pdf.txt", pdf_list++ ? "a" : "w");   /* (tools/check_pdf.py's list) */
    if (f) {
        fprintf(f, "%s %u\n", name, pdf_page);
        fclose(f);
    }
    return pdf_page;
}

static void test_pdf(void)
{
    /* every style, every paragraph style, Polish, page breaks, a header
       and footer with fields */
    static const char *const words[] = {
        "Za\xBF\xF3\xB3\xE5 g\xFD\xF7l\xA2 ja\xAE\xF1", "plain", "bold", "italic", "underlined", "mono", "big",
        "(paren) back\\slash"
    };
    static unsigned char t[200];
    unsigned char st[8], pl, pc, pr, pj, pq, pb, pn, h1;
    unsigned int i, k, n, pages;
    pos_t c = 0;
    doc_new();
    st[0] = 0;                                  /* (each word in its style) */
    st[1] = 0;
    st[2] = sty_find(0, 12, SF_BOLD, 0);
    st[3] = sty_find(0, 12, SF_ITALIC, 0);
    st[4] = sty_find(0, 12, SF_ULINE, 0);
    st[5] = sty_find(font_index("Gate Mono"), 10, 0, 0);
    st[6] = sty_find(0, 18, SF_BOLD | SF_ITALIC, 0);
    st[7] = sty_find(0, 9, 0, 0);
    {
        struct pstyle q = named[PK_BODY];
        q.align = PA_CENTRE; pc = par_find(&q);
        q.align = PA_RIGHT; pr = par_find(&q);
        q.align = PA_JUSTIFY; pj = par_find(&q);
        pl = 0;
        pq = par_find(&named[PK_QUOTE]);
        pb = par_find(&named[PK_BULLET]);
        pn = par_find(&named[PK_NUMBER]);
        h1 = par_find(&named[PK_H1]);
    }
    for (i = 0; i < 60; ++i) {
        /* a paragraph of words in styles; its paragraph style */
        n = 0;
        for (k = 0; k < 12 + i % 17; ++k) {
            const char *w = words[(i + k) % 8];
            unsigned char sty_k = st[(i + k) % 8];
            n = (unsigned int)strlen(w);
            memcpy(t, w, n);
            t[n++] = ' ';
            c = doc_insert(c, t, n, sty_k);
        }
        c = doc_insert(c, (const unsigned char *)"\r", 1, 0);
        k = i % 8;
        tk_a = tk_b = c;
        if (k) {
            pos_t q0 = par_start(pos_prev(c));
            unsigned char ps = k == 1 ? pc : k == 2 ? pr : k == 3 ? pj : k == 4 ? pq : k == 5 ? pb : k == 6 ? pn : h1;
            if (k >= 4 && k != 7) {
                doc_named(q0, q0, k == 4 ? PK_QUOTE : k == 5 ? PK_BULLET : PK_NUMBER);
            } else if (k == 7)
                doc_named(q0, q0, PK_H1);
            else
                doc_para(q0, q0, PF_ALIGN, par[ps].align);
            c = pos_norm(DOC_LEN);
        }
        if (i % 13 == 12)
            c = ins_pbreak(c);
    }
    (void)pl;
    strcpy(pgs.head, "{name}|Za\xBF\xF3\xB3\xE5|{date}");
    strcpy(pgs.foot, "Page {page} of {pages}");
    pgs.flags = PS_NOFIRST;
    pages = pdf_check("styles");
    CHECK(pages >= 5, "pdf styles: %u pages", pages);
    /* Letter, other margins, no header and footer */
    pgs_paper(PP_LETTER);
    pgs.left = pgs.right = 1440;
    pgs.head[0] = pgs.foot[0] = 0;
    pg_apply();
    lay_all();
    pdf_check("letter");
    /* stopped after page 2 */
    pdf_stop_at = 2;
    pg_arg = 1;
    e_pg_first();
    pdf_pages = pg_tot;
    pdf_n = 0;
    CHECK(pdf_write() == PE_STOP && pdf_page == 2, "pdf: stopped at page %u", pdf_page);
    pdf_stop_at = 0;
    /* an empty document: one page */
    doc_new();
    CHECK(pdf_check("empty") == 1, "pdf: an empty document: not one page");
    printf("pdf: ok (%u pages, every style; a Letter one; stopped; an empty one)\n", pages);
}

/* the 300 KB document of test 6 with page breaks, a header and a footer:
   a PDF of a few hundred pages, as the pages have them */
static void test_pdf_big(void)
{
    static unsigned char para[600];
    unsigned int i, j, n;
    unsigned char b, it;
    pos_t p = 0;
    doc_new();
    b = sty_find(0, 12, SF_BOLD, 0);
    it = sty_find(0, 12, SF_ITALIC, 0);
    for (i = 0; DOC_LEN < 300000; ++i) {
        n = 0;
        for (j = 0; j < 40 + (i * 37) % 400; ++j)
            para[n++] = j % 7 == 6 ? ' ' : j % 31 == 5 ? 0xB3 : (unsigned char)('a' + (i + j) % 26);   /* (ł) */
        para[n++] = CH_CR;
        p = doc_insert(p, para, n, i % 5 == 0 ? b : i % 7 == 0 ? it : 0);
        if (i % 41 == 40)
            p = ins_pbreak(p);
    }
    ul_reset();
    strcpy(pgs.head, "{name}||Page {page}");
    strcpy(pgs.foot, "{page}/{pages}");
    n = pdf_check("big");
    printf("pdf, a big document: ok (%lu bytes of text, %u pages, %lu bytes of PDF)\n",
           (unsigned long)DOC_LEN, n, pdf_n);
    pgs.head[0] = pgs.foot[0] = 0;
}

#include "test_office.c"
#include "test_spell.c"
#include "test_table.c"
#include "test_ai.c"

int main(int argc, char **argv)
{
    if (argc > 3 && !strcmp(argv[1], "gw"))
        return gw_file(argv[2], argv[3]);
    if (argc > 3 && !strcmp(argv[1], "ox"))
        return ox_file(argv[2], argv[3]);
    if (argc > 3 && !strcmp(argv[1], "oz"))
        return oz_file(argv[2], argv[3]);
    if (argc > 1 && !strcmp(argv[1], "office")) {
        test_office();                  /* (stage 4's alone) */
        printf("%d checks, %d failures\n", checks, fails);
        return fails != 0;
    }
    if (argc > 1)
        rnd_s = strtoul(argv[1], 0, 10);
    sw_init();
    ul_init();
    doc_new();
    if (getenv("CW_SUGG")) {                /* (tools/script.py: the suggestions) */
        sugg_list();
        return 0;
    }
    if (getenv("CW_SPELL_ONLY")) {           /* (the spelling tests alone) */
        test_spell();
        printf("%d checks, %d failures\n", checks, fails);
        return fails != 0;
    }
    test_cbuf();
    sw_init();
    ul_init();
    doc_new();
    test_doc();
    if (!fails)
        test_find();
    if (!fails)
        test_file();
    if (!fails)
        test_gw();
    if (!fails)
        test_page();
    if (!fails)
        test_big(0);
    if (!fails)
        test_big(6);
    if (!fails)
        test_pages();
    if (!fails)
        test_gw_big();
    if (!fails)
        test_latin_paged();
    if (!fails)
        test_hf();
    if (!fails)
        test_pagesetup();
    if (!fails)
        test_breaks();
    if (!fails)
        test_pdf();
    if (!fails)
        test_pdf_big();
    if (!fails)
        test_office();
    if (!fails)
        test_spell();
    if (!fails)
        test_table();
    if (!fails)
        test_ai();
    printf("%d checks, %d failures\n", checks, fails);
    return fails != 0;
}
