/* Veranda Write's unit tests: stage 4 (W49-W54), .docx and .odt opened
   and exported (#included by test_main.c: its checks and helpers).
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include "../src/office.h"

static unsigned char *ox_buf;
static unsigned long ox_len, ox_pos;

static unsigned int ox_rd(void *p, unsigned int n)
{
    if (n > ox_len - ox_pos)
        n = (unsigned int)(ox_len - ox_pos);
    memcpy(p, ox_buf + ox_pos, n);
    ox_pos += n;
    return n;
}

static unsigned char ox_seek(unsigned long o)
{
    if (o > ox_len)
        return 1;
    ox_pos = o;
    return 0;
}

/* the document in memory (b, n) opened as an office file: 0 done, else FE_ */
static unsigned char ox_open(unsigned char *b, unsigned long n)
{
    ox_buf = b;
    ox_len = n;
    ox_pos = 0;
    oz_rd = ox_rd;
    oz_seek = ox_seek;
    oz_size = n;
    return ox_load(0);
}

static unsigned char *ox_slurp(const char *in, unsigned long *n)
{
    FILE *f = fopen(in, "rb");
    unsigned char *b;
    long k;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    k = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc(k + 1);
    *n = fread(b, 1, k, f);
    fclose(f);
    return b;
}

/* test_cw ox in.docx out.cwr: a file converted */
static int ox_file(const char *in, const char *out)
{
    unsigned long n;
    unsigned char *b = ox_slurp(in, &n), e;
    FILE *f;
    if (!b)
        return 2;
    sw_init();
    ul_init();
    e = ox_open(b, n);
    if (e) {
        printf("ox: %s: error %u\n", in, e);
        return 1;
    }
    fl_flush();
    lay_all();
    flen = 0;
    if (fmt_save(wr, 0) || !(f = fopen(out, "wb")))
        return 1;
    fwrite(file, 1, flen, f);
    fclose(f);
    printf("ox: %s: %lu bytes of text; lost %u, tables %u, pictures %u, boxes %u, notes %u, comments %u, changes %u, fields %u\n",
           in, (unsigned long)DOC_LEN, oxc.lost, oxc.tables, oxc.pics, oxc.boxes, oxc.notes, oxc.comments,
           oxc.changes, oxc.fields);
    free(b);
    return 0;
}

/* test_cw oz in.zip part: a part inflated, to stdout */
static int oz_file(const char *in, const char *part)
{
    unsigned long n;
    unsigned char *b = ox_slurp(in, &n);
    static unsigned char out[256];
    unsigned int k;
    if (!b)
        return 2;
    ox_buf = b;
    ox_len = n;
    ox_pos = 0;
    oz_rd = ox_rd;
    oz_seek = ox_seek;
    oz_size = n;
    if (oz_open(0))
        return 3;
    if (getenv("OZ_PRE")) {
        strcpy((char *)iobuf + 352, getenv("OZ_PRE"));
        if (oz_part((char *)iobuf + 352))
            return 4;
        while ((k = oz_read(out, sizeof out)) > 0)
            ;
    }
    strcpy((char *)iobuf + 352, part);
    if (oz_part((char *)iobuf + 352))
        return 4;
    if (getenv("OZ_TOKENS")) {
        unsigned char t;
        static const char *const tn[] = { "EOF", "START", "ATTR", "OPEN", "TEXT", "END", "ERR" };
        xm_init();
        do {
            t = xm_next();
            printf("%s %s = [%.*s]\n", tn[t], XNAME, xm_vlen, XVAL);
        } while (t != XT_EOF && t != XT_ERR);
        return 0;
    }
    while ((k = oz_read(out, sizeof out)) > 0)
        fwrite(out, 1, k, stdout);
    oz_close();
    return oz_err ? 5 : 0;
}

/*---------------------------------------------------------------
  export: CWRD -> .docx / .odt -> CWRD (W52, W53)
 ---------------------------------------------------------------*/
static unsigned char *oe_buf;
static unsigned long oe_n, oe_cap;

unsigned char FASTCALL plat_oe_out(const unsigned char *p, unsigned int n)
{
    if (oe_n + n > oe_cap) {
        oe_cap = (oe_n + n) * 2;
        oe_buf = realloc(oe_buf, oe_cap);
    }
    memcpy(oe_buf + oe_n, p, n);
    oe_n += n;
    return 0;
}

/* the document as a list: each paragraph's style (its 8 bytes, "PB" a
   page break's), then its characters and their styles (the character,
   the font's name, the size, the flags), one line each */
static char *ox_dump(unsigned long *len)
{
    static char line[200];
    unsigned long n = 0, cap = 1 << 16;
    char *o = malloc(cap);
    pos_t p;
    unsigned char c, s = 0, ps = 0, pb = 0, newp = 1, k;
    const struct pstyle *q;
    for (p = 0; p <= DOC_LEN; ++p) {
        c = p < DOC_LEN ? doc_byte(p) : CH_CR;
        if (IS_KIND(c) && p + 2 < DOC_LEN) {
            k = doc_byte(p + 1);
            if (c == K_CS)
                s = k;
            else if (c == K_PS)
                ps = k;
            else if (c == K_PB)
                pb = 1;
            p += 2;
            continue;
        }
        if (newp || c == CH_CR) {
            if (c == CH_CR) {
                q = &par[ps];
                sprintf(line, pb ? "PB\n" : "P %u %u %u %u %d %u %u %u\n", q->kind, q->align, q->left, q->right,
                        q->first, q->before, q->after, q->spacing);
                ps = pb = 0;
                newp = 0;
            } else
                line[0] = 0;
            if (c == CH_CR) {
                if (n + strlen(line) + 1 > cap)
                    o = realloc(o, cap *= 2);
                strcpy(o + n, line);
                n += strlen(line);
                newp = 1;
                continue;
            }
            newp = 0;
        }
        sprintf(line, " %02X %s %u %u\n", c, fontname[sty[s].font], sty[s].size, sty[s].flags & SF_KNOWN);
        if (n + strlen(line) + 1 > cap)
            o = realloc(o, cap *= 2);
        strcpy(o + n, line);
        n += strlen(line);
    }
    *len = n;
    return o;
}

/* the paragraphs' listing goes before the characters' (each paragraph's
   style line written at its CR, after them: the same in both) */
static void ox_test_doc(void)
{
    static const char *const words[] = {
        "Za\xBF\xF3\xB3\xE6 g\xEA\xF7l\xA2 ja\xAE\xF1", "plain", "bold", "italic", "underlined", "mono", "big",
        "<amp & \"quote\">", "  two spaces", "tab\there"
    };
    static unsigned char t[200];
    unsigned char st[10], fm;
    unsigned int i, k, n;
    pos_t c = 0, q0;
    doc_new();
    fm = font_index("Gate Mono");
    st[0] = 0;
    st[1] = 0;
    st[2] = sty_find(0, 12, SF_BOLD, 0);
    st[3] = sty_find(0, 12, SF_ITALIC, 0);
    st[4] = sty_find(0, 12, SF_ULINE, 0);
    st[5] = sty_find(fm, 10, 0, 0);
    st[6] = sty_find(0, 18, SF_BOLD | SF_ITALIC | SF_ULINE, 0);
    st[7] = sty_find(0, 9, 0, 0);
    st[8] = sty_find(fm, 12, SF_BOLD, 0);
    st[9] = sty_find(0, 27, SF_ITALIC, 0);
    for (i = 0; i < 40; ++i) {
        n = 0;
        for (k = 0; k < 3 + i % 9; ++k) {
            const char *w = words[(i + k) % 10];
            n = (unsigned int)strlen(w);
            memcpy(t, w, n);
            t[n++] = ' ';
            c = doc_insert(c, t, n, st[(i * 3 + k) % 10]);
        }
        c = doc_insert(c, (const unsigned char *)"\r", 1, 0);
        q0 = par_start(pos_prev(c));
        tk_a = tk_b = c;
        switch (i % 12) {
        case 1: doc_named(q0, q0, PK_H1); break;
        case 2: doc_named(q0, q0, PK_H2); break;
        case 3: doc_named(q0, q0, PK_H3); break;
        case 4: doc_named(q0, q0, PK_QUOTE); break;
        case 5: case 6: doc_named(q0, q0, PK_BULLET); break;
        case 7: case 8: doc_named(q0, q0, PK_NUMBER); break;
        case 9: doc_para(q0, q0, PF_ALIGN, PA_CENTRE); doc_para(q0, q0, PF_SPACING, 3); break;
        case 10: doc_para(q0, q0, PF_ALIGN, PA_RIGHT); doc_para(q0, q0, PF_INDENT, 40); doc_para(q0, q0, PF_AFTER, 12); break;
        case 11: doc_para(q0, q0, PF_ALIGN, PA_JUSTIFY); doc_para(q0, q0, PF_SPACING, 4); break;
        }
        c = pos_norm(DOC_LEN);
        if (i % 13 == 12)
            c = ins_pbreak(c);
        if (i == 30) {
            c = ins_pbreak(c);          /* (two in a row) */
            c = ins_pbreak(c);
        }
    }
    c = doc_insert(c, (const unsigned char *)"The end.", 8, st[2]);
    ul_reset();
    pgs_paper(PP_LETTER);
    pgs.left = 1800;
    pgs.right = 1700;
    pgs.top = 1500;
    pgs.bottom = 1300;
    strcpy(pgs.head, "{name}|Za\xBF\xF3\xB3\xE6 & co|{date}");
    strcpy(pgs.foot, "Page {page} of {pages}");
    pgs.flags = PS_NOFIRST;
    pg_apply();
    lay_all();
}

static void FASTCALL ox_rt(unsigned char kind, const char *name)
{
    static char path[80];
    unsigned long n1, n2, i;
    char *a, *b;
    struct pgset keep;
    unsigned char e;
    FILE *f;
    a = ox_dump(&n1);
    memcpy(&keep, &pgs, sizeof keep);
    oe_n = 0;
    strcpy((char *)iobuf, "23 November 2026");  /* ({date}, {name}: the caller's, in iobuf) */
    strcpy((char *)iobuf + 32, name);
    e = oe_write(kind);
    CHECK(e == 0, "export %s: error %u", name, e);
    CHECK(oe_size == oe_n, "export %s: %lu bytes said, %lu written", name, oe_size, oe_n);
    sprintf(path, "build/host/%s", name);
    f = fopen(path, "wb");
    if (f) {
        fwrite(oe_buf, 1, oe_n, f);
        fclose(f);
    }
    e = ox_open(oe_buf, oe_n);
    CHECK(e == 0, "export %s: not opened again: %u", name, e);
    b = ox_dump(&n2);
    if (kind == OK_ODT) {
        /* (an .odt breaks a page before a paragraph: two breaks in a
           row have an empty paragraph between them) */
        char *q;
        static const char e2[] = "PB\nP 0 0 0 0 0 0 0 2\nPB\n";
        while ((q = strstr(b, e2)) != 0) {
            memmove(q + 3, q + 3 + 18, b + n2 - (q + 3 + 18) + 1);
            n2 -= 18;
        }
    }
    for (i = 0; i < n1 && i < n2 && a[i] == b[i]; ++i)
        ;
    if (!(n1 == n2 && i == n1)) {
        sprintf(path, "build/host/%s.a", name);
        f = fopen(path, "wb");
        fwrite(a, 1, n1, f);
        fclose(f);
        sprintf(path, "build/host/%s.b", name);
        f = fopen(path, "wb");
        fwrite(b, 1, n2, f);
        fclose(f);
    }
    CHECK(n1 == n2 && i == n1, "export %s: read back not the same at byte %lu of %lu/%lu (build/host/%s.a, .b)", name,
          i, n1, n2, name);
    CHECK(!memcmp(&keep, &pgs, sizeof keep), "export %s: the page setup not the same: %u %u %u %u %u %u [%s] [%s] %u",
          name, pgs.w, pgs.h, pgs.top, pgs.bottom, pgs.left, pgs.right, pgs.head, pgs.foot, pgs.flags);
    free(a);
    free(b);
}

/* a document of 300 KB (test_pdf_big's kind: paragraphs in styles, Polish,
   page breaks) exported both ways, opened again with 10 banks free: the
   window's 4, the styles' 1, the rest the text's (it pages) */
static void FASTCALL ox_big(unsigned char kind, const char *name)
{
    static unsigned char para[600];
    unsigned int i, j, n;
    unsigned char b, it;
    unsigned long n1, n2, w0;
    char *a, *d;
    pos_t p = 0;
    unsigned char e;
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
    a = ox_dump(&n1);
    oe_n = 0;
    strcpy((char *)iobuf, "23 November 2026");
    strcpy((char *)iobuf + 32, name);
    e = oe_write(kind);
    CHECK(e == 0, "export %s: error %u", name, e);
    doc_new();
    w0 = sw_writes;
    cache(10);
    e = ox_open(oe_buf, oe_n);
    CHECK(e == 0, "export %s: not opened again with 10 banks free: %u", name, e);
    d = ox_dump(&n2);
    CHECK(n1 == n2 && !memcmp(a, d, n1), "export %s: read back with 10 banks free: not the same", name);
    CHECK(sw_writes - w0 > 10, "export %s: read back with 10 banks free: only %lu chunks written", name,
          sw_writes - w0);
    check_layout(name);
    uncache();
    printf("office, big: %s %lu bytes of text, %lu bytes of %s; read back with 10 banks free: the same (%lu chunks written)\n",
           name, (unsigned long)DOC_LEN, oe_n, kind == OK_DOCX ? ".docx" : ".odt", sw_writes - w0);
    free(a);
    free(d);
}

static void test_office(void)
{
    sw_init();
    ul_init();
    ox_test_doc();
    ox_rt(OK_DOCX, "rt.docx");
    ox_test_doc();
    ox_rt(OK_ODT, "rt.odt");
    /* an empty document; one of a page break only */
    doc_new();
    ox_rt(OK_DOCX, "empty.docx");
    doc_new();
    ox_rt(OK_ODT, "empty.odt");
    printf("office: ok (round trips: .docx, .odt; empty ones)\n");
    ox_big(OK_DOCX, "big.docx");
    ox_big(OK_ODT, "big.odt");
}
