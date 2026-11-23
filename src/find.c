/* Veranda Write: find, replace all, the document's counts (overlay 4
   on the X16, with word wrap).

   The text is read a window at a time (cb_read into iobuf); fs_scan
   finds the next byte that may start a match (the pattern's first
   character, folded when case does not matter), passing over codes and
   keeping the character style they set (fs_sty); a candidate is then
   compared with the rest of the pattern, codes inside the text passed
   over (a match may span a style change), in the window or, at its end,
   byte by byte. fs_scan is v/fscan.s on the X16 (27 cycles a byte; the
   window ends with the pattern's first character, a sentinel), the C
   below on the host.

   Replace all replaces every match from the start in one step of the
   text's primitives (the caller's one undo step): each match's bytes out,
   the replacement in, in the style of the match's first character, and a
   style code after it when the match ended in another style (and no code
   follows; with no replacement the code before it takes that style), so
   that no code that does nothing is left; the layout is mended once at
   the end (a pass of doc_tidy over the range cost more than the rest:
   69 s for 624 matches in 140 KB).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CW_HOST
#include "v/ovl4.h"
#pragma bss-name ("BSS")                /* (its variables in main RAM: the commands read them,
                                           and tx_ins its buffer) */
#pragma data-name (push, "OVL4DATA")
#endif
#include <string.h>
#include "cw.h"

unsigned char fd_fold[256] = { 0 };     /* (overlay 4's: only fscan.s and this read it) */
#ifndef CW_HOST
#pragma data-name (pop)
#endif
unsigned char fd_pat[FD_MAX + 1], fd_len, fd_flags;
pos_t fd_lo, fd_hi;
unsigned char fd_sa, fd_sb;
unsigned int fs_i, fs_n;
unsigned char fs_skip, fs_sty, fs_c, fs_prev;

static unsigned char word_ch(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'
           || (c >= 0xC0 && c != 0xD7 && c != 0xF7);
}

void find_set(const char *s, unsigned char flags)
{
    unsigned int c;
    for (c = 0; c < 256; ++c)
        fd_fold[c] = (unsigned char)c;
    if (!(flags & FF_CASE))
        for (c = 0; c < 256; ++c)
            if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7))
                fd_fold[c] = (unsigned char)(c + 32);
    fd_len = 0;
    while (*s && fd_len < FD_MAX)
        fd_pat[fd_len++] = fd_fold[(unsigned char)*s++];
    fd_flags = flags;
}

#ifndef CW_HOST
unsigned char fs_scan(void);            /* v/fscan.s */
#else
unsigned char fs_scan(void)
{
    unsigned char c;
    while (fs_i < fs_n) {
        c = iobuf[fs_i];
        if (fs_skip) {
            if (fs_skip == 2 && fs_c == K_CS) {
                fs_prev = fs_sty;       /* (the argument of a style code) */
                fs_sty = c;
            }
            --fs_skip;
        } else if (IS_KIND(c)) {
            fs_skip = 2;
            fs_c = c;
        } else if (fd_fold[c] == fd_pat[0])
            return 1;
        ++fs_i;
    }
    return 0;
}
#endif

/* the rest of the pattern after its first character at p (iobuf[i],
   k bytes there); 1: a match: fd_lo, fd_hi, fd_sb */
static unsigned char verify(pos_t p, unsigned int i, unsigned int k)
{
    static unsigned char j, c, s;
    static pos_t q;
    j = 1;
    s = fs_sty;
    q = p + 1;
    ++i;
    while (j < fd_len) {
        if (i < k)
            c = iobuf[i];
        else if (q < tx.total)
            c = cb_byte(&tx, q);
        else
            return 0;
        if (IS_KIND(c)) {
            if (c == K_CS)
                s = i + 1 < k ? iobuf[i + 1] : cb_byte(&tx, q + 1);
            else if (c == K_PS)
                return 0;               /* (not across paragraphs) */
            i += 3;
            q += 3;
            continue;
        }
        if (fd_fold[c] != fd_pat[j])
            return 0;
        ++j;
        ++i;
        ++q;
    }
    fd_lo = p;
    fd_hi = q;
    fd_sa = fs_sty;
    fd_sb = s;
    if (fd_flags & FF_WORD) {
        q = pos_prev(p);                /* (the text's first character: itself) */
        if (q < p && word_ch(cb_byte(&tx, q)))
            return 0;
        q = pos_norm(fd_hi);
        if (q < tx.total && word_ch(cb_byte(&tx, q)))
            return 0;
    }
    return 1;
}

/* the first match in [p, end) (p not inside a code; sty the style
   there) */
static unsigned char scan(pos_t p, pos_t end, unsigned char sty)
{
    static unsigned int k;
    fs_skip = 0;
    fs_sty = sty;
    while (p < end) {
        k = cb_read(&tx, p, iobuf, end - p > IOBUF - 1 ? IOBUF - 1 : (unsigned int)(end - p));
        if (!k)
            break;
        iobuf[k] = fd_pat[0];           /* (fs_scan's sentinel) */
        fs_i = 0;
        fs_n = k;
        while (fs_scan()) {
            if (verify(p + fs_i, fs_i, k) && fd_hi <= end)
                return 1;
            ++fs_i;
        }
        p += k;
    }
    return 0;
}

unsigned char find_from(pos_t p, pos_t end)
{
    if (ld_kind) {                      /* (opening: the rest read first) */
        fd_lo = tx.total;
        ld_need(0xFFFFFFFFUL);
        if (end == fd_lo)
            end = tx.total;
    }
    if (!fd_len)
        return 0;
    return scan(p, end, 0);
}

unsigned char find_here(pos_t lo, pos_t hi)
{
    return fd_len && scan(lo, hi, 0) && fd_lo == lo && pos_norm(fd_hi) == pos_norm(hi);
}

/* the code [K_CS a K_CS] at q out, or its argument made a; the place
   after the replacement moves with it */
static pos_t ra_p;

static void code_del(pos_t q)
{
    tx_del(q, 3);
    if (ra_p > q)
        ra_p -= 3;
}

unsigned int replace_all(const unsigned char *r, unsigned char rn)
{
    static unsigned char b[FD_MAX + 3];
    static unsigned int n;
    static unsigned char k, c1, c2, a2;
    static pos_t lo;
    if (ld_kind)
        ld_need(0xFFFFFFFFUL);    /* (all of it: the rest read first) */
    n = 0;
    ra_p = 0;
    doc_err = 0;
    fs_sty = fs_prev = 0;
    if (!fd_len)
        return 0;
    memcpy(b, r, rn);
    while (scan(ra_p, tx.total, fs_sty)) {
        lo = fd_lo;
        tx_del(lo, fd_hi - lo);
        /* the codes around: C1 straight before (fs_prev the style before
           it), C2 straight after; none is left doing nothing */
        c1 = lo >= 3 && cb_byte(&tx, lo - 1) == K_CS;
        c2 = lo < tx.total && cb_byte(&tx, lo) == K_CS;
        a2 = c2 ? cb_byte(&tx, lo + 1) : 0;
        /* then: ra_p where the scan goes on, fs_sty the style in effect
           there (before any code there), fs_prev the one before a code
           straight before it */
        k = rn;
        ra_p = lo;
        fs_sty = fd_sa;
        if (rn) {
            if (c2) {
                if (a2 == fd_sa)
                    code_del(lo);       /* (C2 now sets the replacement's style) */
            } else if (fd_sb != fd_sa && lo < tx.total) {
                b[k++] = K_CS;          /* the match ended in another style */
                b[k++] = fd_sb;
                b[k++] = K_CS;
            }
            if (tx_ins(lo, b, k))
                break;                  /* (no room: doc_err) */
            ra_p = lo + k;
            if (k > rn) {
                fs_prev = fd_sa;
                fs_sty = fd_sb;
            }
        } else if (lo >= tx.total) {
            if (c1)
                code_del(lo - 3);       /* (C1 at the very end now) */
        } else if (c2) {
            if (c1) {
                code_del(lo - 3);       /* (C1 straight before C2) */
                fs_sty = fs_prev;
                if (a2 == fs_prev)
                    code_del(lo - 3);
            } else if (a2 == fd_sa)
                code_del(lo);
        } else if (fd_sb != fd_sa) {
            if (c1) {
                if (fd_sb == fs_prev)
                    code_del(lo - 3);   /* (fs_sty: the style before it, sb) */
                else
                    tx_set(lo - 2, fd_sb);
                fs_sty = fd_sb;
            } else {
                b[0] = K_CS;
                b[1] = fd_sb;
                b[2] = K_CS;
                if (tx_ins(lo, b, 3))
                    break;
                ra_p = lo + 3;
                fs_prev = fd_sa;
                fs_sty = fd_sb;
            }
        }
        ++n;
    }
    if (n)
        lay_fix();
    return n;
}

/*---------------------------------------------------------------
  the counts: words (runs of characters between spaces, tabs and
  paragraph ends), characters (all but paragraph ends), with no spaces
  or tabs, paragraphs (paragraph ends + 1)
 ---------------------------------------------------------------*/
unsigned long cn_words, cn_chars, cn_nonsp, cn_paras;

void doc_count(pos_t lo, pos_t hi)
{
    static unsigned int k, i;
    static unsigned char c, skip, in;
    if (ld_kind) {                      /* (opening: the rest read first) */
        fd_lo = tx.total;
        ld_need(0xFFFFFFFFUL);
        if (hi == fd_lo)
            hi = tx.total;
    }
    cn_words = cn_chars = cn_nonsp = 0;
    cn_paras = 1;
    skip = in = 0;
    while (lo < hi) {
        k = cb_read(&tx, lo, iobuf, hi - lo > IOBUF ? IOBUF : (unsigned int)(hi - lo));
        if (!k)
            break;
        for (i = 0; i < k; ++i) {
            c = iobuf[i];
            if (skip) {
                --skip;
                continue;
            }
            if (IS_KIND(c)) {
                skip = 2;
                continue;
            }
            if (c == CH_CR) {
                ++cn_paras;
                in = 0;
                continue;
            }
            ++cn_chars;
            if (c == ' ' || c == CH_TAB) {
                in = 0;
                continue;
            }
            ++cn_nonsp;
            if (!in) {
                ++cn_words;
                in = 1;
            }
        }
        lo += k;
    }
}
