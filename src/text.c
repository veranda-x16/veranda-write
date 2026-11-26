/* Veranda Write: the document's text and its styles: the places in it,
   the style in effect at a place (main RAM on the X16; the edits are
   edit.c's).

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

   A paragraph's style is the code [K_PS n K_PS] it starts with (before
   any character style code), else style 0 (Body): an entry of the
   paragraph style table (par[]: the alignment, the indents, the space
   around it, the line spacing, its kind: Body, Heading 1-3, Quote, a
   bulleted or numbered list). The places are a code's ends here too: a
   place is normalised past it as past any code.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cw.h"

struct cbuf tx;                         /* (its tables: swap.c's) */
unsigned char doc_modified, doc_err;
unsigned char iobuf[IOBUF];

struct cstyle sty[MAXSTY];
unsigned char nsty;
char fontname[MAXFONT][FONT_LEN];
unsigned char nfont;
const char def_font[] = "Gate Sans";
struct pstyle par[MAXPAR];
unsigned char npar;
/* the named styles (pixels at 80 an inch): kind, align, left, right,
   first, before, after, spacing */
const struct pstyle named[PK_N] = {
    { PK_BODY,   PA_LEFT, 0,  0,  0,   0,  0, 2 },
    { PK_H1,     PA_LEFT, 0,  0,  0,  16,  6, 2 },
    { PK_H2,     PA_LEFT, 0,  0,  0,  12,  4, 2 },
    { PK_H3,     PA_LEFT, 0,  0,  0,   8,  2, 2 },
    { PK_QUOTE,  PA_LEFT, 40, 40, 0,   4,  4, 2 },
    { PK_BULLET, PA_LEFT, 32, 0, -16,  0,  2, 2 },
    { PK_NUMBER, PA_LEFT, 32, 0, -24,  0,  2, 2 },
};

void tx_reset(void)
{
    sa_ok = tb_ok = 0;
    cb_clear(&tx);
}

/*---------------------------------------------------------------
  places
 ---------------------------------------------------------------*/
unsigned char FASTCALL doc_byte(pos_t p)
{
    return cb_byte(&tx, p);
}

pos_t FASTCALL pos_norm(pos_t p)
{
    if (p > tx.total)
        return tx.total;                /* (W41: never past the end) */
    while (p < tx.total && IS_KIND(cb_byte(&tx, p)))
        p += 3;
    return p;
}

pos_t FASTCALL pos_next(pos_t p)
{
    p = pos_norm(p);
    if (p < tx.total)
        ++p;
    return pos_norm(p);
}

pos_t FASTCALL pos_prev(pos_t p)
{
    while (p >= 3 && IS_KIND(cb_byte(&tx, p - 1)))
        p -= 3;
    if (p)
        --p;
    return pos_norm(p);
}

