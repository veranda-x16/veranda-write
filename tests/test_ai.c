/* Veranda Write's unit tests: AI help's text (src/ai.c, DECISIONS W68;
   included by test_main.c after test_table.c, whose helpers it uses).

   What goes to Veranda Ask (AI.IN): a selection's characters with CR at
   each paragraph's end and no codes (character and paragraph styles, a
   cell's further paragraphs), Polish letters as the text has them; the
   paragraph at the cursor; nothing but spaces refused; 8,192 bytes taken,
   8,193 refused; in a table one cell's text, a selection over two cells,
   out of a cell or holding a table refused. The answer (AI.OUT) put in:
   in place of the selection (its first character's style, the
   paragraphs in its paragraph's style, a heading's too), after its
   paragraph (Body after a heading, its characters Body's), in a cell (the
   new paragraphs the cell's), CR LF, LF, runs of CRs and CRs at the ends,
   characters the text cannot hold, a 15 KB answer in pieces; each one
   undo step back to the bytes before and redone to the bytes after, the
   line table as a fresh layout's, the rows whole.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */

static unsigned char ai_out[20000];
static unsigned int ai_on;

void FASTCALL plat_ai_put(const unsigned char *b, unsigned int n)
{
    if (ai_on + n <= sizeof ai_out)
        memcpy(ai_out + ai_on, b, n);
    ai_on += n;
}

/* AI.IN for [lo, hi): ai_check's result, the bytes in ai_out */
static unsigned char ai_in(pos_t lo, pos_t hi)
{
    unsigned char e;
    ai_lo = lo;
    ai_hi = hi;
    e = ai_check();
    ai_on = 0;
    if (!e)
        ai_text();
    return e;
}

/* the document's characters (no codes) from p to q */
static unsigned int plain(pos_t p, pos_t q, unsigned char *b)
{
    unsigned int n = 0;
    unsigned char c;
    for (; p < q; ++p) {
        c = doc_byte(p);
        if (IS_KIND(c))
            p += 2;
        else
            b[n++] = c;
    }
    return n;
}

static unsigned char ai_tmp[20000];

static int text_is(const char *want)
{
    unsigned int n = plain(0, DOC_LEN, ai_tmp);
    return n == strlen(want) && !memcmp(ai_tmp, want, n);
}

/* the answer s put in for [lo, hi) as one undo step (mode), in pieces of
   k bytes (as v/aiui.c reads AI.OUT); 0 done */
static unsigned char ai_put(pos_t lo, pos_t hi, unsigned char mode, const char *s, unsigned int k)
{
    unsigned int n = (unsigned int)strlen(s), i;
    static unsigned char in[20000];
    memcpy(in, s, n);                   /* (not iobuf: ai_add's runs are there) */
    step_begin(hi);
    ai_lo = lo;
    ai_hi = hi;
    ai_begin(mode);
    for (i = 0; i < n; i += k)
        ai_add(in + i, n - i < k ? n - i : k);
    return ai_end();
}

/* the paragraphs' styles (par_at) in order, up to 32 */
static unsigned int par_list(unsigned char *ps)
{
    unsigned int n = 0;
    pos_t p = 0;
    for (;;) {
        ps[n++] = par_at(p);
        p = par_next(p);
        if (p >= DOC_LEN || n == 32)
            return n;
    }
}

/* the place of the text's k-th character */
static pos_t at(unsigned int k)
{
    pos_t p = pos_norm(0);
    while (k--)
        p = pos_next(p);
    return p;
}

/* the size of the style of the first character at p */
static unsigned char size_at(pos_t p)
{
    return sty[style_at(pos_norm(p))].size;
}

static void test_ai(void)
{
    pos_t p, q, c;
    unsigned char e, ps[32], t, h1;
    unsigned int i, n;
    static char big[16000];
    const char *pl = "Za\xbf\xf3\xb3\xe6 g\xea\xb6l\xb1 ja\xbc\xf1";   /* (Zażółć gęślą jaźń, ISO-8859-16) */
    doc_new();
    ul_reset();
    wrap_w = 504;
    /* "One two three." bold "two", "Second par." a heading, "Third." */
    c = tins(0, "One two three.\rSecond par.\rThird ");
    c = tins(c, pl);
    doc_format(4, 7, FMT_SET, SF_BOLD);
    doc_named(at(15), at(15), PK_H1);
    lay_fix();
    check_layout("ai: the document");
    /* AI.IN: a word, a selection over the paragraphs, the codes left out */
    CHECK(!ai_in(at(4), at(7)) && ai_on == 3 && !memcmp(ai_out, "two", 3), "ai: a bold word");
    p = at(0);
    q = DOC_LEN;
    CHECK(!ai_in(p, q) && ai_n == ai_on, "ai: all of it: %d bytes, counted %u", ai_on, ai_n);
    n = (unsigned int)sprintf((char *)ai_tmp, "One two three.\rSecond par.\rThird %s", pl);
    CHECK(ai_on == n && !memcmp(ai_out, ai_tmp, n), "ai: all of it: CR between the paragraphs, no codes, Polish as it is");
    /* the paragraph at the cursor: its characters, not its CR */
    ai_para(at(5));
    CHECK(ai_lo == at(0) && doc_byte(ai_hi) == CH_CR && ai_hi - ai_lo == 14 + 6, "ai: the paragraph at the cursor (%lu..%lu)",
          (unsigned long)ai_lo, (unsigned long)ai_hi);
    ai_para(DOC_LEN);
    CHECK(ai_hi == DOC_LEN && !ai_in(ai_lo, ai_hi) && ai_on == 6 + strlen(pl), "ai: the last paragraph (%u bytes)", ai_on);
    /* nothing to send */
    c = tins(DOC_LEN, "\r   \t ");
    ai_para(DOC_LEN);
    CHECK(ai_in(ai_lo, ai_hi) == AE_EMPTY, "ai: spaces only: refused");
    CHECK(ai_in(DOC_LEN, DOC_LEN) == AE_EMPTY, "ai: nothing: refused");
    /* the length: 8,192 taken, 8,193 refused */
    doc_new();
    ul_reset();
    for (i = 0; i < 8191; ++i)
        big[i] = (char)(i % 61 == 60 ? '\r' : 'a' + i % 26);
    big[8191] = 0;
    tins(0, big);
    tins(DOC_LEN, "b");
    CHECK(!ai_in(at(0), DOC_LEN) && ai_on == 8192 && ai_n == 8192, "ai: 8,192 bytes taken (%u)", ai_on);
    tins(DOC_LEN, "c");
    CHECK(ai_in(at(0), DOC_LEN) == AE_LONG, "ai: 8,193 refused");
    CHECK(!ai_in(at(1), DOC_LEN), "ai: less of it taken");

    /* the answer in place of a selection: its first character's style
       (bold), its paragraphs the paragraph's style */
    doc_new();
    ul_reset();
    c = tins(0, "One two three.\rSecond par.\rThird.");
    doc_format(4, 7, FMT_SET, SF_BOLD);
    lay_fix();
    tt_an = snapshot(tt_a);
    e = ai_put(at(4), at(13), AM_REPLACE, "Deux\r\rtrois", 3);
    CHECK(!e, "ai: replaced: %d", e);
    CHECK(text_is("One Deux\rtrois.\rSecond par.\rThird."), "ai: replaced: the text (%.*s)", (int)plain(0, DOC_LEN, ai_tmp), ai_tmp);
    CHECK(sty[style_at(at(4))].flags == SF_BOLD && sty[style_at(at(10))].flags == SF_BOLD
          && sty[style_at(at(14))].flags == 0, "ai: replaced: the first character's style, the text after its own");
    CHECK(ai_lo == at(4) && plain(ai_lo, ai_hi, ai_tmp) == 10 && !memcmp(ai_tmp, "Deux\rtrois", 10), "ai: the answer's range");
    check_layout("ai: replaced");
    tb_undo_check("ai: replaced");
    /* over two paragraphs, the first a heading: the answer's paragraphs headings */
    doc_new();
    ul_reset();
    c = tins(0, "Title here\rBody text.\rEnd.");
    doc_named(at(0), at(0), PK_H1);
    lay_fix();
    h1 = par_at(0);
    CHECK(IS_HEAD(par[h1].kind), "ai: a heading");
    tt_an = snapshot(tt_a);
    e = ai_put(at(6), at(15), AM_REPLACE, "one\r\ntwo\nthree\r\r", 2);
    CHECK(!e && text_is("Title one\rtwo\rthree text.\rEnd."), "ai: over two paragraphs (%.*s)", (int)plain(0, DOC_LEN, ai_tmp), ai_tmp);
    n = par_list(ps);
    CHECK(n == 4 && ps[0] == h1 && ps[1] == h1 && ps[2] == h1 && ps[3] == 0, "ai: the heading's paragraphs (%u: %u %u %u %u)",
          n, ps[0], ps[1], ps[2], ps[3]);
    check_layout("ai: over two paragraphs");
    tb_undo_check("ai: over two paragraphs");

    /* after the paragraph (Summarise, Ask): after a heading Body, its
       characters Body's (12 points, not bold) */
    tt_an = snapshot(tt_a);
    e = ai_put(at(0), at(5), AM_AFTER, "\r\rA summary.\rIn two.\r", 5);
    CHECK(!e && text_is("Title one\rA summary.\rIn two.\rtwo\rthree text.\rEnd."), "ai: after a heading (%.*s)",
          (int)plain(0, DOC_LEN, ai_tmp), ai_tmp);
    n = par_list(ps);
    CHECK(n == 6 && ps[0] == h1 && ps[1] == 0 && ps[2] == 0 && ps[3] == h1, "ai: after a heading: Body");
    p = par_next(0);
    CHECK(size_at(p) == DEF_SIZE && !(sty[style_at(pos_norm(p))].flags & SF_BOLD) && size_at(0) == 27,
          "ai: after a heading: Body's characters (%u)", size_at(p));
    CHECK(ai_lo == pos_norm(p) && plain(ai_lo, ai_hi, ai_tmp) == 18 && !memcmp(ai_tmp, "A summary.\rIn two.", 18), "ai: the summary's range");
    check_layout("ai: after a heading");
    tb_undo_check("ai: after a heading");
    /* after the last paragraph, whose end is the text's */
    tt_an = snapshot(tt_a);
    p = par_start(DOC_LEN);
    e = ai_put(pos_norm(p), DOC_LEN, AM_AFTER, "Answer: yes.", 4);
    CHECK(!e && text_is("Title one\rA summary.\rIn two.\rtwo\rthree text.\rEnd.\rAnswer: yes."), "ai: after the last paragraph");
    tb_undo_check("ai: after the last paragraph");
    /* a selection that ends with its paragraph's CR: after that paragraph */
    p = par_next(0);
    q = par_next(p);
    tt_an = snapshot(tt_a);
    e = ai_put(pos_norm(p), q, AM_AFTER, "X", 4);
    CHECK(!e && text_is("Title one\rA summary.\rX\rIn two.\rtwo\rthree text.\rEnd.\rAnswer: yes."), "ai: a selection with its CR");
    tb_undo_check("ai: a selection with its CR");
    /* control characters left out, $A0-$FF kept; an empty answer: nothing */
    tt_an = snapshot(tt_a);
    e = ai_put(at(0), at(5), AM_REPLACE, "\x01T\x7f\x85\xa1t\x1bl", 8);
    CHECK(!e && plain(ai_lo, ai_hi, ai_tmp) == 4 && !memcmp(ai_tmp, "T\xa1tl", 4), "ai: bytes the text cannot hold left out");
    tb_undo_check("ai: bytes left out");
    tt_an = snapshot(tt_a);
    e = ai_put(at(0), at(5), AM_REPLACE, "\r\r\x02", 8);
    CHECK(e == DE_AI && snapshot(tt_b) == tt_an && !memcmp(tt_a, tt_b, tt_an), "ai: an empty answer: nothing changed");
    /* a 15 KB answer, Polish, in pieces */
    for (i = 0; i < 15000; ++i)
        big[i] = (char)(i % 97 == 96 ? '\r' : i % 7 == 6 ? ' ' : (unsigned char)pl[i % 17]);
    big[15000] = 0;
    tt_an = snapshot(tt_a);
    e = ai_put(at(0), at(5), AM_REPLACE, big, 192);
    n = plain(ai_lo, ai_hi, ai_tmp);
    CHECK(!e && n > 14000 && n <= 15000, "ai: 15 KB: %u bytes", n);
    check_layout("ai: 15 KB");
    tb_undo_check("ai: 15 KB");

    /* in a table's cell: its text only; the answer's paragraphs the cell's */
    doc_new();
    ul_reset();
    c = tins(0, "Before.\rAfter.");
    p = pos_norm(8);
    e = tb_insert(p, 2, 2);
    CHECK(!e, "ai: a table");
    c = tins(tb_at, "Cell one text");
    CHECK(tb_next(c, 1), "ai: Tab");
    c = tins(tb_at, "Cell two");
    tb_ok_all("ai: the table");
    tb_row(c);
    q = tb_cstart(0);
    CHECK(!ai_in(q, q + 13) && ai_on == 13 && !memcmp(ai_out, "Cell one text", 13), "ai: a cell's text");
    ai_para(q + 5);
    CHECK(ai_lo == q && !ai_in(ai_lo, ai_hi) && ai_on == 13, "ai: the cell's paragraph at the cursor");
    CHECK(ai_in(q + 5, c) == AE_CELLS, "ai: over two cells: refused");
    CHECK(ai_in(pos_norm(0), DOC_LEN) == AE_CELLS, "ai: a table in it: refused");
    CHECK(ai_in(pos_norm(2), q + 3) == AE_CELLS, "ai: into a cell: refused");
    CHECK(ai_in(c - 2, DOC_LEN) == AE_CELLS, "ai: out of a cell: refused");
    tt_an = snapshot(tt_a);
    e = ai_put(q + 5, q + 13, AM_REPLACE, "first\rsecond\rthird", 4);
    CHECK(!e, "ai: in a cell: %d", e);
    tb_ok_all("ai: in a cell");
    tb_row(q);
    CHECK(tr_n == 2, "ai: in a cell: the row's 2 cells (%u)", tr_n);
    tb_head(par_next(q));
    CHECK(th_k == K_TPARA, "ai: in a cell: a further paragraph of it (%02X)", th_k);
    tb_row(q);
    CHECK(ai_in(tb_cstart(0), TB_CEND(0)) == 0 && ai_on == 23 && !memcmp(ai_out, "Cell first\rsecond\rthird", 23),
          "ai: in a cell: the cell's text (%.*s)", ai_on, ai_out);
    tb_undo_check("ai: in a cell");
    tb_row(q);
    t = tb_cell(tb_cstart(1));
    CHECK(t == 1, "ai: the second cell");
    p = tb_cstart(1);
    tt_an = snapshot(tt_a);
    e = ai_put(p, p + 4, AM_AFTER, "Its summary.\rTwo.", 3);
    tb_ok_all("ai: after a cell's paragraph");
    tb_row(p);
    CHECK(!e && tr_n == 2 && !ai_in(tb_cstart(1), TB_CEND(1)) && ai_on == 26 && !memcmp(ai_out, "Cell two\rIts summary.\rTwo.", 26),
          "ai: after a cell's paragraph (%.*s)", ai_on, ai_out);
    tb_undo_check("ai: after a cell's paragraph");
    CHECK(text_is("Before.\rCell first\rsecond\rthird\rCell two\rIts summary.\rTwo.\r\r\rAfter."), "ai: the table's text (%.*s)",
          (int)plain(0, DOC_LEN, ai_tmp), ai_tmp);
}
