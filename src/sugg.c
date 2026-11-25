/* Veranda Write: spelling suggestions (overlay 24 on the X16).

   Writer's suggest.c (the user's own earlier project, ~/Projects/Writer
   src/spell/suggest.c), taken over: bounded, never a scan of the
   dictionary; candidates are made from the word and each is looked up,
   the likeliest first --
     1. the same letters with Polish letters put in or changed (the
        letters that share a plain letter: a/ą, c/ć, e/ę, l/ł, n/ń,
        o/ó, s/ś, z/ź/ż), for text typed without them ("zolc" ->
        "żółć"); Polish only;
     2. the language's confusions, one for the other (Polish: rz/ż,
        ch/h, u/ó, om/ą, on/ą, em/ę, en/ę; English: ie/ei, ant/ent, ...,
        and an apostrophe left out: "dont" -> "don't");
     3. two neighbouring letters swapped ("recieve" -> "receive");
     4. a letter left out; 5. a letter changed; 6. a letter added
        (the language's alphabet);
   stopping at SP_NSUGG found, LOOKUP_BUDGET lookups or, on the X16,
   SUGG_TIME jiffies (the dialog shows what was found by then). Writer
   took its alphabet, its confusions and the letters' plain forms from
   its language pack; Write has two languages and the Veranda set, so
   they are tables here.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).
   The method and its order: Writer's (the user's), taken over. */
#include <string.h>
#ifndef CW_HOST
#include "v/ovl24.h"                  /* (first: its names for spell.h's) */
#endif
#include "spell.h"
#ifndef CW_HOST
unsigned char cw_jiffies(void);
#endif

#define LOOKUP_BUDGET 1200
#define SUGG_TIME     150               /* 2.5 s */
#define MAXW          SP_MAXW

static const unsigned char al_pl[32] = {
    0x61, 0xA2, 0x62, 0x63, 0xE5, 0x64, 0x65, 0xFD, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0xB3,
    0x6D, 0x6E, 0xF1, 0x6F, 0xF3, 0x70, 0x72, 0x73, 0xF7, 0x74, 0x75, 0x77, 0x79, 0x7A, 0xAE, 0xBF
};
static const unsigned char al_en[27] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u',
    'v', 'w', 'x', 'y', 'z', '\''
};
/* the confusions: length, bytes, length, bytes ...; 0 the end */
static const unsigned char cf_pl[] = {
    2, 'r', 'z', 1, 0xBF,  2, 'c', 'h', 1, 'h',  1, 'u', 1, 0xF3,  2, 'o', 'm', 1, 0xA2,
    2, 'o', 'n', 1, 0xA2,  2, 'e', 'm', 1, 0xFD,  2, 'e', 'n', 1, 0xFD,  0
};
static const unsigned char cf_en[] = {
    2, 'i', 'e', 2, 'e', 'i',  3, 'a', 'n', 't', 3, 'e', 'n', 't',  3, 'a', 'n', 'c', 3, 'e', 'n', 'c',
    2, 'e', 'r', 2, 'o', 'r',  2, 'c', 'e', 2, 's', 'e',  2, 'f', 'f', 1, 'f',  2, 'l', 'l', 1, 'l',
    2, 's', 's', 1, 's',  2, 'r', 'r', 1, 'r',  2, 'm', 'm', 1, 'm',  2, 'p', 'h', 1, 'f',  0
};
/* Polish: the letters that share a plain one (Writer: the pack's
   transliteration), the plain one first */
static const unsigned char sib[][4] = {
    { 'a', 0xA2 }, { 'c', 0xE5 }, { 'e', 0xFD }, { 'l', 0xB3 }, { 'n', 0xF1 },
    { 'o', 0xF3 }, { 's', 0xF7 }, { 'z', 0xAE, 0xBF }
};

static unsigned char sugg[SP_NSUGG][MAXW + 1];
static unsigned char nsugg, clen, wlen, nalpha, stop;
static unsigned char cand[MAXW + 2], lw[MAXW + 1];
static const unsigned char *alpha;
static unsigned int budget;
#ifndef CW_HOST
static unsigned char t0;
#endif

static void try_cand(void)
{
    static unsigned char i;
    if (stop || clen == 0 || clen > MAXW)
        return;
    if (clen == wlen && !memcmp(cand, lw, clen))
        return;
    for (i = 0; i < nsugg; ++i)
        if (strlen((const char *)sugg[i]) == clen && !memcmp(sugg[i], cand, clen))
            return;
    if (!budget--) {
        stop = 1;
        return;
    }
#ifndef CW_HOST
    if ((unsigned char)(cw_jiffies() - t0) >= SUGG_TIME) {
        stop = 1;
        return;
    }
#endif
    memcpy(sp_lw, cand, clen);
    sp_wl = clen;
    if (sp_look() == SP_KNOWN) {
        memcpy(sugg[nsugg], cand, clen);
        sugg[nsugg][clen] = 0;
        if (++nsugg == SP_NSUGG)
            stop = 1;
    }
}

/* the letters c can be (c first) */
static unsigned char FASTCALL siblings(unsigned char c, unsigned char *out)
{
    static unsigned char i, j;
    out[0] = c;
    for (i = 0; i < sizeof sib / sizeof sib[0]; ++i)
        for (j = 0; j < 4 && sib[i][j]; ++j)
            if (sib[i][j] == c) {
                out[0] = sib[i][0];
                for (j = 1; j < 4 && sib[i][j]; ++j)
                    out[j] = sib[i][j];
                /* (c first: the word as it is is combination 0) */
                for (i = 0; i < j; ++i)
                    if (out[i] == c) {
                        out[i] = out[0];
                        out[0] = c;
                    }
                return j;
            }
    return 1;
}

static void diacritics(void)
{
    static unsigned char opt[MAXW][4], nopt[MAXW], digit[MAXW];
    static unsigned char i;
    static unsigned int combos;
    combos = 1;
    for (i = 0; i < wlen; ++i) {
        nopt[i] = siblings(lw[i], opt[i]);
        digit[i] = 0;
        if (combos < 512)
            combos *= nopt[i];
    }
    if (combos > 512)
        combos = 512;                   /* (bounded: the first 512) */
    clen = wlen;
    while (--combos && !stop) {
        for (i = 0; i < wlen; ++i) {    /* the next: a counter of mixed radix */
            if (++digit[i] < nopt[i])
                break;
            digit[i] = 0;
        }
        for (i = 0; i < wlen; ++i)
            cand[i] = opt[i][digit[i]];
        try_cand();
    }
}

static void FASTCALL swap(const unsigned char *a, unsigned char la, const unsigned char *b, unsigned char lb)
{
    static unsigned char i;
    for (i = 0; i + la <= wlen && !stop; ++i)
        if (!memcmp(lw + i, a, la) && wlen - la + lb <= MAXW) {
            memcpy(cand, lw, i);
            memcpy(cand + i, b, lb);
            memcpy(cand + i + lb, lw + i + la, wlen - i - la);
            clen = wlen - la + lb;
            try_cand();
        }
}

static void FASTCALL confusions(const unsigned char *p)
{
    static const unsigned char *a, *b;
    static unsigned char la, lb;
    while ((la = *p) != 0) {
        a = p + 1;
        lb = a[la];
        b = a + la + 1;
        p = b + lb;
        swap(a, la, b, lb);
        swap(b, lb, a, la);
    }
}

/* (English) an apostrophe left out: "dont" -> "don't" */
static void apostrophe(void)
{
    static unsigned char i;
    for (i = 1; i < wlen && wlen < MAXW; ++i) {
        memcpy(cand, lw, i);
        cand[i] = '\'';
        memcpy(cand + i + 1, lw + i, wlen - i);
        clen = wlen + 1;
        try_cand();
    }
}

static void edits(void)
{
    static unsigned char i, j;
    for (i = 0; i + 1 < wlen; ++i) {    /* transpositions */
        memcpy(cand, lw, wlen);
        cand[i] = lw[i + 1];
        cand[i + 1] = lw[i];
        clen = wlen;
        try_cand();
    }
    for (i = 0; i < wlen; ++i) {        /* deletions */
        memcpy(cand, lw, i);
        memcpy(cand + i, lw + i + 1, wlen - i - 1);
        clen = wlen - 1;
        try_cand();
    }
    for (i = 0; i < wlen && !stop; ++i) /* substitutions */
        for (j = 0; j < nalpha; ++j) {
            if (alpha[j] == lw[i])
                continue;
            memcpy(cand, lw, wlen);
            cand[i] = alpha[j];
            clen = wlen;
            try_cand();
        }
    if (wlen < MAXW)
        for (i = 0; i <= wlen && !stop; ++i)    /* insertions */
            for (j = 0; j < nalpha; ++j) {
                memcpy(cand, lw, i);
                cand[i] = alpha[j];
                memcpy(cand + i + 1, lw + i, wlen - i);
                clen = wlen + 1;
                try_cand();
            }
}

void sp_suggest(void)
{
    static unsigned char i;
    wlen = sp_wl;
    memcpy(lw, sp_lw, wlen);
    nsugg = stop = 0;
    budget = LOOKUP_BUDGET;
#ifndef CW_HOST
    t0 = cw_jiffies();
#endif
    if (fmt_lang == LANG_PL) {
        alpha = al_pl;
        nalpha = sizeof al_pl;
        diacritics();
        confusions(cf_pl);
    } else {
        alpha = al_en;
        nalpha = sizeof al_en;
        confusions(cf_en);
        apostrophe();
    }
    edits();
    sp_end();
    /* for the panel and the pop-up: the lists' bank (through main RAM) */
    sp_lw[0] = nsugg;
    if (sp_xb) {
        sp_wr(sp_xb, SP_SUGG, sp_lw, 1);
        for (i = 0; i < nsugg; ++i) {
            memcpy(sp_lw, sugg[i], MAXW + 1);
            sp_wr(sp_xb, SP_SUGG + 1 + (i << 5), sp_lw, MAXW + 1);
        }
    }
    memcpy(sp_lw, lw, wlen);             /* (the word again) */
    sp_wl = wlen;
}

#ifdef CW_HOST
unsigned char sp_sugg_get(unsigned char i, char *out)
{
    if (i >= nsugg)
        return 0;
    strcpy(out, (const char *)sugg[i]);
    return 1;
}
#endif
