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

static unsigned char in_tag, pend;
#ifdef CW_HOST
static unsigned char xi, xn, txt;
static int ahead;
unsigned char xm_skipa;                 /* (the X16's: main.c, main RAM) */
unsigned int xm_hash;
unsigned char xm_nlen;
#else
/* (W71) the bytes read by v/oxtok.s: its place in XIN (xi, xn), the
   byte put back (ahead: EOS none), txt (1: a text's, its characters
   counted when lost) */
extern unsigned char xi, xn, txt;
extern int ahead;
int rd(void);
void xs_name(void);
unsigned char xs_skipa(void);
void FASTCALL xs_value(unsigned char q);
void FASTCALL xs_text(unsigned char c);
void xs_toend(void);
unsigned char xs_skipc(void);
int sp(void);
#define chr xm_chr
#endif

void xm_init(void)
{
    xi = xn = 0;
    in_tag = pend = 0;
    ahead = EOS;
}

#ifdef CW_HOST
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
#else
/* (v/oxtok.s's: XIN filled; its bytes, 0 at the part's end) */
unsigned char xfill(void)
{
    xn = (unsigned char)oz_read(XIN, XINN);
    xi = 0;
    return xn;
}
#endif

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
#ifndef CW_HOST
int FASTCALL xm_chr(int c)
#else
static int FASTCALL chr(int c)
#endif
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

#ifdef CW_HOST
/* (the X16's: v/oxtok.s) a name into XNAME (XNAMEN - 1 bytes kept),
   its hash (ohelp.c's) and length for the readers' look-up (W71: once,
   here); the byte after it put back */
static void name(void)
{
    static int c;
    static unsigned char n;
    static unsigned int h;
    n = 0;
    h = 5381;
    for (;;) {
        c = RD();
        if (c < 'A' && (c == EOS || SPACE(c) || c == '>' || c == '/' || c == '=' || c == '"' || c == '\''))
            break;
        if (n < XNAMEN - 1) {
            XNAME[n++] = (char)c;
            h = (((h << 5) + h) ^ (unsigned char)c) & 0xFFFF;
        }
    }
    XNAME[n] = 0;
    xm_hash = h;
    xm_nlen = n;
    ahead = c;
}
#define xs_name name

/* a start tag's attributes passed over, to its end: 0 >, 1 />, 2 the
   part's end (a value's quotes kept apart) */
static unsigned char xs_skipa(void)
{
    static int c, q;
    for (;;) {
        c = RD();
        if (c == '>')
            return 0;
        if (c == '/') {
            RD();                       /* (its >) */
            return 1;
        }
        if (c == '"' || c == '\'') {
            q = c;
            while ((c = RD()) != q && c != EOS)
                ;
        }
        if (c == EOS)
            return 2;
    }
}

/* an attribute's value (after its quote q) into XVAL */
static void FASTCALL xs_value(unsigned char q)
{
    static int c;
    static unsigned char n;
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
}

/* text from its first byte c up to the next <, a buffer at a time (the
   rest the next token) into XVAL */
static void FASTCALL xs_text(unsigned char c0)
{
    static int c;
    static unsigned char n;
    c = c0;
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
}

static void xs_toend(void)
{
    static int c;
    while ((c = RD()) != '>' && c != EOS)
        ;
}

/* (W71) an element's content passed over, to the next tag: 0 the
   part's end, 1 an end tag, 2 a start tag, 3 an empty one (each read to
   its >), 4 <!, 5 <? (read to there); a text's characters as the
   tokens' (those the set has not counted) */
static unsigned char xs_skipc(void)
{
    static int c, q;
    txt = 1;
    for (;;) {
        c = RD();
        if (c == EOS)
            return 0;
        if (c != '<') {
            if ((c >= 0x80 || c == '&') && chr(c) == EOS)
                return 0;
            continue;
        }
        c = RD();
        if (c == EOS)
            return 0;
        if (c == '/') {
            xs_toend();
            return 1;
        }
        if (c == '!')
            return 4;
        if (c == '?')
            return 5;
        for (;;) {
            if (c == '>')
                return 2;
            if (c == '/') {
                RD();                   /* (its >) */
                return 3;
            }
            if (c == '"' || c == '\'') {
                q = c;
                while ((c = RD()) != q && c != EOS)
                    ;
            }
            if (c == EOS)
                return 0;
            c = RD();
        }
    }
}
#endif

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

/* after <! (a comment, a declaration, CDATA: passed over) */
static void bang(void)
{
    static int c;
    c = rd();
    if (c == '-') {
        rd();
        skip(0);
    } else
        skip(c == '[' ? "]]>" : ">");   /* (a declaration; CDATA: not in these files, passed over) */
}

/* (W71) the element just begun passed over whole (the reader's xm_skipa
   2: no tokens of it): XT_END (XT_EOF at the part's end) */
static unsigned char skipel(void)
{
    static unsigned int depth;
    static unsigned char r;
    if (in_tag) {
        in_tag = 0;
        r = xs_skipa();
        if (r)
            return r == 1 ? XT_END : XT_EOF;
    } else if (pend) {
        pend = 0;
        return XT_END;
    }
    depth = 1;
    for (;;) {
        switch (xs_skipc()) {
        case 0:
            return XT_EOF;
        case 1:
            if (!--depth)
                return XT_END;
            break;
        case 2:
            ++depth;
            break;
        case 4:
            bang();
            break;
        case 5:
            skip("?>");
            break;
        }
    }
}

unsigned char xm_next(void)
{
    static int c, q;
    static unsigned char r;
    if (xm_skipa == 2)
        return skipel();
    if (pend) {
        pend = 0;
        return XT_END;                  /* (the readers know which: their stacks) */
    }
    if (in_tag && xm_skipa) {
        /* (the reader reads none of this element's attributes: to the
           tag's end, its values' quotes kept apart) */
        r = xs_skipa();
        if (r == 2)
            return XT_ERR;
        in_tag = 0;
        pend = r;
        return XT_OPEN;
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
        xs_name();
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
        xs_value((unsigned char)q);
        return XT_ATTR;
    }
    c = rd();
    if (c == EOS)
        return XT_EOF;
    if (c == '<') {
        c = rd();
        if (c == '/') {
            xs_toend();                 /* (its name: the readers know it) */
            return XT_END;
        }
        if (c == '?') {
            skip("?>");
            return xm_next();
        }
        if (c == '!') {
            bang();
            return xm_next();
        }
        ahead = c;
        xs_name();
        in_tag = 1;
        return XT_START;
    }
    /* text up to the next <, a buffer at a time (the rest the next token) */
    xs_text((unsigned char)c);
    return XT_TEXT;
}

#ifdef CW_HOST
unsigned char xm_vlen;                  /* (the X16's: main.c, main RAM) */
struct oxcount oxc;
#endif
