/* Veranda Write on Veranda: the clipboard (overlay 7, with the window chrome).

   A copy goes to Veranda's Text Scrap (the SDK's scrap library, compiled
   into this overlay: Veranda's DECISIONS V54), as plain text for every
   other program, and to Veranda Write's own clipboard (a chunked
   buffer in RAM banks: the bytes with their style codes, a code for the
   style at the start first). A paste takes the own clipboard while the
   Text Scrap is the one this program wrote (scrap_stamp), else the Text
   Scrap's text (its escapes dropped) in the typing style.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <scrap.h>
#include "cwv.h"
#pragma code-name ("OVERLAY7")
#pragma rodata-name ("OVL7RO")

/*---------------------------------------------------------------
  the clipboard
 ---------------------------------------------------------------*/
char scrap_path[40];                    /* (the scrap library's: SCRAP_PATH) */
static unsigned char cl_bank[40];
static unsigned int cl_len[40];
static struct cbuf clip;
static unsigned char inited, stamp[8], have_stamp;
#define MAXSCRAP 60000U

static unsigned char plain(unsigned char c)
{
    return IS_CHAR(c) || c == CH_CR;
}

void clip_copy(void)
{
    pos_t lo = SEL_LO(), hi = SEL_HI(), p;
    unsigned int n, i, o, total = 4;
    unsigned char e, k;
    static const unsigned char cardset[4] = { SCRAP_NEWCARDSET, 9, 0, 0 };
    unsigned char code[3];
    if (!HAS_SEL())
        return;
    if (!inited) {
        cb_init(&clip, cl_bank, cl_len, sizeof cl_bank);
        inited = 1;
    }
    /* its own: the style at the start, then the bytes */
    cb_clear(&clip);
    code[0] = code[2] = K_CS;
    code[1] = style_at(lo);
    e = cb_append(&clip, code, 3, BANK_SIZE);
    for (p = lo; p < hi && !e; p += n) {
        n = cb_read(&tx, p, iobuf, hi - p > IOBUF ? IOBUF : (unsigned int)(hi - p));
        e = cb_append(&clip, iobuf, n, BANK_SIZE);
    }
    if (e) {
        cb_clear(&clip);
        message("There is not enough memory", "to copy so much.");
        return;
    }
    /* the Text Scrap: the characters (codes left out, the first 60000) */
    k = 0;                              /* (a code's bytes to skip) */
    for (p = lo; p < hi; p += n) {
        n = cb_read(&tx, p, iobuf, hi - p > IOBUF ? IOBUF : (unsigned int)(hi - p));
        for (i = 0; i < n && total < MAXSCRAP; ++i) {
            if (k) { --k; continue; }
            if (IS_KIND(iobuf[i])) { k = 2; continue; }
            ++total;
        }
    }
    e = scrap_text_begin(total);
    if (!e)
        e = scrap_write(cardset, 4);
    total -= 4;
    k = 0;
    for (p = lo; p < hi && !e && total; p += n) {
        n = cb_read(&tx, p, iobuf, hi - p > IOBUF ? IOBUF : (unsigned int)(hi - p));
        for (i = o = 0; i < n && o < total; ++i) {
            if (k) { --k; continue; }
            if (IS_KIND(iobuf[i])) { k = 2; continue; }
            iobuf[o++] = iobuf[i];
        }
        total -= o;
        e = scrap_write(iobuf, o);
    }
    if (scrap_close() && !e)
        e = 1;
    have_stamp = !e && !scrap_stamp(SCRAP_TEXT, stamp);
    if (e)
        set_status("Copied (not to the Text Scrap)");
    else
        set_status("Copied");
}

void clip_cut(void)
{
    pos_t lo = SEL_LO(), hi = SEL_HI();
    if (!HAS_SEL())
        return;
    clip_copy();
    ul_step(U_CMD, cur, anc);
    cur = anc = doc_delete(lo, hi);
    ui_moved();
}

/* the selection replaced by bytes (codes and all), in pieces: the
   document's primitives, one layout at the end */
static pos_t at, start;
static unsigned char sp, bad;

static void raw_begin(void)
{
    pos_t lo = SEL_LO(), hi = SEL_HI();
    ul_step(U_CMD, cur, anc);
    at = start = cur = anc = doc_delete(lo, hi);
    sp = style_at(at);
    bad = 0;
}

static void raw_add(unsigned char *b, unsigned int n)
{
    if (bad || !n)
        return;
    if (tx_ins(at, b, n))
        bad = 1;
    else
        at += n;
}

static void raw_end(void)
{
    static unsigned char code[3] = { K_CS, 0, K_CS };
    code[1] = sp;
    if (at < DOC_LEN)
        tx_ins(at, code, 3);
    lay_fix();
    tp_a = start;
    tp_b = at;
    doc_tidy(start, at + 3);
    lay_fix();
    cur = anc = pos_norm(tp_b);
    if (bad) {
        doc_err = DE_MEM;
        message("There is not enough memory", "for all of it: pasted in part.");
    }
    ui_moved();
}

void clip_paste(void)
{
    unsigned char st[8], c, skip = 0;
    unsigned int n, i, o, left = 0;
    pos_t p;
    if (inited && clip.total && have_stamp && !scrap_stamp(SCRAP_TEXT, st) && !memcmp(st, stamp, 8)) {
        raw_begin();
        for (p = 0; p < clip.total && !bad; p += n) {
            n = cb_read(&clip, p, iobuf, IOBUF);
            raw_add(iobuf, n);
        }
        raw_end();
        return;
    }
    /* another program's text: its characters, in the typing style */
    if (scrap_open(SCRAP_TEXT) || scrap_read(&left, 2) != 2 || !left) {
        scrap_close();
        set_status("The clipboard is empty");
        return;
    }
    if (HAS_SEL())
        ty_style = style_typing(SEL_LO());
    c = ty_style;
    raw_begin();
    iobuf[0] = iobuf[2] = K_CS;
    iobuf[1] = c;
    raw_add(iobuf, 3);
    while (left && !bad) {
        n = scrap_read(iobuf, left > IOBUF ? IOBUF : left);
        if (!n)
            break;
        left -= n;
        for (i = o = 0; i < n; ++i) {
            c = iobuf[i];
            if (skip) {
                --skip;
                continue;
            }
            switch (c) {
            case SCRAP_NEWCARDSET: skip = 3; continue;
            case SCRAP_ESC_RULER: skip = 26; continue;
            case SCRAP_ESC_GRAPHICS: skip = 4; continue;
            case CR: case SCRAP_PAGE_BREAK: c = CH_CR; break;
            case 10: c = CH_CR; break;
            }
            if (plain(c))
                iobuf[o++] = c;
        }
        raw_add(iobuf, o);
    }
    scrap_close();
    raw_end();
}
