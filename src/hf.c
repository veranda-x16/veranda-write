/* Veranda Write: the header's and the footer's text for a page (DECISIONS
   W46): its parts and its fields.

   A header (or footer) is one line of up to 63 bytes: "text" is centred,
   "left|right" has a part at each margin, "left|centre|right" three (a
   '|' past the second is the text's own). The fields {page}, {pages},
   {date} and {name} become the page's number, the number of pages,
   today's date ("23 November 2026") and the document's name; any other
   '{' is itself.

   On the X16 this file is compiled twice (the Makefile: the segments on
   cl65's command line): into overlay 14 for the view's bands between the
   pages, and into overlay 15 (-DHF15: cw.h names it hf15_...) for the
   PDF, each with its own variables.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include "cw.h"

char hf_part[3][HF_OUT];
const char *hf_name;
char hf_date[24];

static char *o, *oe;

static void FASTCALL put(const char *s)
{
    while (*s && o < oe)
        *o++ = *s++;
}

static void FASTCALL put_n(unsigned int v)
{
    static char b[6];
    unsigned char n = 0;
    do {
        b[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n && o < oe)
        *o++ = b[--n];
}

static const char *const fld[4] = { "page}", "pages}", "date}", "name}" };

void FASTCALL hf_make(const char *s, unsigned int pg, unsigned int n)
{
    static unsigned char k, f, bars, l;
    static const char *t;
    for (bars = 0, t = s; *t; ++t)
        bars += *t == '|';
    /* no '|': the centre's; one: left and right */
    k = bars ? 0 : 1;
    hf_part[0][0] = hf_part[1][0] = hf_part[2][0] = 0;
    o = hf_part[k];
    oe = o + HF_OUT - 1;
    for (; *s; ++s) {
        if (*s == '|' && k < 2) {
            *o = 0;
            k = bars == 1 ? 2 : k + 1;
            o = hf_part[k];
            oe = o + HF_OUT - 1;
            continue;
        }
        if (*s == '{') {
            for (f = 0; f < 4; ++f) {
                l = (unsigned char)strlen(fld[f]);
                if (!strncmp(s + 1, fld[f], l))
                    break;
            }
            if (f < 4) {
                s += l;
                if (f == 0)
                    put_n(pg);
                else if (f == 1)
                    put_n(n);
                else
                    put(f == 2 ? hf_date : hf_name ? hf_name : "");
                continue;
            }
        }
        if (o < oe)
            *o++ = *s;
    }
    *o = 0;
}

#ifndef CW_HOST
#include <veranda.h>
static const char *const mname[12] = {
    "January", "February", "March", "April", "May", "June", "July", "August", "September", "October",
    "November", "December"
};

void hf_today(void)
{
    o = hf_date;
    oe = hf_date + sizeof hf_date - 1;
    put_n(day);
    put(" ");
    put(mname[(month - 1) % 12]);
    put(" ");
    put_n(2000 + year);
    *o = 0;
}
#endif
