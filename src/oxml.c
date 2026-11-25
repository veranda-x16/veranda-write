/* Veranda Write: the tokens of an XML part (a .docx's or an .odt's), a
   pull tokenizer in constant memory (DECISIONS W49; office.h).

   Commander Calc's src/import/xml.c (MIT, Copyright (c) 2026 Marcin
   Spoczynski): a token at a time from the bytes ozip.c inflates, no tree,
   no checks a machine-written part does not need; comments, processing
   instructions, declarations and CDATA passed over. Changed for
   Veranda Write: the characters (UTF-8 and the character references)
   into the Veranda set (cs_byte: ISO-8859-16), one the set has not a '?'
   counted (oxc.lost); a token more, XT_OPEN, at the end of a start tag
   (after its attributes: what a reader keeps of them is complete then),
   and XT_END says no name (a reader keeps a stack of the elements open);
   the token's name and value in main RAM (iobuf: the reader is in
   another overlay); the state this file's statics.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "office.h"
#ifndef CW_HOST
#pragma code-name ("OVERLAY17")
#pragma rodata-name ("OVL17RO")
#pragma bss-name ("OVL17BSS")
#define xm_init e_xm_init
#define xm_next e_xm_next
unsigned int FASTCALL e_oz_read(unsigned char *dst, unsigned int n);
#define oz_read e_oz_read
#endif

#define EOS (-1)

static unsigned char xi, xn, in_tag, pend, txt;
static int ahead;
#ifdef CW_HOST
unsigned char xm_skipa;                 /* (the X16's: main.c, main RAM) */
#endif

void xm_init(void)
{
    xi = xn = 0;
    in_tag = pend = 0;
    ahead = EOS;
}

static int c0;

static int rd(void)
{
    if (ahead != EOS) {
        c0 = ahead;
        ahead = EOS;
        return c0;
    }
    if (xi == xn) {
        xn = (unsigned char)oz_read(XIN, XINN);
        xi = 0;
        if (!xn)
            return EOS;
    }
    return XIN[xi++];
}

/* (the common case in line: a byte of the buffer, none put back) */
#define RD() (ahead == EOS && xi < xn ? XIN[xi++] : rd())

#define SPACE(c) ((c) == ' ' || (c) == '\n' || (c) == '\r' || (c) == '\t')

static int sp(void)
{
    static int c;
    do
        c = RD();
    while (SPACE(c));
    return c;
}

/* code point u in the set; '?' (counted) when it has none */
static int FASTCALL set(unsigned long u)
{
    static unsigned char b;
    if (u < 0x80)
        return (int)u;
    b = u < 0x10000UL ? cs_byte((unsigned int)u) : 0;
    if (!b) {
        oxc.lost += txt;                /* (a text's: an attribute's are not the document's) */
        return '?';
    }
    return b;
}

/* a character reference or an entity (after its &) */
static int entity(void)
{
    static char b[10];
    static unsigned char n, i, hex;
    static unsigned long v;
    static int c;
    for (n = 0; n < sizeof b - 1;) {
        c = rd();
        if (c == EOS)
            return EOS;
        if (c == ';')
            break;
        b[n++] = (char)c;
    }
    b[n] = 0;
    if (!strcmp(b, "amp"))
        return '&';
    if (!strcmp(b, "lt"))
        return '<';
    if (!strcmp(b, "gt"))
        return '>';
    if (!strcmp(b, "quot"))
        return '"';
    if (!strcmp(b, "apos"))
        return '\'';
    if (b[0] != '#')
        return '&';                     /* (another entity: its & kept) */
    v = 0;
    hex = b[1] == 'x' || b[1] == 'X';
    for (i = 1 + hex; b[i]; ++i) {
        c = b[i];
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (hex && (c | 0x20) >= 'a' && (c | 0x20) <= 'f')
            c = (c | 0x20) - 'a' + 10;
        else
            return '?';
        v = v * (hex ? 16 : 10) + c;
    }
    return set(v);
}

/* a character of a text or a value (its first byte c): the set's byte;
   EOS at the part's end */
static int FASTCALL chr(int c)
{
    static unsigned long u;
    static unsigned char k;
    static int d;
    if (c == '&')
        return entity();
    if (c < 0x80)
        return c;
    if ((c & 0xE0) == 0xC0) {
        u = c & 0x1F;
        k = 1;
    } else if ((c & 0xF0) == 0xE0) {
        u = c & 0x0F;
        k = 2;
    } else if ((c & 0xF8) == 0xF0) {
        u = c & 0x07;
        k = 3;
    } else {
        oxc.lost += txt;
        return '?';
    }
    while (k--) {
        d = rd();
        if (d == EOS)
            return EOS;
        if ((d & 0xC0) != 0x80) {
            ahead = d;
            oxc.lost += txt;
            return '?';
        }
        u = u << 6 | (d & 0x3F);
    }
    return set(u);
}

static void name(void)
{
    static int c;
    static unsigned char n;
    n = 0;
    for (;;) {
        c = RD();
        if (c < 'A' && (c == EOS || SPACE(c) || c == '>' || c == '/' || c == '=' || c == '"' || c == '\''))
            break;
        if (n < XNAMEN - 1)
            XNAME[n++] = (char)c;
    }
    XNAME[n] = 0;
    ahead = c;
}

/* past the end of a comment (after its <!--), or to the string t */
static void skip(const char *t)
{
    static int c;
    static unsigned char m, d;
    m = d = 0;
    for (;;) {
        c = rd();
        if (c == EOS)
            return;
        if (!t) {
            if (c == '-')
                d = d < 2 ? d + 1 : 2;
            else if (c == '>' && d >= 2)
                return;
            else
                d = 0;
            continue;
        }
        if (c == t[m]) {
            if (!t[++m])
                return;
        } else
            m = c == t[0];
    }
}

unsigned char xm_next(void)
{
    static int c, q;
    static unsigned char n;
    if (pend) {
        pend = 0;
        return XT_END;                  /* (the readers know which: their stacks) */
    }
    if (in_tag && xm_skipa) {
        /* (the reader reads none of this element's attributes: to the
           tag's end, its values' quotes kept apart) */
        for (;;) {
            c = RD();
            if (c == '>') {
                in_tag = 0;
                return XT_OPEN;
            }
            if (c == '/') {
                RD();                   /* (its >) */
                in_tag = 0;
                pend = 1;
                return XT_OPEN;
            }
            if (c == '"' || c == '\'') {
                q = c;
                while ((c = RD()) != q && c != EOS)
                    ;
            }
            if (c == EOS)
                return XT_ERR;
        }
    }
    if (in_tag) {
        c = sp();
        if (c == EOS)
            return XT_ERR;
        if (c == '/') {
            rd();                       /* (its >) */
            in_tag = 0;
            pend = 1;
            return XT_OPEN;
        }
        if (c == '>') {
            in_tag = 0;
            return XT_OPEN;
        }
        ahead = c;
        name();
        c = sp();
        XVAL[0] = 0;
        xm_vlen = 0;
        if (c != '=') {
            ahead = c;                  /* (no value: not XML, taken as empty) */
            return XT_ATTR;
        }
        q = sp();
        if (q != '"' && q != '\'')
            return XT_ERR;
        txt = 0;
        n = 0;
        for (;;) {
            c = RD();
            if (c == EOS || c == q)
                break;
            if (c >= 0x80 || c == '&') {
                c = chr(c);
                if (c == EOS)
                    break;
            }
            if (n < XVALN - 1)
                XVAL[n++] = (char)c;
        }
        XVAL[n] = 0;
        xm_vlen = n;
        return XT_ATTR;
    }
    c = rd();
    if (c == EOS)
        return XT_EOF;
    if (c == '<') {
        c = rd();
        if (c == '/') {
            while ((c = RD()) != '>' && c != EOS)
                ;                       /* (its name: the readers know it) */
            return XT_END;
        }
        if (c == '?') {
            skip("?>");
            return xm_next();
        }
        if (c == '!') {
            c = rd();
            if (c == '-') {
                rd();
                skip(0);
                return xm_next();
            }
            skip(c == '[' ? "]]>" : ">");   /* (a declaration; CDATA: not in these files, passed over) */
            return xm_next();
        }
        ahead = c;
        name();
        in_tag = 1;
        return XT_START;
    }
    /* text up to the next <, a buffer at a time (the rest the next token) */
    txt = 1;
    for (n = 0;;) {
        if (c == EOS)
            break;
        if (c == '<' || n >= XVALN - 4) {
            ahead = c;
            break;
        }
        if (c >= 0x80 || c == '&') {
            c = chr(c);
            if (c == EOS)
                break;
        }
        XVAL[n++] = (char)c;
        c = RD();
    }
    XVAL[n] = 0;
    xm_vlen = n;
    return XT_TEXT;
}

#ifdef CW_HOST
unsigned char xm_vlen;                  /* (the X16's: main.c, main RAM) */
struct oxcount oxc;
#endif
