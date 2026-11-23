/* Veranda Write: the edits on the document's text (overlay 6 on the
   X16, with undo.c; text.c has the text and its places).

   The text is one byte stream (cw.h, docs/FORMAT.md): characters, CR at
   each paragraph's end and 3-byte codes [kind arg kind]. A character
   style code [K_CS s K_CS] says that the characters after it, up to the
   next one, are in style s of the style table (a font, a size, bold,
   italic, underline); the text starts in style 0. A code reads the same
   from either end (its last byte is its kind), so the text can be walked
   both ways from any place that is not inside a code.

   A place in the text ("normalised") is never inside a code and never
   just before one: the cursor stands after the codes, in front of the
   character they style (pos_norm). The edits below keep the styles of
   the text around them: typing in a style other than the one in effect
   puts a code before the new characters and one after them for the text
   that follows; deleting a range puts back the style of the text after
   it; a format change rewrites the codes inside a range and bounds it
   with two. doc_tidy then takes out the codes that do nothing (one
   straight before another, one setting the style already in effect).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CW_HOST
#include "v/ovl6.h"
#endif
#include <string.h>
#include "cw.h"

/*---------------------------------------------------------------
  the primitives: logged for undo, the layout told; tk_a and tk_b (the
  callers' places: doc_para, doc_named) moved with the bytes
 ---------------------------------------------------------------*/
pos_t tk_a, tk_b;

unsigned char tx_ins(pos_t p, const unsigned char *s, unsigned int n)
{
    if (!n)
        return 0;
    if (!cb_room(&tx, n) || cb_insert(&tx, p, s, n)) {
        doc_err = DE_MEM;
        return 1;
    }
    if (tk_a >= p)
        tk_a += n;
    if (tk_b >= p)
        tk_b += n;
    ul_log('I', p, s, n);
    lay_dirty(p, n, 0);
    doc_modified = 1;
    return 0;
}

void tx_del(pos_t p, pos_t n)
{
    if (!n)
        return;
    ul_log_del(p, n);
    cb_delete(&tx, p, n);
    if (tk_a > p)
        tk_a = tk_a >= p + n ? tk_a - n : p;
    if (tk_b > p)
        tk_b = tk_b >= p + n ? tk_b - n : p;
    lay_dirty(p, 0, n);
    doc_modified = 1;
}

void FASTCALL tx_set(pos_t p, unsigned char c)
{
    unsigned char old = cb_byte(&tx, p);
    if (old == c)
        return;
    ul_log_set(p, old, c);
    cb_write(&tx, p, &c, 1);
    lay_dirty(p, 1, 1);
    doc_modified = 1;
}

static unsigned char code[3] = { K_CS, 0, K_CS };
static unsigned char pcode[3] = { K_PS, 0, K_PS };

static unsigned char ins_code(pos_t p, unsigned char s)
{
    code[1] = s;
    return tx_ins(p, code, 3);
}

/*---------------------------------------------------------------
  the edits
 ---------------------------------------------------------------*/
void doc_new(void)
{
    ld_kind = LD_NONE;                  /* (a document being opened: no more) */
    tx_reset();
    sty_reset();
    par_reset();
    ul_reset();
    lay_all();
    doc_modified = 0;
}

/* places kept right across doc_tidy's deletions (the callers' cursor
   and selection) */
pos_t tp_a, tp_b;

static void track(pos_t q)              /* 3 bytes at q deleted */
{
    if (tp_a > q)
        tp_a = tp_a >= q + 3 ? tp_a - 3 : q;
    if (tp_b > q)
        tp_b = tp_b >= q + 3 ? tp_b - 3 : q;
}

/* the style a paragraph begun by a CR typed at p gets: the style of p's
   paragraph, but Body after a heading when p is its end */
static unsigned char new_par(pos_t p)
{
    unsigned char ps = par_at(par_start(p)), c;
    if (ps && IS_HEAD(par[ps].kind)) {
        c = cb_byte(&tx, p);
        if (p >= tx.total || c == CH_CR)
            return 0;
    }
    return ps;
}

/* a paragraph code [K_PS ps K_PS] after each CR in [lo, hi) (codes in it
   whole); the bytes added */
static unsigned int par_codes(pos_t lo, pos_t hi, unsigned char ps)
{
    unsigned int k = 0;
    unsigned char c;
    if (!ps)
        return 0;
    pcode[1] = ps;
    while (hi > lo) {
        c = cb_byte(&tx, --hi);
        if (IS_KIND(c))
            hi -= 2;
        else if (c == CH_CR && !tx_ins(hi + 1, pcode, 3))
            k += 3;
    }
    return k;
}

pos_t doc_insert(pos_t p, const unsigned char *s, unsigned int n, unsigned char t)
{
    unsigned char sp, np = 0;
    unsigned int i;
    pos_t q;
    doc_err = 0;
    for (i = 0; i < n; ++i)
        if (s[i] == CH_CR) {
            np = new_par(p);
            break;
        }
    sp = style_at(p);
    if (t == sp) {
        if (!tx_ins(p, s, n))
            p += n + par_codes(p, p + n, np);
        lay_fix();
        return pos_norm(p);
    }
    q = codes_before(p);
    if (q < p && t == style_at(q)) {
        if (tx_ins(q, s, n)) {
            lay_fix();
            return p;
        }
        q += n + par_codes(q, q + n, np);
        lay_fix();
        return pos_norm(q);
    }
    if (ins_code(p, t))
        return p;
    tp_a = p;
    tp_b = p + 3;
    if (!tx_ins(p + 3, s, n)) {
        tp_b += n + par_codes(p + 3, p + 3 + n, np);
        if (tp_b < tx.total)
            ins_code(tp_b, sp);         /* (no room: the text after takes t) */
    }
    lay_fix();
    doc_tidy(p, tp_b);
    lay_fix();
    return pos_norm(tp_b);
}

pos_t doc_insert_raw(pos_t p, const unsigned char *s, unsigned int n)
{
    unsigned char sp;
    doc_err = 0;
    sp = style_at(p);
    if (tx_ins(p, s, n))
        return p;
    tp_a = p;
    tp_b = p + n;
    if (tp_b < tx.total)
        ins_code(tp_b, sp);
    lay_fix();
    doc_tidy(p, tp_b);
    lay_fix();
    return pos_norm(tp_b);
}

pos_t doc_delete(pos_t lo, pos_t hi)
{
    unsigned char sl, sh;
    doc_err = 0;
    if (hi <= lo)
        return pos_norm(lo);
    sl = style_at(lo);
    sh = style_at(hi);
    tx_del(lo, hi - lo);
    if (sl != sh && lo < tx.total)
        ins_code(lo, sh);
    lay_fix();
    tp_a = tp_b = lo;
    doc_tidy(lo, lo + 3);
    lay_fix();
    /* (W41: lo itself may be past the end now: "ab" [code] "c" with c
       deleted takes the code out too, 3 bytes before lo) */
    return pos_norm(tp_a);
}

/* the codes in [lo, hi) that do nothing taken out (tp_a, tp_b kept
   right). The walk starts at lo's line, whose style is known */
void doc_tidy(pos_t lo, pos_t hi)
{
    struct line l;
    pos_t q;
    unsigned char s, c, a;
    if (hi > tx.total)
        hi = tx.total;
    lt_find(lo);
    lt_get(lf_i, &l);
    q = lf_pos;
    s = l.s;
    while (q < hi) {
        c = cb_byte(&tx, q);
        if (!IS_KIND(c)) {
            ++q;
            continue;
        }
        if (c == K_PB && !((!q || cb_byte(&tx, q - 1) == CH_CR) && q + 3 < tx.total
                           && cb_byte(&tx, q + 3) == CH_CR)) {
            tx_del(q, 3);               /* (a page break is a paragraph of its own: one joined to another goes) */
            track(q);
            hi = hi >= q + 3 ? hi - 3 : q;
            continue;
        }
        if (c != K_CS) {
            q += 3;
            continue;
        }
        a = cb_byte(&tx, q + 1);
        if (q + 3 >= tx.total || a == s || cb_byte(&tx, q + 3) == K_CS) {
            tx_del(q, 3);               /* dead: at the end, the style there, or overridden */
            track(q);
            hi = hi >= q + 3 ? hi - 3 : q;
            continue;
        }
        s = a;
        q += 3;
    }
}

/* every character in [lo, hi) has flag f */
unsigned char doc_all_have(pos_t lo, pos_t hi, unsigned char f)
{
    unsigned char s = style_at(lo), c;
    pos_t q = lo;
    while (q < hi) {
        c = cb_byte(&tx, q);
        if (c == K_CS) {
            s = cb_byte(&tx, q + 1);
            q += 3;
            continue;
        }
        if (IS_KIND(c)) {
            q += 3;
            continue;
        }
        if (c != CH_CR && !(sty[s].flags & f))
            return 0;
        ++q;
    }
    return 1;
}

void doc_format(pos_t lo, pos_t hi, unsigned char op, unsigned char v)
{
    unsigned char sl, sh, c, n;
    pos_t q;
    doc_err = 0;
    tp_a = lo;
    tp_b = hi;
    if (hi <= lo)
        return;
    sl = style_at(lo);
    sh = style_at(hi);
    /* the new styles first: a full table changes nothing */
    n = fmt_map(sl, op, v);
    if (n == 0xFF) {
        doc_err = DE_STY;
        return;
    }
    for (q = lo; q < hi;) {
        c = cb_byte(&tx, q);
        if (c == K_CS && fmt_map(cb_byte(&tx, q + 1), op, v) == 0xFF) {
            doc_err = DE_STY;
            return;
        }
        q += IS_KIND(c) ? 3 : 1;
    }
    if (hi < tx.total && ins_code(hi, sh))
        return;
    for (q = lo; q < hi;) {
        c = cb_byte(&tx, q);
        if (c == K_CS) {
            tx_set(q + 1, fmt_map(cb_byte(&tx, q + 1), op, v));
            q += 3;
        } else
            q += IS_KIND(c) ? 3 : 1;
    }
    if (ins_code(lo, n))
        return;
    tp_a = lo + 3;
    tp_b = hi + 3;
    lay_fix();
    doc_tidy(lo, tp_b + 3);
    lay_fix();
    tp_a = pos_norm(tp_a);
    tp_b = pos_norm(tp_b);
}

/*---------------------------------------------------------------
  paragraph styles
 ---------------------------------------------------------------*/
unsigned char FASTCALL par_map(unsigned char ps, unsigned char op, unsigned char v)
{
    static struct pstyle c;
    int k;
    memcpy(&c, &par[ps], sizeof c);
    switch (op) {
    case PF_NAMED: memcpy(&c, &named[v], sizeof c); break;
    case PF_ALIGN: c.align = v; break;
    case PF_INDENT:
        k = c.left + (signed char)v;
        c.left = k < 0 ? 0 : k > 240 ? 240 : (unsigned char)k;
        break;
    case PF_SPACING: c.spacing = v; break;
    case PF_AFTER: c.after = v; break;
    }
    return par_find(&c);
}

/* the paragraph starting at q to style n (its code put in, changed or
   taken out) */
static void par_set(pos_t q, unsigned char n)
{
    unsigned char o = par_at(q);
    if (o == n)
        return;
    if (!o) {
        pcode[1] = n;
        tx_ins(q, pcode, 3);
    } else if (!n)
        tx_del(q, 3);
    else
        tx_set(q + 1, n);
    lay_fix();
}

/* the paragraphs of [lo, hi] from the last to the first (an edit in one
   moves none before it); 0: the first's start, its q */
static pos_t p_first, p_q;

static unsigned char par_last(pos_t lo, pos_t hi)
{
    doc_err = 0;
    p_first = par_start(lo);
    p_q = par_start(hi > lo ? pos_prev(hi) : lo);
    return 1;
}

static unsigned char par_back(void)
{
    if (p_q <= p_first)
        return 0;
    p_q = par_start(p_q - 1);
    return 1;
}

/* the new styles first: a full table changes nothing */
static unsigned char par_room(pos_t lo, pos_t hi, unsigned char op, unsigned char v)
{
    par_last(lo, hi);
    do
        if (par_map(par_at(p_q), op, v) == 0xFF) {
            doc_err = DE_PAR;
            return 0;
        }
    while (par_back());
    par_last(lo, hi);
    return 1;
}

void doc_para(pos_t lo, pos_t hi, unsigned char op, unsigned char v)
{
    if (!par_room(lo, hi, op, v))
        return;
    do {
        par_set(p_q, par_map(par_at(p_q), op, v));
        if (doc_err)
            return;
    } while (par_back());
}

/* the named styles' characters: headings their size and bold, a quote
   italic (the size back to Body's, the flags off, when a paragraph stops
   being one) */
static const unsigned char nm_size[PK_N] = { 0, 27, 18, 12, 0, 0, 0 };
static const unsigned char nm_flags[PK_N] = { 0, SF_BOLD, SF_BOLD, SF_BOLD, SF_ITALIC, 0, 0 };

static void chars(pos_t *a, pos_t *b, unsigned char op, unsigned char v)
{
    if (!v || *a >= *b || doc_err)
        return;
    doc_format(*a, *b, op, v);
    *a = tp_a;
    *b = tp_b;
}

void doc_named(pos_t lo, pos_t hi, unsigned char kind)
{
    unsigned char o, n;
    pos_t a, b, e;
    if (!par_room(lo, hi, PF_NAMED, kind))
        return;
    do {
        o = par[par_at(p_q)].kind;
        n = par_map(par_at(p_q), PF_NAMED, kind);
        e = par_next(p_q);
        a = pos_norm(p_q);
        b = e == 0xFFFFFFFFUL ? tx.total : pos_norm(e);
        chars(&a, &b, FMT_CLR, nm_flags[o]);
        chars(&a, &b, FMT_SIZE, IS_HEAD(kind) ? nm_size[kind] : IS_HEAD(o) ? DEF_SIZE : 0);
        chars(&a, &b, FMT_SET, nm_flags[kind]);
        if (doc_err)
            return;
        par_set(p_q, n);
        if (doc_err)
            return;
    } while (par_back());
}
