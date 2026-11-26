/* Veranda Write: Word (.docx) and OpenDocument (.odt) documents, opened
   (converted) and exported (stage 4; DECISIONS W49-W54, may be
   renumbered).

   Opening: the file is a ZIP; ozip.c finds a part in its central
   directory and inflates it (a stored part is read as it is), a buffer at
   a time, its 32 KB window in four RAM banks; oxml.c turns the bytes into
   tokens (a pull tokenizer in constant memory, UTF-8 into the Veranda
   set); odocx.c and oodt.c read the tokens of the parts they need and put
   the text into the document through oemit.c (paragraphs, their styles,
   page breaks, character styles). Nothing of a part is kept: a document
   of megabytes needs no more memory than a small one.

   Exporting: oexp.c walks the text (paragraphs, runs) and writes the
   parts of a .docx or an .odt into a ZIP of stored (uncompressed)
   entries, each part made twice (its size and CRC-32 first, then the
   bytes: the entry's header comes before them); the XML's text is
   oparts.c's templates (tools/oparts.py), oexp.c putting in the values.

   ozip.c and oxml.c are Commander Calc's src/import/zip.c, inflate.c and
   xml.c (MIT, Copyright (c) 2026 Marcin Spoczynski), reworked for this
   program: its banks, its streams, its set. oexp.c's ZIP writer is
   Calc's src/export/zipw.c's way.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef OFFICE_H
#define OFFICE_H

#include "cw.h"

/* iobuf's parts while a document is converted (main RAM: the kernel's
   reads, other overlays' calls) */
#define ZBUF     (iobuf)                /* the ZIP's bytes as read (96) */
#define ZBUFN    96
#define XIN      (iobuf + 96)           /* a part's bytes inflated: the tokenizer's (112) */
#define XINN     112
#define EOB      (iobuf + 208)          /* the text on its way into the document (112) */
#define EOBN     112
#define EPS      ((struct pstyle *)(iobuf + 320))   /* a paragraph style for par_find (8) */
#define EFN      ((char *)iobuf + 328)  /* a font's name for font_index; a style's properties (16) */
#define XNAME    ((char *)iobuf + 344)  /* the token's name (40) */
#define XNAMEN   40
#define XVAL     ((char *)iobuf + 384)  /* its value: an attribute's, a text's (128) */
#define XVALN    128

/*---------------------------------------------------------------
  ozip.c: a ZIP's parts (opening)
 ---------------------------------------------------------------*/
#define OZ_IO    1                      /* the file could not be read */
#define OZ_BAD   2                      /* not a ZIP, or damaged */
#define OZ_MEM   3                      /* no banks for the window */
#define OZ_NONE  4                      /* no such part */
extern unsigned char oz_err;
/* the ZIP in the file (the X16: stream h): its central directory found;
   0 done, else OZ_ */
unsigned char FASTCALL oz_open(unsigned char h);
/* part name (main RAM) opened for oz_read; 0 found, else OZ_ */
unsigned char FASTCALL oz_part(const char *name);
/* its next bytes, inflated, into dst (main RAM; up to n); 0: its end */
unsigned int FASTCALL oz_read(unsigned char *dst, unsigned int n);
void oz_close(void);                    /* the window's banks given back */
#ifdef CW_HOST
extern unsigned int (*oz_rd)(void *p, unsigned int n);
extern unsigned char (*oz_seek)(unsigned long off);
extern unsigned long oz_size;
#endif

/*---------------------------------------------------------------
  oxml.c: the tokens of the part oz_part opened
 ---------------------------------------------------------------*/
#define XT_EOF   0
#define XT_START 1                      /* <name: XNAME */
#define XT_ATTR  2                      /* an attribute: XNAME = XVAL */
#define XT_OPEN  3                      /* the start tag's end (> or />) */
#define XT_TEXT  4                      /* text: XVAL, xm_vlen bytes (the set's), perhaps a part of it */
#define XT_END   5                      /* </name> or a /> : XNAME the element's */
#define XT_ERR   6
void xm_init(void);
unsigned char xm_next(void);
extern unsigned char xm_vlen;
extern unsigned char xm_skipa;          /* (the reader's, after XT_START) its attributes not wanted;
                                           (W71) 2, after XT_START or XT_OPEN: the element passed
                                           over whole, its XT_END next */
extern unsigned int xm_hash;            /* (W71) XNAME's hash (ohelp.c's) and length */
extern unsigned char xm_nlen;
/* the counts the note says (main RAM: both overlays read them) */
struct oxcount {
    unsigned int lost;                  /* characters the set has not: '?' */
    unsigned int tables, pics, notes, comments, changes, boxes, fields;
    unsigned char sty;                  /* 1: styles past the tables' room */
    unsigned char kind;                 /* OK_DOCX, OK_ODT */
    unsigned char hf;                   /* 1: a header or footer cut short */
};
extern struct oxcount oxc;
#define OK_DOCX 1
#define OK_ODT  2

/*---------------------------------------------------------------
  the conversion (odocx.c, oodt.c, oemit.c)
 ---------------------------------------------------------------*/
/* the .docx or .odt in the file (the X16: stream h, read from its
   start) converted into the document (doc_new first); 0 done, else FE_ */
unsigned char FASTCALL ox_load(unsigned char h);   /* (ozip.c: which kind, then the one below) */
unsigned char dx_load(void);            /* a .docx */
unsigned char od_load(void);            /* an .odt */
void ox_note(void);                     /* (the X16's) what was changed, said */

/*---------------------------------------------------------------
  oexp.c: the document as a .docx or an .odt
 ---------------------------------------------------------------*/
#define OE_IO   1                       /* the file could not be written */
#define OE_STOP 2                       /* stopped */
/* the document into the file (plat_oe_out); kind OK_DOCX or OK_ODT;
   iobuf holds {date} (at 0) and {name} (at 32) for the header and the
   footer, {name} the .odt's title too; 0 done, else OE_ */
unsigned char FASTCALL oe_write(unsigned char kind);
extern unsigned long oe_size;           /* the file's bytes */
/* the platform's: n bytes at p to the file (0 done) */
unsigned char FASTCALL plat_oe_out(const unsigned char *p, unsigned int n);
/* oparts.c: the templates (oparts.h's T_ numbers, its V_ values):
   template k begun; its next bytes put in the exporter's buffer (iobuf +
   256, at oe_on, up to OE_N): op_next returns a value's number (31: op_x
   the next byte), $FF when the buffer is full, 0 at the template's end */
#define OE_N 255
void FASTCALL op_open(unsigned char k);
unsigned char op_next(void);
extern unsigned char oe_on, op_x;
/* the set's byte c as a Unicode code point (the SDK's vrcs.s: cs_uni) */
#ifndef CW_HOST
#define cs_uni f_cs_uni
#endif
unsigned int FASTCALL cs_uni(unsigned char c);

#endif
