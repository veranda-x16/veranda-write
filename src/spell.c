/* Veranda Write: spelling's dictionary, its cache, the hash of results,
   the user's and the ignored words (overlay 23 on the X16; spell.h says
   what is where and what was Writer's).

   A lookup (sp_check): the hash first (a word met before: no lookup at
   all), then the user's and the ignored words, then the dictionary: the
   top index (the first key of each key page, in the lists' bank) is
   searched for the last key page whose key is below the word, that page
   (a unit of the file: cached, or one 1 KB read) for the last block
   whose key is below it, that block (cached, or one more read) scanned;
   keys are words cut to 16 bytes, so a run of blocks can share one and
   the walk goes on into the next block while its key equals the word's
   (Writer's dict.c, step for step). The cache: 1 KB slots in RAM banks,
   8 a bank, the least recently used given up; a unit's slot is found
   through a table of every unit (the hash bank's last 2 KB), not a
   search. The file is opened when a unit must be read and stays open
   until the operation's end (sp_end); a unit that follows the last one
   read needs no seek.

   The hash: 512 sets of four entries (a 16-bit tag, the word's length,
   known or unknown), the word's own two 16-bit sums; the most recent
   first in its set (a word met again moves to the front), a new result
   pushes the last known word out, never an unknown one while there is a
   known one (the marks on the screen depend on them).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).
   The lookup and the word lists: Writer's (the user's own earlier
   project, ~/Projects/Writer src/spell/dict.c, dict_list.c), taken over. */
#include <string.h>
#ifndef CW_HOST
#include "v/ovl23.h"                  /* (first: its names for spell.h's) */
#endif
#include "spell.h"

/* the Veranda set's letters (ISO-8859-16): lower case, upper case */
const unsigned char sp_lc[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xA2, 0xA2, 0xB3, 0x00, 0x00, 0xA8, 0x00, 0xA8, 0x00, 0xBA, 0x00, 0xAE, 0x00, 0xAE, 0xBF,
    0x00, 0x00, 0xB9, 0xB3, 0xB8, 0x00, 0x00, 0x00, 0xB8, 0xB9, 0xBA, 0x00, 0xBD, 0xBD, 0xFF, 0xBF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};
const unsigned char sp_uc[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA1, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA6, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAC, 0xAF,
    0xB0, 0xB1, 0xB2, 0xA3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB4, 0xB2, 0xAA, 0xBB, 0xBC, 0xBC, 0xBE, 0xAF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xBE,
};

/* (initialised: the X16's overlay BSS is not cleared) */
static unsigned char hb = 0;            /* the hash's bank */
static unsigned char nslot = 0;
#define ncb sp_ncb                       /* (main RAM: fonts.c reads it) */
static unsigned char dlang = 0;         /* the language whose dictionary is read (0 none) */
static unsigned char dmiss = 0;         /* the language whose dictionary is not there */
static unsigned char fopen_ = 0;        /* its file is open */
static unsigned char cb[SP_CBMAX];      /* the cache's banks (the document's pool's) */
static unsigned int s_unit[SP_CBMAX * 8], s_age[SP_CBMAX * 8], age;
static unsigned long fpos;              /* where the file is (0xFFFFFFFF: not known) */
static unsigned int nblocks, npages, keys_unit, data_unit;
#ifdef CW_HOST
static unsigned int h1, h2, hset;
#else
void __fastcall__ LogText(void *text);
extern unsigned int h1, h2, hset;       /* (v/sphash.s) */
void sp_hash(void);
#define hash() sp_hash()
#endif
#define HGET() sp_rd(hb, hset, sp_buf, 12)
#define HPUT() sp_wr(hb, hset, sp_buf, 12)
#ifdef CW_HOST
unsigned char sp_buf[24];
unsigned char sp_xb, sp_tflags, sp_ncb, sp_busy;   /* (the X16's: main RAM, v/spell.s) */
#endif
unsigned char sp_err = 0;               /* (overlay 23's on the X16) */
#ifdef CW_HOST
unsigned int sp_ulen, sp_ilen;
#endif

#define SLOT_BANK(s) (cb[(s) >> 3])
#define SLOT_OFF(s)  ((unsigned int)((s) & 7) << 10)
#define NO_SLOT 0xFF

/* the case of w: 0 lower (or mixed), 1 its first letter a capital, 2 all */
unsigned char FASTCALL sp_case(const unsigned char *w, unsigned char n)
{
    static unsigned char i, up, c;
    up = 0;
    for (i = 0; i < n; ++i) {
        c = w[i];
        if (sp_lc[c] && sp_lc[c] != c)
            ++up;
    }
    if (up == n && n > 1)
        return 2;
    c = w[0];
    return sp_lc[c] && sp_lc[c] != c ? 1 : 0;
}

/*---------------------------------------------------------------
  the words of the text (Writer's spell.c's next_word, for Write's
  text: its style codes inside a word passed over). A word: letters
  and digits, an apostrophe between letters ("don't"); a hyphen ends
  one ("well-known": two). Checked: two letters or more, no digit, at
  most 31 bytes; all in capitals and at most 5 letters: an
  abbreviation (sp_abbr), not marked. Read a window at a time (lb:
  128 bytes, main RAM on the X16: cb_read writes it)
 ---------------------------------------------------------------*/
#ifdef CW_HOST
static unsigned char lb[128];
#else
extern unsigned char lb[128];           /* (view.c's) */
#endif
static pos_t tk_end, wa;

#ifdef CW_HOST
static unsigned char wi, wn;

static void FASTCALL fill(pos_t q)
{
    wa = q;
    wi = 0;
    wn = (unsigned char)cb_read(&tx, q, lb, tk_end - q > sizeof lb ? sizeof lb : (unsigned int)(tk_end - q));
}

void FASTCALL sp_tokat(pos_t from, pos_t to)
{
    tk_end = to > DOC_LEN ? DOC_LEN : to;
    if (from > tk_end)
        from = tk_end;
    fill(from);
}

/* where the scan is */
pos_t sp_tokpos(void)
{
    return wa + wi;
}

unsigned char sp_tok(void)
{
    static unsigned char c, lc, n, dig;
    static unsigned int nb, last;
    static pos_t ws;
    for (;;) {
        for (;;) {                      /* a word's start */
            if (wi >= wn) {
                if (!wn || wa + wn >= tk_end)
                    return 0;
                fill(wa + wi);
                continue;
            }
            c = lb[wi];
            if (IS_KIND(c)) {
                wi += 3;
                continue;
            }
            if (sp_lc[c] || (unsigned char)(c - '0') < 10)
                break;
            ++wi;
        }
        ws = wa + wi;
        n = dig = 0;
        nb = last = 0;
        for (;;) {
            if (wi >= wn) {
                if (!wn || wa + wn >= tk_end)
                    break;
                fill(wa + wi);
                continue;
            }
            c = lb[wi];
            if (IS_KIND(c)) {
                wi += 3;
                nb += 3;
                continue;
            }
            lc = sp_lc[c];
            if (!lc) {
                if ((unsigned char)(c - '0') < 10) {
                    dig = 1;
                    lc = c;
                } else if (c == '\'' && n) {
                    if (wi + 1 >= wn && wa + wn < tk_end)
                        fill(wa + wi);  /* (the next character in the window) */
                    if (wi + 1 >= wn || !sp_lc[lb[wi + 1]])
                        break;
                    lc = c;
                } else
                    break;
            }
            if (n < SP_MAXW) {
                sp_word[n] = c;
                sp_lw[n] = lc;
            }
            ++n;
            ++wi;
            last = ++nb;
        }
        if (n >= 2 && n <= SP_MAXW && !dig) {
            sp_wl = n;
            sp_ws = ws;
            sp_we = ws + last;
            return 1;
        }
    }
}
#else
/* the X16's: the bytes in v/sphash.s's sp_scanw, the windows and the
   places here */
extern unsigned char tk_wi, tk_wn, tk_mode, tk_n, tk_dig, tk_more;
extern unsigned int tk_nb, tk_last, tk_wso, tk_base;
unsigned char sp_scanw(void);
static pos_t tk_from;

static void FASTCALL fill(pos_t q)
{
    static unsigned int n;
    wa = q;
    n = tk_end - q > sizeof lb ? sizeof lb : (unsigned int)(tk_end - q);
    tk_wn = (unsigned char)cb_read(&tx, q, lb, n);
    tk_wi = 0;
    tk_base = (unsigned int)(q - tk_from);
    tk_more = q + tk_wn < tk_end;
}

void FASTCALL sp_tokat(pos_t from, pos_t to)
{
    tk_end = to > DOC_LEN ? DOC_LEN : to;
    if (from > tk_end)
        from = tk_end;
    tk_from = from;
    tk_mode = 0;
    fill(from);
}

pos_t sp_tokpos(void)
{
    return wa + tk_wi;
}

unsigned char sp_tok(void)
{
    static unsigned char r;
    for (;;) {
        r = sp_scanw();
        if (r != 1) {
            if (tk_wn && tk_more) {
                fill(wa + tk_wi);       /* (the next window; an apostrophe read again with what follows) */
                continue;
            }
            if (!tk_mode)
                return 0;
            tk_mode = 0;                /* (a word to the end) */
        }
        if (tk_n >= 2 && tk_n <= SP_MAXW && !tk_dig) {
            sp_wl = tk_n;
            sp_ws = tk_from + tk_wso;
            sp_we = sp_ws + tk_last;
            return 1;
        }
    }
}
#endif

/* an abbreviation: all capitals, at most 5 letters (NATO, PKP) */
unsigned char sp_abbr(void)
{
    return sp_wl <= 5 && sp_case(sp_word, sp_wl) == 2;
}

/*---------------------------------------------------------------
  the hash of results
 ---------------------------------------------------------------*/
#ifdef CW_HOST
/* (the X16's: v/sphash.s, the same sums) */
static void hash(void)
{
    static unsigned char i, c;
    h1 = sp_wl;
    h2 = (unsigned int)sp_wl * 7;
    for (i = 0; i < sp_wl; ++i) {
        c = sp_lw[i];
        h1 = (unsigned int)((h1 << 5) + h1 + c);
        h2 = (unsigned int)(((h2 << 3) | (h2 >> 13)) ^ c);
    }
    hset = ((h1 ^ (h1 >> 9)) & (SP_HSETS - 1)) * 12;
}
#endif

/* the entry of the set in sp_buf (4 of 3 bytes, the most recent first)
   that is this word's: its index, 4 none */
static unsigned char find_e(void)
{
    static unsigned char k, *e;
    for (k = 0, e = sp_buf; k < SP_WAYS; ++k, e += 3)
        if ((e[2] & 0x80) && (e[2] & 0x1F) == sp_wl && e[0] == (unsigned char)h2 &&
            e[1] == (unsigned char)(h2 >> 8))
            return k;
    return SP_WAYS;
}

/* entry k moved to the front, the ones before it one back */
static void FASTCALL front(unsigned char k)
{
    static unsigned char t[3];
    memcpy(t, sp_buf + k * 3, 3);
    memmove(sp_buf + 3, sp_buf, k * 3);
    memcpy(sp_buf, t, 3);
}

unsigned char sp_probe(void)
{
    static unsigned char k;
    hash();
    if (!hb || fmt_lang != dlang || (sp_tflags & 1))
        return SP_NONE;
    HGET();
    k = find_e();
    if (k == SP_WAYS)
        return SP_NONE;
    if (k) {
        front(k);                       /* (a word met again stays: W59) */
        HPUT();
    }
    return sp_buf[2] & 0x40 ? SP_UNKNOWN : SP_KNOWN;
}

/* (hash() first: sp_probe's, or the caller's) the result first in its
   set; the entry that goes is the last known word's (an unknown word's
   mark on the screen depends on it), the last one if all are unknown */
static void FASTCALL hash_put(unsigned char r)
{
    static unsigned char k;
    if (!hb || (sp_tflags & 1))
        return;
    HGET();
    k = find_e();
    if (k == SP_WAYS)
        for (k = SP_WAYS - 1; k && (sp_buf[k * 3 + 2] & 0xC0) == 0xC0; --k)
            ;
    if (!k && (sp_buf[2] & 0xC0) == 0xC0)
        k = SP_WAYS - 1;                /* (all unknown: the oldest) */
    front(k);
    sp_buf[0] = (unsigned char)h2;
    sp_buf[1] = (unsigned char)(h2 >> 8);
    sp_buf[2] = 0x80 | sp_wl | (r == SP_UNKNOWN ? 0x40 : 0);
    HPUT();
}

/* n bytes of bank b from off made 0 */
static void FASTCALL clear(unsigned char b, unsigned int off, unsigned int n)
{
    static unsigned char k;
    memset(sp_buf, 0, sizeof sp_buf);
    for (; n; n -= k, off += k) {
        k = n > sizeof sp_buf ? sizeof sp_buf : (unsigned char)n;
        sp_wr(b, off, sp_buf, k);
    }
}

void sp_flush(void)
{
    if (hb)
        clear(hb, 0, SP_UMAP);
}

/*---------------------------------------------------------------
  the word lists (Writer's dict_list.c): NUL-terminated words, one
  after another, in the lists' bank
 ---------------------------------------------------------------*/
static unsigned char FASTCALL list_has(unsigned int at, unsigned int used)
{
    static unsigned int k;
    static unsigned char i, n, m, c;
    m = 0;
    for (k = 0; k < used; k += n) {
        n = used - k > sizeof sp_buf ? sizeof sp_buf : (unsigned char)(used - k);
        sp_rd(sp_xb, at + k, sp_buf, n);
        for (i = 0; i < n; ++i) {
            c = sp_buf[i];
            if (!c) {
                if (m == sp_wl)
                    return 1;
                m = 0;
            } else if (m != 0xFF) {
                m = m < sp_wl && sp_lw[m] == c ? m + 1 : 0xFF;
            }
        }
    }
    return 0;
}

unsigned char FASTCALL sp_listadd(unsigned int at, unsigned int max, unsigned int *used)
{
    if (list_has(at, *used))
        return 0;
    if (*used + sp_wl + 1 > max)
        return 1;
    sp_lw[sp_wl] = 0;
    sp_wr(sp_xb, at + *used, sp_lw, sp_wl + 1);
    *used += sp_wl + 1;
    return 0;
}

void sp_ignore(void)
{
    if (!sp_xb)
        return;
    sp_listadd(SP_IGN, SP_IGNMAX, &sp_ilen);
    hash();
    hash_put(SP_KNOWN);
}

unsigned char sp_adduser(void)
{
    if (!sp_xb || sp_listadd(SP_USER, SP_USERMAX, &sp_ulen))
        return 1;
    hash();
    hash_put(SP_KNOWN);
    sp_end();                           /* (one file at a time) */
    return sp_usave();
}

/*---------------------------------------------------------------
  the cache and the file
 ---------------------------------------------------------------*/
void sp_end(void)
{
    if (fopen_)
        sp_fclose();
    fopen_ = 0;
}

static unsigned char FASTCALL fread_at(unsigned long off, unsigned char b, unsigned int at, unsigned int n)
{
    if (!fopen_) {
        if (sp_fopen(dlang))
            return 1;
        fopen_ = 1;
        fpos = 0;
    }
    if (fpos != off) {
        if (sp_fseek(off)) {
            sp_end();                   /* (the channel gone: opened again, once) */
            if (sp_fopen(dlang))
                return 1;
            fopen_ = 1;
            if (sp_fseek(off)) {
                sp_end();
                return 1;
            }
        }
    }
    fpos = 0xFFFFFFFFUL;
    if (sp_fread(b, at, n) != n) {
        sp_end();
        return 1;
    }
    fpos = off + n;
    return 0;
}

/* unit u of the file in the cache: its slot (NO_SLOT: not read) */
static unsigned char FASTCALL unit(unsigned int u)
{
    static unsigned char s, k;
    static unsigned int best, a;
    if (u >= SP_UNITS)
        return NO_SLOT;
    sp_rd(hb, SP_UMAP + u, sp_buf, 1);
    s = sp_buf[0];
    if (s) {
        --s;
        s_age[s] = ++age;
        return s;
    }
    s = 0;
    best = 0;
    for (k = 0; k < nslot; ++k) {
        if (s_unit[k] == 0xFFFF) {
            s = k;
            break;
        }
        a = age - s_age[k];
        if (a >= best) {
            best = a;
            s = k;
        }
    }
    if (s_unit[s] != 0xFFFF) {
        sp_buf[0] = 0;
        sp_wr(hb, SP_UMAP + s_unit[s], sp_buf, 1);
        s_unit[s] = 0xFFFF;
    }
    if (fread_at((unsigned long)u << 10, SLOT_BANK(s), SLOT_OFF(s), SP_BLOCK))
        return NO_SLOT;
    s_unit[s] = u;
    s_age[s] = ++age;
    sp_buf[0] = s + 1;
    sp_wr(hb, SP_UMAP + u, sp_buf, 1);
    return s;
}

/* the dictionary's lookup (Writer's dict_known): the last key < the word,
   never <= (keys cut to 16 bytes: a run of blocks can share one) */
/* block b's key against the word (CMP_ERR: its key page not read) */
#define CMP_ERR 0x7F
static signed char FASTCALL bkey(unsigned int b)
{
    static unsigned char s;
    s = unit(keys_unit + (b >> 6));
    if (s == NO_SLOT)
        return CMP_ERR;
    return sp_kcmp(SLOT_BANK(s), SLOT_OFF(s) + ((b & (SP_PAGEKEYS - 1)) << 4));
}

static unsigned int lastb = 0xFFFF;     /* the block the last lookup started at */

#ifdef CW_HOST
unsigned long sp_finds;                 /* (the tests': dictionary lookups made) */
#endif

static unsigned char find(void)
{
    static unsigned int lo, hi, mid, base;
    static unsigned char s, r;
    static signed char c;
#ifdef CW_HOST
    ++sp_finds;
#endif
    if (sp_wl > SP_MAXW || !sp_wl)
        return SP_UNKNOWN;
    /* the block the last lookup started at, when the word is in its
       range (its key < the word <= the next one's): the suggestions'
       candidates mostly are, so the two searches are left out */
    if (lastb < nblocks) {
        c = bkey(lastb);
        if (c == -1 && (lastb + 1 >= nblocks || ((c = bkey(lastb + 1)) >= 0 && c != CMP_ERR))) {
            lo = lastb;
            goto scan;
        }
    }
    lo = 0;
    hi = npages;
    while (hi - lo > 1) {
        mid = (lo + hi) >> 1;
        if (sp_kcmp(sp_xb, SP_TOP + (mid << 4)) < 0)
            lo = mid;
        else
            hi = mid;
    }
    base = lo << 6;
    s = unit(keys_unit + lo);
    if (s == NO_SLOT)
        return SP_UNSURE;
    lo = 0;
    hi = nblocks - base;
    if (hi > SP_PAGEKEYS)
        hi = SP_PAGEKEYS;
    while (hi - lo > 1) {
        mid = (lo + hi) >> 1;
        if (sp_kcmp(SLOT_BANK(s), SLOT_OFF(s) + (mid << 4)) < 0)
            lo = mid;
        else
            hi = mid;
    }
    lo += base;
scan:
    for (lastb = lo;;) {
        s = unit(data_unit + lo);
        if (s == NO_SLOT)
            return SP_UNSURE;
        r = sp_bscan(SLOT_BANK(s), SLOT_OFF(s));
        if (r != 2)
            return r;
        if (++lo >= nblocks)
            return SP_UNKNOWN;
        c = bkey(lo);
        if (c == CMP_ERR)
            return SP_UNSURE;
        if (c != 0)
            return SP_UNKNOWN;          /* (the next block starts after the word) */
    }
}

/* the cache emptied (its banks taken again, or another dictionary) */
static void cache_clear(void)
{
    static unsigned char k;
    lastb = 0xFFFF;
    for (k = 0; k < nslot; ++k)
        s_unit[k] = 0xFFFF;
    clear(hb, SP_UMAP, SP_UNITS);
}

/* the language's dictionary: its header and its top index read */
static unsigned char load(void)
{
    static unsigned char l;
    l = fmt_lang;
    sp_end();
    dlang = l;
    if (fread_at(0, sp_xb, SP_SUGG, SP_HDR))
        goto bad;
    sp_rd(sp_xb, SP_SUGG, sp_buf, 20);  /* (the fields read: the first 20 bytes) */
    if (memcmp(sp_buf, "WRDC", 4) || sp_buf[4] != 2 || sp_buf[5] != SP_KEY)
        goto bad;
    memcpy(&nblocks, sp_buf + 6, 2);    /* (little-endian, as the 65C02) */
    memcpy(&npages, sp_buf + 12, 2);
    memcpy(&keys_unit, sp_buf + 14, 2);
    memcpy(&data_unit, sp_buf + 16, 2);
    if (!nblocks || !npages || npages > SP_TOPMAX || data_unit + nblocks >= SP_UNITS)
        goto bad;
    if (fread_at(SP_HDR, sp_xb, SP_TOP, npages << 4))
        goto bad;
    cache_clear();
    sp_flush();
    dmiss = 0;
    return 1;
bad:
    sp_end();
    dlang = 0;
    dmiss = l;
    sp_err = SPE_NODIC;
    return 0;
}

unsigned char sp_ready(void)
{
    static unsigned char k, n;
#ifdef SPELL_OFF
    return 0;
#endif
    if (!fmt_lang)
        fmt_lang = sp_deflang();
    if (!hb) {
        /* (from the document's pool, as the cache's: Veranda keeps 24
           runs for every program together, and the pool takes them, W59) */
        hb = sp_bank(1);
        sp_xb = sp_bank(1);
        if (!hb || !sp_xb) {
            sp_bfree(hb);
            sp_bfree(sp_xb);
            hb = sp_xb = 0;
            sp_err = SPE_MEM;
            return 0;
        }
        sp_flush();
        clear(hb, SP_UMAP, SP_UNITS);
        sp_ulen = sp_ilen = 0;
        sp_uload();
    }
    if (dlang != fmt_lang) {
        if (dmiss == fmt_lang) {
            sp_err = SPE_NODIC;
            return 0;
        }
        if (!load())
            return 0;                   /* (no dictionary: no cache's banks taken) */
    }
    if (!ncb) {
        n = sp_tflags & 2 ? 2 : sp_nbanks();
        if (n > SP_CBMAX)
            n = SP_CBMAX;
        for (k = 0; k < n; ++k)         /* (sp_bank(0): only while none goes to the card for it) */
            if (!(cb[k] = sp_bank(k < 1 ? 1 : 0)))
                break;
        ncb = k;
        nslot = k << 3;
        if (!ncb) {
            sp_err = SPE_MEM;
            return 0;
        }
        cache_clear();
#if defined(CW_TEST) && !defined(CW_HOST)
        memcpy(iobuf, "CW SPBANKS ", 11);   /* (the gate's: the cache's banks) */
        iobuf[11] = '0' + ncb / 10;
        iobuf[12] = '0' + ncb % 10;
        iobuf[13] = 0;
        LogText(iobuf);
#endif
    }
    return 1;
}

/* the dictionary not there for this language (live marking leaves the
   words alone) */
unsigned char sp_nodic(void)
{
    return dmiss && dmiss == fmt_lang;
}

/* (the gate's measurements: all 0; v/spload.c's sp_unload: all 1) the
   hash emptied; all: the dictionary's banks given back too, all its
   state forgotten (the overlays go next) */
void FASTCALL sp_drop(unsigned char all)
{
    if (all == 2) {
        dmiss = 0;                      /* (W61) a dictionary not there: looked for again next time */
        return;
    }
    if (!all) {
        sp_flush();
        return;
    }
    sp_aside();
    sp_bfree(hb);
    sp_bfree(sp_xb);
    hb = sp_xb = 0;
    dlang = dmiss = 0;
    sp_ulen = sp_ilen = 0;
}

void sp_aside(void)
{
    static unsigned char k;
    sp_end();
    for (k = 0; k < ncb; ++k)
        sp_bfree(cb[k]);
    ncb = nslot = 0;
}

unsigned char sp_look(void)
{
    static unsigned char r;
    if (sp_xb && (list_has(SP_USER, sp_ulen) || list_has(SP_IGN, sp_ilen)))
        return SP_KNOWN;
    if (!sp_ready())
        return SP_UNSURE;
    sp_busy = 1;                        /* (the cache in use: kept) */
    r = find();
    sp_busy = 0;
    return r;
}

unsigned char sp_check(void)
{
    static unsigned char r;
    r = sp_probe();                     /* (its hash() is hash_put's too) */
    if (r != SP_NONE)
        return r;
    r = sp_look();
    if (r != SP_UNSURE && hb && fmt_lang == dlang)
        hash_put(r);
    return r;
}

#ifdef CW_HOST
/* the host's: the X16's main RAM part (v/spell.s) */
unsigned char sp_lw[SP_MAXW + 1], sp_wl, sp_word[SP_MAXW + 1], sp_live, sp_pend, sp_kj, sp_cfg;
pos_t sp_ws, sp_we;
extern unsigned char host_bank[256][8192];

void FASTCALL sp_rd(unsigned char b, unsigned int off, unsigned char *dst, unsigned int n)
{
    memcpy(dst, host_bank[b] + off, n);
}

void FASTCALL sp_wr(unsigned char b, unsigned int off, const unsigned char *src, unsigned int n)
{
    memcpy(host_bank[b] + off, src, n);
}

/* Writer's fast_scan (its C version) */
unsigned char FASTCALL sp_bscan(unsigned char b, unsigned int off)
{
    const unsigned char *p = host_bank[b] + off, *w = sp_lw;
    unsigned char m = 0, pre, n, k, len = sp_wl;
    while ((pre = p[0]) != 0xFF) {
        n = p[1];
        if (pre < m)
            return 0;
        if (pre == m) {
            for (k = 0; k < n && m + k < len && p[2 + k] == w[m + k]; ++k)
                ;
            if (k == n && m + k == len)
                return 1;
            if (m + k == len || (k < n && p[2 + k] > w[m + k]))
                return 0;
            m = (unsigned char)(m + k);
        }
        p += 2 + n;
    }
    return 2;
}

signed char FASTCALL sp_kcmp(unsigned char b, unsigned int off)
{
    const unsigned char *k = host_bank[b] + off;
    unsigned char j, c;
    for (j = 0; j < SP_KEY; ++j) {
        c = j < sp_wl ? sp_lw[j] : 0;
        if (k[j] != c)
            return k[j] < c ? -1 : 1;
        if (!c)
            return 0;
    }
    return 0;
}

/* the host's statistics */
unsigned int sp_hreads(void) { return age; }
#endif
