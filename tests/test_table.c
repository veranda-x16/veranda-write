/* Veranda Write's unit tests: simple tables (CWRD 1.6, DECISIONS
   W62-W66; included by test_main.c).

   A table made, typed into, its rows and columns added and deleted, its
   widths and borders changed, Return in its cells, deletions that would
   break it refused; after each the line table against a fresh layout,
   every row's structure (its first paragraph's codes, its cells), a
   row's height its tallest cell's, undo back to the bytes before and redo
   to the bytes after; random edits inside cells; the file (1.6) saved and
   read back.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */

#include "../src/tbhead.h"

static unsigned char tt_a[200000], tt_b[200000];
static pos_t tt_an, tt_bn;

/* the table codes in order: each one's paragraph (its number, a byte),
   kind, argument */
static void tc_dump(unsigned char *b, pos_t *n)
{
    pos_t p;
    unsigned int np = 0;
    unsigned char c;
    *n = 0;
    for (p = 0; p < DOC_LEN; ++p) {
        c = doc_byte(p);
        if (c == CH_CR)
            ++np;
        else if (IS_KIND(c)) {
            if (IS_TKIND(c)) {
                b[(*n)++] = (unsigned char)np;
                b[(*n)++] = c;
                b[(*n)++] = doc_byte(p + 1);
            }
            p += 2;
        }
    }
}

static pos_t snapshot(unsigned char *b)
{
    pos_t n = DOC_LEN;
    if (n > 200000)
        n = 200000;
    cb_read(&tx, 0, b, (unsigned int)n);
    return n;
}

static pos_t tins(pos_t p, const char *s)
{
    return doc_insert(p, (const unsigned char *)s, (unsigned int)strlen(s), style_typing(p));
}

/* every row: its first paragraph a row's, its cells' first paragraphs
   cells', the rest further paragraphs; at most TB_MAXC cells; its line
   as tall as its tallest cell (tb_lay), its sub-lines its bytes whole */
static int tb_rows(const char *what)
{
    unsigned int i, j, n = 0;
    unsigned char k;
    unsigned int y, ch[TB_MAXC];
    struct line l;
    pos_t p;
    unsigned int off;
    for (i = 0; i < lt_n; ++i) {
        lt_get(i, &l);
        if (!IS_TR(&l))
            continue;
        ++n;
        p = lt_start(i);
        CHECK(tb_row(p), "%s: row %u not found", what, i);
        CHECK(tr_lo == p && tr_hi == p + l.len, "%s: row %u's bounds", what, i);
        CHECK(tr_n >= 1 && tr_n <= TB_MAXC, "%s: row %u: %u cells", what, i, tr_n);
        tb_lay(p, l.s);
        CHECK(tc_end == p + l.len, "%s: row %u laid out to %lu, its line %u bytes", what, i,
              (unsigned long)(tc_end - p), l.len);
        CHECK(tc_n == tr_n, "%s: row %u: %u cells laid out, %u found", what, i, tc_n, tr_n);
        CHECK(tc_h == l.h, "%s: row %u: %u high, its line %u", what, i, tc_h, l.h);
        memset(ch, 0, sizeof ch);
        off = 0;
        for (j = 0; j < sl_n; ++j) {
            CHECK(tsl[j].off == off, "%s: row %u sub-line %u at %u, want %u", what, i, j, tsl[j].off, off);
            off += tsl[j].l.len;
            k = tsl[j].c;
            CHECK(k < tc_n && tsl[j].y == (TB_PADT + ch[k] > 255 ? 255 : TB_PADT + ch[k]), "%s: row %u sub-line %u: cell %u y %u", what, i, j, k, tsl[j].y);
            ch[k] += tsl[j].l.h;
            CHECK(p + tsl[j].off >= tr_c[k] && p + tsl[j].off < tr_c[k + 1], "%s: row %u sub-line %u outside its cell", what, i, j);
        }
        if (sl_n < TB_SL)
            CHECK(off == l.len, "%s: row %u: its sub-lines %u bytes, it %u", what, i, off, l.len);
        for (k = y = 0; k < tc_n; ++k)
            if (ch[k] > y)
                y = ch[k];
        if (sl_n < TB_SL && TB_PADT + y + TB_PADB + tc_last < 255)
            CHECK(l.h == TB_PADT + y + TB_PADB + tc_last, "%s: row %u: %u high, its cells %u (last %u)", what, i,
                  l.h, y, tc_last);
        for (k = 0; k + 1 <= tc_n; ++k)
            CHECK(tc_x[k] <= tc_x[k + 1] && tc_x[k + 1] < wrap_w, "%s: row %u: borders", what, i);
    }
    return (int)n;
}

static void tb_ok_all(const char *what)
{
    check_layout(what);
    tb_rows(what);
}

/* an edit as one undo step: undone, the bytes before; redone, after */
static void tb_undo_check(const char *what)
{
    pos_t n;
    tt_bn = snapshot(tt_b);
    CHECK(ul_undo(), "%s: undo", what);
    lay_fix();
    n = snapshot(tt_a + 100000);
    CHECK(n == tt_an && !memcmp(tt_a, tt_a + 100000, n), "%s: undone: not the bytes before", what);
    tb_ok_all(what);
    CHECK(ul_redo(), "%s: redo", what);
    lay_fix();
    n = snapshot(tt_a + 100000);
    CHECK(n == tt_bn && !memcmp(tt_b, tt_a + 100000, n), "%s: redone: not the bytes after", what);
    tb_ok_all(what);
}

static void step_begin(pos_t p)
{
    tt_an = snapshot(tt_a);
    ul_step(U_CMD, p, p);
}

static unsigned int nrows(void)
{
    unsigned int i, n = 0;
    struct line l;
    for (i = 0; i < lt_n; ++i) {
        lt_get(i, &l);
        n += IS_TR(&l);
    }
    return n;
}

static void test_table(void)
{
    pos_t c, p;
    unsigned int i, h1, step;
    unsigned char k, e, w[3];
    struct line l;
    char what[64];
    doc_new();
    ul_reset();
    wrap_w = 504;
    c = tins(0, "Before the table.\rAfter it.");
    /* a table of 3 x 3 before "After it." */
    p = pos_norm(18);
    step_begin(p);
    e = tb_insert(p, 3, 3);
    CHECK(!e, "insert a table: %d", e);
    tb_ok_all("a 3 x 3 table");
    CHECK(nrows() == 3, "3 rows: %u", nrows());
    CHECK(tb_row(tb_at) && tr_n == 3 && tr_w[0] == 86 && tr_w[1] == 85 && tr_w[2] == 85,
          "its cells: %u, widths %u %u %u", tr_n, tr_w[0], tr_w[1], tr_w[2]);
    CHECK(tb_at == 18 + 6, "the cursor in its first cell: %lu", (unsigned long)tb_at);
    tb_undo_check("insert a table");
    /* typed into: the second cell three lines, the row as tall */
    c = tins(tb_at, "One");
    CHECK(tb_next(c, 1), "Tab: the next cell");
    c = tins(tb_at, "A longer text that wraps in its narrow cell onto more lines than one.");
    tb_ok_all("typed into two cells");
    lt_find(c);
    lt_get(lf_i, &l);
    CHECK(IS_TR(&l), "the cursor's line a row");
    tb_lay(lf_pos, l.s);
    for (i = h1 = 0; i < sl_n; ++i)
        if (tsl[i].c == 1)
            ++h1;
    CHECK(h1 >= 3, "the second cell: %u lines", h1);
    lt_get(0, &l);
    CHECK(tc_h == TB_PADT + h1 * l.h + TB_PADB, "the row %u high (%u lines of %u)", tc_h, h1, l.h);
    /* Tab to the third, the next row's first, back */
    CHECK(tb_next(c, 1) && tb_row(tb_at) && tb_cell(tb_at) == 2, "Tab: the third cell");
    c = tins(tb_at, "Three");
    tb_row(c);
    p = tr_lo;
    CHECK(tb_next(c, 1) && tb_row(tb_at) && tb_cell(tb_at) == 0 && tr_lo > p, "Tab: the next row");
    CHECK(tb_next(tb_at, 0) && tb_row(tb_at) && tb_cell(tb_at) == 2, "Shift+Tab: back");
    /* Return in a cell: a further paragraph of it */
    tb_row(c);
    step_begin(c);
    e = tb_return(c - 2, style_typing(c - 2));
    CHECK(!e && tb_row(tb_at) && tr_n == 3 && tb_cell(tb_at) == 2, "Return in a cell: %d, %u cells", e, tr_n);
    tb_head(par_start(tb_at));
    CHECK(th_k == K_TPARA, "Return: a further paragraph (%02X)", th_k);
    tb_ok_all("Return in a cell");
    tb_undo_check("Return in a cell");
    c = tb_at;
    /* par_start in a cell: its own paragraph; alignment for it alone */
    tb_row(c);
    CHECK(par_start(c + 1) == par_start(c) && par_start(c) > tr_c[2] && par_start(c) < tr_hi, "par_start in a cell's further paragraph");
    step_begin(c);
    doc_para(c, c, PF_ALIGN, PA_CENTRE);
    tb_ok_all("a cell's paragraph centred");
    CHECK(par[par_at(par_start(c))].align == PA_CENTRE && par[par_at(tr_c[0])].align == PA_LEFT,
          "centred: that paragraph only");
    tb_undo_check("a cell's paragraph centred");
    /* rows and columns */
    step_begin(c);
    CHECK(!tb_addrow(c, 1) && nrows() == 4, "a row below: %u rows", nrows());
    tb_ok_all("a row below");
    tb_undo_check("a row below");
    step_begin(tb_at);
    CHECK(!tb_addrow(tb_at, 0) && nrows() == 5, "a row above: %u rows", nrows());
    tb_ok_all("a row above");
    tb_undo_check("a row above");
    c = tb_at;
    step_begin(c);
    CHECK(!tb_addcol(c, 1), "a column right");
    CHECK(tb_row(tb_at) && tr_n == 4 && tb_cell(tb_at) == 1, "a column right: %u cells", tr_n);
    tb_ok_all("a column right");
    tb_undo_check("a column right");
    c = tb_at;
    tb_row(c);
    c = tb_cstart(0);
    step_begin(c);
    CHECK(!tb_addcol(c, 0), "a column left of the first");
    CHECK(tb_row(tb_at) && tr_n == 5 && tb_cell(tb_at) == 0, "a column left: %u cells", tr_n);
    tb_head(tr_c[0]);
    CHECK(th_k == K_TROW, "the new first cell has the row's code");
    tb_head(tr_c[1]);
    CHECK(th_k == K_TCELL, "the old first cell's code: a cell's");
    tb_ok_all("a column left");
    tb_undo_check("a column left");
    step_begin(tb_at);
    CHECK(!tb_delcol(tb_at), "the first column deleted");
    CHECK(tb_row(tb_at) && tr_n == 4, "deleted: %u cells", tr_n);
    tb_head(tr_c[0]);
    CHECK(th_k == K_TROW, "the new first cell has the row's code (deleted)");
    tb_ok_all("the first column deleted");
    tb_undo_check("the first column deleted");
    tb_row(tb_at);
    step_begin(tb_cstart(2));
    CHECK(!tb_delcol(tb_cstart(2)) && tb_row(tb_at) && tr_n == 3, "a middle column deleted: %u", tr_n);
    tb_ok_all("a middle column deleted");
    tb_undo_check("a middle column deleted");
    w[0] = 60;
    w[1] = 100;
    w[2] = 96;
    step_begin(tb_at);
    CHECK(!tb_widths(tb_at, w, 3), "widths");
    CHECK(tb_row(tb_at) && tr_w[0] == 60 && tr_w[1] == 100 && tr_w[2] == 96, "widths: %u %u %u", tr_w[0], tr_w[1], tr_w[2]);
    tb_ok_all("widths");
    tb_undo_check("widths");
    step_begin(tb_at);
    CHECK(!tb_grid(tb_at, 0) && tb_row(tb_at) && (tr_f & TR_NOGRID), "no borders");
    tb_ok_all("no borders");
    tb_undo_check("no borders");
    step_begin(tb_at);
    i = nrows();
    CHECK(!tb_delrow(tb_at) && nrows() == i - 1, "a row deleted: %u", nrows());
    tb_ok_all("a row deleted");
    tb_undo_check("a row deleted");
    /* what may be deleted; Backspace and Delete at a cell's edges */
    tb_row(tb_at);
    c = tb_cstart(1);
    CHECK(tb_delok(c, c + 1) || cb_byte(&tx, c) == CH_CR, "inside a cell: may");
    CHECK(!tb_delok(tb_cstart(0), tb_cstart(1)), "two cells: not");
    CHECK(tb_delok(5, pos_norm(DOC_LEN)) && tb_delok(0, 3), "outside, and whole tables: may");
    CHECK(!tb_delok(5, tb_cstart(1)), "from outside into a cell: not");
    CHECK(tb_bs(tb_cstart(1)) == 0, "Backspace at a cell's start: nothing");
    CHECK(tb_del(TB_CEND(0)) == 0, "Delete at a cell's end: nothing");
    tb_table(tb_at);
    p = tt_hi;
    CHECK(tb_bs(pos_norm(p)) == 2 && tb_at == p - 1, "Backspace after a table: into it");
    CHECK(tb_del(17) == 0, "Delete before a table: nothing");
    CHECK(tb_bs(5) == 1 && tb_del(5) == 1, "elsewhere: as always");
    /* a table inside a table: no */
    tb_row(tt_lo);
    CHECK(tb_insert(tb_cstart(0), 2, 2) == DE_TB, "no table in a table");
    /* random edits in the cells */
    for (step = 0; step < 2000; ++step) {
        unsigned int nr = nrows(), r, op = rnd(100);
        struct line rl;
        sprintf(what, "table step %u op %u", step, op);
        if (!nr) {
            step_begin(0);
            tb_insert(pos_norm(DOC_LEN), (unsigned char)(1 + rnd(4)), (unsigned char)(1 + rnd(5)));
            tb_ok_all(what);
            continue;
        }
        r = rnd(nr);
        for (i = 0; i < lt_n; ++i) {
            lt_get(i, &rl);
            if (IS_TR(&rl) && !r--)
                break;
        }
        tb_row(lt_start(i));
        k = (unsigned char)rnd(tr_n);
        c = tb_cstart(k);
        for (i = rnd(30); i-- && c < TB_CEND(k);)
            c = pos_next(c);            /* (a character's place in the cell) */
        if (!tb_row(c))
            continue;
        if (op < 40 && rl.len < TB_ROWMAX - 40) {
            static const char *const words[] = { "a", "word ", "Wrapped text that goes on ", "x", "Za\xBF\xF3\xB3\xE6 " };
            step_begin(c);
            tins(c, words[rnd(5)]);
        } else if (op < 55) {
            p = pos_prev(c);
            if (p == c || !tb_delok(p, c))
                continue;
            step_begin(c);
            if (tb_bs(c) == 1)
                doc_delete(p, c);
        } else if (op < 62 && rl.len < TB_ROWMAX - 40) {
            step_begin(c);
            tb_return(c, style_typing(c));
        } else if (op < 68 && rl.len < TB_ROWMAX - 100) {
            step_begin(c);
            tb_addrow(c, (unsigned char)rnd(2));
        } else if (op < 73 && nr > 1) {
            step_begin(c);
            tb_delrow(c);
        } else if (op < 78) {
            step_begin(c);
            if (tb_addcol(c, (unsigned char)rnd(2)) == DE_TB)
                continue;
        } else if (op < 82) {
            step_begin(c);
            tb_delcol(c);
        } else if (op < 86) {
            step_begin(c);
            doc_para(c, c, PF_ALIGN, (unsigned char)rnd(4));
        } else if (op < 90) {
            step_begin(c);
            p = tb_cstart(tb_cell(c));
            if (TB_CEND(tb_cell(c)) <= p)
                continue;
            doc_format(p, TB_CEND(tb_cell(c)), FMT_SET, SF_BOLD);
        } else if (op < 93) {
            step_begin(c);
            tb_grid(c, (unsigned char)rnd(2));
        } else if (op < 96) {
            w[0] = (unsigned char)(20 + rnd(100));
            w[1] = (unsigned char)(20 + rnd(100));
            w[2] = (unsigned char)(20 + rnd(100));
            step_begin(c);
            tb_widths(c, w, 3);
        } else {
            step_begin(c);
            tb_deltable(c);
        }
        if (getenv("TB_DEBUG") && (int)step >= atoi(getenv("TB_DEBUG"))) {
            pos_t q;
            printf("%s at %lu:", what, (unsigned long)c);
            for (q = 0; q < DOC_LEN; ++q) printf(" %02X", doc_byte(q));
            printf("\nlines:");
            for (i = 0; i < lt_n; ++i) { lt_get(i, &l); printf(" [%u h%u w%u f%u]", l.len, l.h, l.w, l.f); }
            printf("\n");
        }
        tb_ok_all(what);
        if (rnd(4) == 0)
            tb_undo_check(what);
        if (fails)
            break;
    }
    printf("tables: %u rows after 2000 random edits: ok\n", nrows());
    /* the file: 1.6, the table's bytes as they were */
    tt_an = snapshot(tt_a);
    flen = 0;
    CHECK(fmt_save(wr, 0) == 0, "tables: saved");
    CHECK(!memcmp(file, "CWRD\1\6", 6), "tables: the header (1.6)");
    doc_new();
    fpos = 0;
    CHECK(fmt_load(rd) == 0, "tables: read back");
    tt_bn = snapshot(tt_b);
    CHECK(tt_an == tt_bn && !memcmp(tt_a, tt_b, tt_an), "tables: the same bytes read back");
    if (tt_an != tt_bn || memcmp(tt_a, tt_b, tt_an)) {
        for (i = 0; i < tt_an && i < tt_bn && tt_a[i] == tt_b[i]; ++i)
            ;
        printf("first difference at %u of %lu/%lu:", i, (unsigned long)tt_an, (unsigned long)tt_bn);
        for (h1 = i > 12 ? i - 12 : 0; h1 < i + 12 && h1 < tt_an; ++h1) printf(" %02X", tt_a[h1]);
        printf("\n   read back:");
        for (h1 = i > 12 ? i - 12 : 0; h1 < i + 12 && h1 < tt_bn; ++h1) printf(" %02X", tt_b[h1]);
        printf("\n");
    }
    tb_ok_all("tables: read back");
    /* a row longer than a line can hold: cut, laid out the same */
    doc_new();
    tb_insert(0, 1, 2);
    c = tb_at;
    for (i = 0; i < 50; ++i)
        c = tins(c, "Forty-one characters of a long cell text ");
    tb_ok_all("a row longer than a line");
    lt_get(0, &l);
    CHECK(IS_TR(&l) && l.len <= 2047, "a long row: its line %u bytes", l.len);
    /* (del_keep) a row deleted whose bold text goes on into the next row
       without a code: the next row's text stays bold; then a column, then
       the table, the same way */
    {
        pos_t a, z;
        static const char *const what[3] = { "a row", "a column", "the table" };
        unsigned char k2;
        for (k2 = 0; k2 < 3; ++k2) {
            doc_new();
            ul_reset();
            c = tins(0, "Before.\r");
            tb_insert(c, 3, k2 == 1 ? 2 : 1);
            if (k2 == 1) {
                c = tins(tb_at, "One");
                tb_next(c, 1);          /* (row 1, cell 2: the column deleted) */
                a = tb_at;
                c = tins(a, "Two");
                tb_next(c, 1);          /* (row 2, cell 1) */
                c = tins(tb_at, "Three");
            } else {
                c = tins(tb_at, "One");
                tb_next(c, 1);
                a = tb_at;
                c = tins(a, "Two");
                tb_next(c, 1);
                c = tins(tb_at, "Three");
            }
            if (k2 == 2)
                c = tins(pos_norm(DOC_LEN), "After");
            doc_format(a, k2 == 2 ? pos_norm(DOC_LEN) : c, FMT_SET, SF_BOLD);
            lay_fix();
            k = k2 == 0 ? tb_delrow(pos_norm(a + 3)) : k2 == 1 ? tb_delcol(pos_norm(a + 3)) : tb_deltable(pos_norm(a + 3));
            CHECK(!k, "del_keep: %s not deleted", what[k2]);
            z = snapshot(tt_a);
            for (i = 0; i + 4 < z && memcmp(tt_a + i, k2 == 2 ? "After" : "Three", 5); ++i)
                ;
            CHECK(i + 4 < z, "del_keep, %s: the text after it gone", what[k2]);
            CHECK(sty[style_at(i + 1)].flags & SF_BOLD, "del_keep, %s: the text after it no longer bold", what[k2]);
            tb_ok_all("del_keep");
        }
        printf("tables: a row, a column, the table deleted, the style after them kept: ok\n");
    }
    /* a table at the document's end, no paragraph after it */
    doc_new();
    tb_insert(0, 2, 2);
    tx_del(DOC_LEN - 1, 1);
    lay_fix();
    tb_ok_all("a table at the end without its last CR");
    printf("tables: the file 1.6, long rows, a table at the end: ok\n");
    /* the PDF: the rows' boxes, their cells' text where the screen has it */
    doc_new();
    ul_reset();
    wrap_w = 504;
    c = tins(0, "A table in a PDF.\rAfter it.");
    tb_insert(pos_norm(18), 2, 3);
    c = tins(tb_at, "Cell one");
    tb_next(c, 1);
    c = tins(tb_at, "A second cell, its text long enough to wrap onto lines of its own in the PDF too.");
    tb_next(c, 1);
    c = tins(tb_at, "Third cell");
    tb_return(c, style_typing(c));
    c = tins(tb_at, "its second paragraph");
    tb_next(c, 1);
    c = tins(tb_at, "Row two");
    tb_ok_all("the PDF's table");
    pdf_check("table");
    pdf_buf[pdf_n] = 0;
    {
        unsigned int nre = 0;
        const char *q = (const char *)pdf_buf;
        while ((q = strstr(q, " re\n")) != 0) {
            ++nre;
            ++q;
        }
        CHECK(nre == 6, "the PDF's table: %u cells' boxes, want 6", nre);
    }
    printf("tables: the PDF (6 cells' boxes): ok\n");
    /* the round trips: exported as .docx and .odt, read back: the same
       text, styles, paragraphs, and table codes (their widths, the
       borders off in the second table) */
    w[0] = 60;
    w[1] = 100;
    w[2] = 96;
    tb_row(pos_norm(20));
    tb_widths(pos_norm(20), w, 3);
    c = tins(pos_norm(DOC_LEN), "\rA second table, without borders:\r");
    tb_insert(c, 2, 2);
    c = tins(tb_at, "Za\xBF\xF3\xB3\xE6");
    doc_format(c - 6, c, FMT_SET, SF_BOLD);
    tb_grid(c, 0);
    tb_ok_all("the round trips' document");
    tc_dump(tt_a, &tt_an);
    ox_rt(OK_DOCX, "tables.docx");
    tc_dump(tt_b, &tt_bn);
    CHECK(tt_an == tt_bn && !memcmp(tt_a, tt_b, tt_an), "tables.docx read back: its table codes not the same (%lu, %lu)",
          (unsigned long)tt_an, (unsigned long)tt_bn);
    tb_ok_all("tables.docx read back");
    ox_rt(OK_ODT, "tables.odt");
    tc_dump(tt_b, &tt_bn);
    CHECK(tt_an == tt_bn && !memcmp(tt_a, tt_b, tt_an), "tables.odt read back: its table codes not the same (%lu, %lu)",
          (unsigned long)tt_an, (unsigned long)tt_bn);
    tb_ok_all("tables.odt read back");
    if (tt_an != tt_bn || memcmp(tt_a, tt_b, tt_an))
        for (i = 0; i < tt_an || i < tt_bn; i += 3)
            printf("  %u %02X %3u | %u %02X %3u\n", tt_a[i], tt_a[i + 1], tt_a[i + 2], tt_b[i], tt_b[i + 1], tt_b[i + 2]);
    printf("tables: exported as .docx and .odt and read back: the same\n");
    /* (W71) a list's item, then a table: the .odt's list closed before
       the table, not in its first cell (tools/check_docx.py reads
       listtbl.odt's XML: make test) */
    doc_new();
    c = tins(0, "Some examples:\rAn item\r");
    doc_named(pos_norm(16), pos_norm(16), PK_BULLET);
    tb_insert(pos_norm(DOC_LEN), 2, 2);
    c = tins(tb_at, "Extension");
    tb_ok_all("a list, then a table");
    tc_dump(tt_a, &tt_an);
    ox_rt(OK_ODT, "listtbl.odt");
    tc_dump(tt_b, &tt_bn);
    CHECK(tt_an == tt_bn && !memcmp(tt_a, tt_b, tt_an), "listtbl.odt read back: its table codes not the same");
    ox_rt(OK_DOCX, "listtbl.docx");
    printf("tables: a list, then a table, exported: ok\n");
}
