/* Veranda Write: the readers of .docx and .odt documents and the
   emitter they put the text in through (oemit.c; DECISIONS W50).
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef OIMP_H
#define OIMP_H

/* what a file says of a paragraph or a run (or a style of them): each
   byte field P_NO when it says nothing; first P_NOS; the flags fset (said)
   and fval; size 0 (oemit.c's, the readers set them by pr_set) */
#define P_NO  0xFF
#define P_NOS (-128)
struct prop {
    unsigned char kind;                 /* PK_ (a heading, a quotation) */
    unsigned char align;                /* PA_ */
    unsigned char left, right;          /* indents, pixels */
    unsigned char before, after;        /* spacing, pixels */
    unsigned char spacing;              /* line spacing, halves */
    unsigned char list;                 /* 0 none, PK_BULLET, PK_NUMBER */
    unsigned char pbb, pba;             /* a page break before, after */
    unsigned char mono;                 /* 1: a monospaced font */
    unsigned char mp;                   /* (.odt) its master page */
    signed char first;                  /* the first line's indent, pixels */
    unsigned char fset, fval;           /* SF_ */
    unsigned char size;                 /* points */
};
#define P_BYTES 12
/* pr_set's fields (struct prop's bytes) and operations */
#define F_KIND    0
#define F_ALIGN   1
#define F_LEFT    2
#define F_RIGHT   3
#define F_BEFORE  4
#define F_AFTER   5
#define F_SPACING 6
#define F_LIST    7
#define F_PBB     8
#define F_PBA     9
#define F_MONO    10
#define F_MP      11
#define F_FIRST   12
#define F_SIZE    15
#define F_ON      0x20                  /* | SF_: the flag on */
#define F_OFF     0x40                  /* | SF_: off */
#define F_KIND1   0x60                  /* the kind, unless it was said */
#define PR(f, v)  ((unsigned int)(f) << 8 | (unsigned char)(v))

/* main RAM (the readers' overlays and the emitter's) */
extern unsigned char pr_t;              /* whose properties pr_set sets: */
#define PT_NONE 0                       /*   nobody's */
#define PT_PAR  1                       /*   the paragraph's */
#define PT_RUN  2                       /*   the run's */
#define PT_DEF  3                       /*   the document's defaults */
#define PT_STY  4                       /*   the style st_new defined */
extern unsigned char em_ps, em_rs;      /* the paragraph's, the run's style (0xFF: none) */
extern unsigned char def_ps;            /* the paragraphs' default style (0xFF: none) */
/* (1.6) a table's events, as the readers meet them (em_tbl: oemit.c
   keeps the grid, the cells, the codes): */
#define TB_START 0                      /* a table (not in a cell) */
#define TB_COL   1                      /*   a grid column, v twips wide */
#define TB_BORD  2                      /*   its borders: v TR_NOGRID or 0 */
#define TB_ROW   3                      /*   a row */
#define TB_CELL  4                      /*   a cell */
#define TB_SPAN  5                      /*   over v grid columns */
#define TB_PARA  6                      /*   a paragraph in it begun (em_par first) */
#define TB_NEST  7                      /*   a paragraph of a table in a cell (the cell's) */
#define TB_ENDC  8                      /*   the cell's end */
#define TB_END   9                      /* the table's end */
void em_tbl(unsigned char op, unsigned int v);

#define OS_N 255                        /* the styles a file may define (a bank of 16 bytes each) */

void FASTCALL pr_set(unsigned int fv);
void FASTCALL pr_pct(unsigned int pct); /* the size: pct per cent of what it is */
void FASTCALL pr_clr(unsigned char t);  /* t's properties cleared, t the target */
unsigned char FASTCALL st_find(unsigned int h);
unsigned char FASTCALL st_new(unsigned int h);
void FASTCALL st_base(unsigned char i); /* the style being defined based on style i */
void FASTCALL st_auto(unsigned char i);   /* (.odt) the style being defined automatic, its parent i */
void FASTCALL em_pstyle(unsigned char i);  /* the paragraph's style i (an automatic one: its own
                                              properties the paragraph's, its parent the style) */
unsigned char em_mp(void);              /* (.odt) the paragraph's master page */
void FASTCALL em_span(unsigned char i); /* (.odt) a span in style i (0xFF: none) begins */
void em_unspan(void);                   /*   ends */

unsigned char em_init(void);            /* 1: no bank for the styles */
void em_done(void);                     /* (its bank given back) */
void em_par(void);                      /* a paragraph starts */
void em_need(void);                     /* (it begun in the document) */
void em_endpar(void);
void em_split(void);                    /* a line's end in it */
void em_pbreak(void);
void em_run(void);                      /* the run's properties changed */
void FASTCALL em_chars(const char *s, unsigned char n);   /* (s: main RAM or the emitter's) */
void FASTCALL em_char(unsigned char c); /* one (a tab, a space ...) */
void FASTCALL em_spaces(unsigned char n);
void FASTCALL em_field(unsigned char k);   /* (a header's or footer's) 1 {page}, 2 {pages} */
void FASTCALL em_hfbegin(unsigned char k);
void em_hfend(void);
unsigned char em_finish(void);
void em_page(void);                     /* the page setup the file gave: checked, applied */

#endif
