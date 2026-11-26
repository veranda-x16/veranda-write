/* Veranda Write on Veranda: after a .docx or an .odt was converted
   (overlay 18; DECISIONS W51): "Converted from Word" (or OpenDocument)
   in the status bar, and what was changed in a box, as
   the geoWrite import says it (gwimport.c's gw_note): "1 table as text,
   3 pictures left out, ..." in a box of three lines (the counts of struct
   oxcount).

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#include <string.h>
#include <vrwin.h>
#include "cwv.h"
#include "view.h"
#include "../office.h"
#pragma code-name ("OVERLAY18")
#pragma rodata-name ("OVL18RO")
#pragma bss-name ("OVL18BSS")

/* "Left out: 2 pictures, 1 note. 1 table in a cell as text, 2 changes accepted."
   in three lines of at most LN characters (the standard box's width:
   main RAM's msg_a, msg_b, msg_c, main.c's note3 shows them), apart at
   spaces; what does not fit ends with "..." */
#define LN 33
extern char msg_c[40];
void note3(void);
static char *const ln3[3] = { msg_a, msg_b, msg_c };
static char b[160];
static char *p;
static unsigned char k;

static void FASTCALL num(unsigned int v)
{
    char d[6];
    unsigned char i = 0;
    do
        d[i++] = (char)('0' + v % 10);
    while ((v /= 10) != 0);
    while (i)
        *p++ = d[--i];
}

/* "n what" (one or more), after ", " (or the first) */
static void FASTCALL item(unsigned int n, const char *one, const char *more)
{
    if (!n)
        return;
    if (k++) {
        *p++ = ',';
        *p++ = ' ';
    }
    num(n);
    *p++ = ' ';
    strcpy(p, n == 1 ? one : more);
    p += strlen(p);
}

static void note(void)
{
    static unsigned char i, l;
    static char *q, *e;
    set_status(oxc.kind == OK_DOCX ? "Converted from Word" : "Converted from OpenDocument");
#ifdef CW_TEST
    {
        /* "CW NOTE kind tables pictures boxes notes comments changes fields lost" */
        static unsigned int *const vp[8] = { &oxc.tables, &oxc.pics, &oxc.boxes, &oxc.notes, &oxc.comments,
                                             &oxc.changes, &oxc.fields, &oxc.lost };
        p = (char *)iobuf;
        strcpy(p, "CW NOTE ");
        p += 8;
        num(oxc.kind);
        for (k = 0; k < 8; ++k) {
            *p++ = ' ';
            num(*vp[k]);
        }
        *p = 0;
        LogText(iobuf);
    }
#endif
    p = b;
    k = 0;
    if (oxc.pics || oxc.boxes || oxc.notes || oxc.comments) {
        strcpy(p, "Left out: ");
        p += 10;
        item(oxc.pics, "picture", "pictures");
        item(oxc.boxes, "text box", "text boxes");
        item(oxc.notes, "note", "notes");
        item(oxc.comments, "comment", "comments");
        *p++ = '.';
        *p++ = ' ';
        k = 0;
    }
    item(oxc.tables, "table in a cell as text", "tables in cells as text");   /* (1.6: tables kept, but in a cell) */
    item(oxc.changes, "change accepted", "changes accepted");
    item(oxc.fields, "field as text", "fields as text");
    item(oxc.lost, "character as ?", "characters as ?");
    if (oxc.sty) {
        oxc.sty = 0;
        item(1, "style merged", "");
    }
    if (oxc.hf)
        item(1, "header cut short", "");
    if (p == b)
        return;
    if (k)
        *p++ = '.';
    while (p > b && p[-1] == ' ')
        --p;
    *p = 0;
    /* the lines: each to its last space before LN characters */
    q = b;
    for (i = 0; i < 3; ++i) {
        l = (unsigned char)strlen(q);
        if (l > LN) {
            for (e = q + LN; e > q && *e != ' '; --e)
                ;
            if (e == q)
                e = q + LN;
            l = (unsigned char)(e - q);
        }
        memcpy(ln3[i], q, l);
        ln3[i][l] = 0;
        q += l;
        while (*q == ' ')
            ++q;
    }
    if (*q)
        strcpy(msg_c + (strlen(msg_c) > LN - 3 ? LN - 3 : strlen(msg_c)), "...");
    note3();
}

/* (W66) then the office overlays' banks back for the text (this one's
   among them: nothing takes it before the return) */
void e_ox_note(void)
{
    note();
    callb(bank16, ox_down);
}
