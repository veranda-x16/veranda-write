/* Veranda Write: the document model's declarations (portable C: the
   X16 under Veranda and the host's unit tests build the same files).

   The document is one byte stream (docs/FORMAT.md, "The text"): the
   characters, CR at each paragraph's end, and 3-byte codes [kind arg kind]
   for everything that is not a character (a character style; a
   paragraph style, first in its paragraph; a page break, an object
   later). It is kept in RAM
   banks as a chunked buffer (cbuf.c: one chunk an 8 KB bank, an edit
   moves at most one bank's bytes); the line table (layout.c) and the undo
   log (undo.c) are chunked buffers too. DECISIONS.md W1-W6 say why.

   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#ifndef CW_H
#define CW_H

typedef unsigned long pos_t;            /* a byte position in a stream */

#if defined(CW_TEST) && !defined(CW_HOST)
void __fastcall__ test_mark(const char *what);
#define MARK(s) test_mark(s)
#else
#define MARK(s)
#endif
#if defined(CW_PROF) && defined(CW_TEST) && !defined(CW_HOST)
#define PM(s) test_mark(s)
#else
#define PM(s)
#endif

#ifndef FASTCALL
#ifdef CW_HOST
#define FASTCALL
#else
#define FASTCALL __fastcall__
#endif
#endif

/*---------------------------------------------------------------
  RAM banks (bank.c; the platform gives runs of them)
 ---------------------------------------------------------------*/
#define BANK_SIZE 8192u
#ifdef CW_HOST
extern unsigned char host_bank[256][8192];
extern unsigned char host_mapped;
#define BANK_SAVE()     unsigned char bk_saved_ = host_mapped
#define BANK_MAP(b)     (host_mapped = (b))
#define BANK_P          (host_bank[host_mapped])
#define BANK_RESTORE()  (host_mapped = bk_saved_)
#else
#define BANK_SAVE()     unsigned char bk_saved_ = *(volatile unsigned char *)0
#define BANK_MAP(b)     (*(volatile unsigned char *)0 = (b))
#define BANK_P          ((unsigned char *)0xA000)
#define BANK_RESTORE()  (*(volatile unsigned char *)0 = bk_saved_)
#endif

unsigned char bk_get(void);             /* a bank of the pool, else a chunk's put out to the card (0: none) */
void FASTCALL bk_put(unsigned char b);  /* back to the pool */
unsigned char bk_free_n(void);          /* banks free in the pool now */
unsigned char bk_cheap(void);           /* bk_get (a paged chunk's) would put nothing out to the card */
extern unsigned char bk_owned;          /* banks the pool took in all */
void bk_release(void);                  /* runs wholly free: back to the system */
#define NRUN 24                         /* the runs the pool may take (Veranda's V57) */
extern unsigned char run_b[NRUN], run_n[NRUN];   /* each run's first bank, its banks (0: none) */
unsigned char FASTCALL bk_isfree(unsigned char b);   /* b is in the pool, free */
extern unsigned char bk_keep;           /* banks left to the system when the pool takes a run */
#define BK_LO 4                         /* the first runs: banks never put out (the undo log ...) */
extern unsigned char bk_pg;             /* bk_get: the bank is for a paged chunk */
unsigned char FASTCALL bk_inlo(unsigned char b);   /* b is in the first BK_LO runs */
unsigned char sw_take_lo(void);         /* sw_take, a bank of the first runs */
/* the platform's */
unsigned char FASTCALL plat_bank_run(unsigned char n);  /* n banks in a row: the first, 0 none */
void FASTCALL plat_bank_free(unsigned char b);          /* the run starting at b */
/* (W59) banks the platform can give back to the pool before a chunk is
   put out to the card for one (the spelling dictionary's cache): 1 some */
unsigned char plat_spare(void);

/*---------------------------------------------------------------
  cbuf.c, cbufx.c: a byte stream in RAM banks, a chunk a bank;
  swap.c: the text and the line table paged to the card
 ---------------------------------------------------------------*/
/* A paged cbuf (the text, the line table: pg) keeps where each chunk's
   bytes are on the card (swapwh.c): a chunk need not be in RAM (bank 0)
   and is read back when it is used (swap.c; DECISIONS W29) */
#define WH_NONE 0xFFFFFFFFUL            /* no copy on the card (new, or changed since) */
#define WH_SWAP 0x80000000UL            /* | n: in the swap's slot n */
#define WH_DIRTY(w) ((unsigned char)((w) >> 24) == 0xFF)
struct cbuf {
    unsigned char *bank;                /* [max] each chunk's bank (0: on the card only) */
    unsigned int *len;                  /* [max] its bytes (at $A000 ..) */
    unsigned int max, n;                /* chunks there may be, there are */
    unsigned int ci;                    /* the cache: a chunk ... */
    pos_t cs;                           /* ... and where it starts */
    pos_t total;                        /* the bytes */
    unsigned char pg;                   /* paged: PG_TX, PG_LT (0: always in RAM) */
};
void cb_init(struct cbuf *c, unsigned char *bank, unsigned int *len, unsigned int max);
void cb_initp(struct cbuf *c, unsigned char *bank, unsigned int *len, unsigned char pg, unsigned int max);
void cb_clear(struct cbuf *c);          /* empty, its banks back to the pool */
/* src in main RAM; 0 done, 1 no room (nothing changed). cb_split (the
   typing: ui.c's type): an insertion into the text with more than
   CB_SPLIT bytes after it in its chunk splits the chunk there instead of
   moving them, when a bank is to be had without putting a chunk out to
   the card (W43: the characters typed after it go in at the chunk's end) */
#ifdef CW_HOST
#define CB_SPLIT 16                     /* (the tests' small documents split too) */
#else
#define CB_SPLIT 1024
#endif
extern unsigned char cb_split;
unsigned char cb_insert(struct cbuf *c, pos_t at, const unsigned char *src, unsigned int n);
void cb_delete(struct cbuf *c, pos_t at, pos_t n);
unsigned int cb_read(struct cbuf *c, pos_t at, unsigned char *dst, unsigned int n);
void cb_write(struct cbuf *c, pos_t at, const unsigned char *src, unsigned int n);
unsigned char cb_byte(struct cbuf *c, pos_t at);
/* style_at's last answer, kept until the text changes (lay_dirty, lay_all,
   tx_reset: W43) */
extern pos_t sa_pos;
extern unsigned char sa_s, sa_ok;
/* at the end, chunks filled to fill bytes (loading); 0 done, 1 no room.
   cb_src: where src's bytes are in the document's file (a new chunk
   starting there needs no copy in the swap), WH_NONE (the default: set
   again after each call) when they are not the file's as they are */
unsigned char cb_append(struct cbuf *c, const unsigned char *src, unsigned int n, unsigned int fill);
extern pos_t cb_src;
/* could n more bytes go in (chunks and banks)? */
unsigned char cb_room(struct cbuf *c, pos_t n);

/* the pager (swap.c) */
extern unsigned char sw_scan;           /* a pass through all of it (save, find): what it read goes first */
extern unsigned char sw_err;            /* the card failed: SE_ */
#define SE_WRITE 1                      /*   a chunk could not be written (the card full?) */
#define SE_READ  2                      /*   one could not be read back */
extern unsigned int sw_reads, sw_writes; /* chunks read and written (the gate's counts) */
void sw_init(void);                     /* the tables (tx, lt: cb_initp) */
unsigned char sw_take(void);            /* a bank from a chunk put out to the card (0: none) */
void sw_reset(void);                    /* the document replaced: the swap's slots free */
/* the document was saved: its text is the file's from off on (every
   chunk clean, backed by it); the swap's slots free */
void sw_saved(pos_t off);
/* put aside: chunks out to the card until the system has want banks
   free (or nothing more can go); the banks given back */
void sw_aside(void);
void sw_ulout(void), sw_ulin(void);     /* (swapwh.c) the undo log's banks out to the card, back */
extern unsigned char sw_want;
/* the platform's (swapio.c on the X16; tests/host.c) */
unsigned char FASTCALL plat_sw_write(unsigned int slot, unsigned char bank, unsigned int n);   /* 0 done */
unsigned char FASTCALL plat_sw_read(unsigned int slot, unsigned char bank, unsigned int n);    /* 0 done */
unsigned char FASTCALL plat_doc_read(pos_t off, unsigned char bank, unsigned int n);          /* 0 done */
unsigned char plat_free(void);          /* the system's free banks */
unsigned char FASTCALL plat_sw_fail(unsigned char e);   /* the card failed (SE_): told; 1 try again */

/*---------------------------------------------------------------
  the text's bytes (docs/FORMAT.md)
 ---------------------------------------------------------------*/
#define CH_TAB   0x09
#define CH_CR    0x0D                   /* a paragraph's end */
#define K_FIRST  0x10                   /* 3-byte codes: kind arg kind */
#define K_CS     0x10                   /*   a character style: arg = its index */
#define K_PS     0x11                   /*   (2) a paragraph style */
#define K_PB     0x12                   /*   (1.2) a page break: a paragraph of its own */
#define K_OBJ    0x13                   /*   (5) an object */
#define K_TROW   0x14                   /*   (1.6) a table row starts: its first cell's first paragraph; arg TR_ */
#define K_TCELL  0x15                   /*   (1.6) a table cell starts: arg its width, 256ths of the text's */
#define K_TPARA  0x16                   /*   (1.6) a further paragraph of the cell before */
#define K_LAST   0x17
#define IS_TKIND(c) ((unsigned char)((c) - K_TROW) <= K_TPARA - K_TROW)
#define IS_KIND(c) ((unsigned char)((c) - K_FIRST) <= K_LAST - K_FIRST)
/* a byte that may be a character of the text */
#define IS_CHAR(c) ((c) == CH_TAB || ((c) >= 0x20 && (c) < 0x7F) || (c) >= 0xA0)

/*---------------------------------------------------------------
  styles (doc.c): the document's character styles and fonts
 ---------------------------------------------------------------*/
#define MAXSTY   128
#define MAXFONT  8
#define FONT_LEN 32                     /* a name with its 0 */
#define SF_BOLD   0x01
#define SF_ITALIC 0x02
#define SF_ULINE  0x04
#define SF_KNOWN  0x07                  /* (the others are kept, not drawn) */
struct cstyle {
    unsigned char font;                 /* index into the font names */
    unsigned char size;                 /* points */
    unsigned char flags;                /* SF_ */
    unsigned char colour;               /* (later; 0) */
};
extern struct cstyle sty[MAXSTY];
extern unsigned char nsty;
extern char fontname[MAXFONT][FONT_LEN];
extern unsigned char nfont;
unsigned char FASTCALL font_index(const char *name);   /* added if new; 0xFF full */
/* the style with these attributes (added if new); 0xFF: the table is full */
unsigned char sty_find(unsigned char font, unsigned char size, unsigned char flags, unsigned char colour);
void sty_reset(void);                   /* style 0 the default only */
extern const char def_font[];           /* "Gate Sans" */
#define DEF_SIZE 12

/* paragraph styles (FORMAT 1.1: PARA): a paragraph's style is the code
   [K_PS n K_PS] that starts it (right after the CR before it, before any
   character style code), or style 0 (Body, as 1.0 laid it out) without
   one; a code never names style 0 */
#define MAXPAR   32
#define PK_BODY   0                     /* kinds: the named styles */
#define PK_H1     1
#define PK_H2     2
#define PK_H3     3
#define PK_QUOTE  4
#define PK_BULLET 5
#define PK_NUMBER 6
#define PK_N      7
#define PA_LEFT    0                    /* alignment */
#define PA_CENTRE  1
#define PA_RIGHT   2
#define PA_JUSTIFY 3
struct pstyle {
    unsigned char kind;                 /* PK_ */
    unsigned char align;                /* PA_ */
    unsigned char left, right;          /* indents, pixels */
    signed char first;                  /* the first line's, from left (lists: the marker's) */
    unsigned char before, after;        /* space above its first line, below its last */
    unsigned char spacing;              /* line spacing in halves: 2 single, 3, 4 double */
};
extern struct pstyle par[MAXPAR];
extern unsigned char npar;
extern const struct pstyle named[PK_N]; /* the named styles' defaults */
/* the style like *p (added if new); 0xFF: the table is full */
unsigned char FASTCALL par_find(const struct pstyle *p);
void par_reset(void);                   /* style 0 only: Body */
#define IS_LIST(k) ((k) >= PK_BULLET)
#define IS_HEAD(k) ((unsigned char)((k) - PK_H1) < 3)

/*---------------------------------------------------------------
  the document (doc.c)
 ---------------------------------------------------------------*/
extern struct cbuf tx;                  /* the text */
#define DOC_LEN (tx.total)
extern unsigned char doc_modified;
extern unsigned char doc_err;           /* DE_ of the last edit (0 fine) */
#define DE_MEM   1                      /* no room */
#define DE_STY   2                      /* too many styles */
#define DE_PAR   3                      /* too many paragraph styles */
void doc_new(void);                     /* empty: style 0, laid out */
void tx_reset(void);                    /* the text emptied (its banks freed) */
unsigned char FASTCALL doc_byte(pos_t p);
pos_t FASTCALL pos_norm(pos_t p);       /* past the codes at p */
pos_t FASTCALL pos_next(pos_t p);       /* the next character's place */
pos_t FASTCALL pos_prev(pos_t p);
pos_t FASTCALL codes_before(pos_t p);   /* the start of the character style codes right before p */
pos_t FASTCALL par_start(pos_t p);      /* the start of p's paragraph */
pos_t FASTCALL par_next(pos_t p);       /* the next paragraph's start after p (DOC_LEN: none) */
unsigned char FASTCALL par_at(pos_t q); /* the style of the paragraph starting at q */
unsigned char FASTCALL style_at(pos_t p);   /* the style in effect at p */
/* the style typing at p gets: its character's before it, else at p */
unsigned char FASTCALL style_typing(pos_t p);
/* n characters (no codes; CR ends a paragraph) in style t at p (p
   normalised); returns where the cursor goes after them */
pos_t doc_insert(pos_t p, const unsigned char *s, unsigned int n, unsigned char t);
/* bytes with codes (a copy of the document's, starting with a style
   code) at p; the style after them kept; returns the place after them */
pos_t doc_insert_raw(pos_t p, const unsigned char *s, unsigned int n);
/* normalised places; returns where the cursor goes: lo as the tidying
   left it (a code it took out before lo moves it back; W41) */
pos_t doc_delete(pos_t lo, pos_t hi);
/* the selection's characters' styles changed (FMT_*) */
#define FMT_SET   0                     /* flags |= v */
#define FMT_CLR   1                     /* flags &= ~v */
#define FMT_FONT  2                     /* the font (index) = v */
#define FMT_SIZE  3                     /* the size = v */
#define FMT_PLAIN 4                     /* no flags */
unsigned char FASTCALL fmt_map(unsigned char s, unsigned char op, unsigned char v);
void doc_format(pos_t lo, pos_t hi, unsigned char op, unsigned char v);
/* 1 if every character in [lo, hi) has flag f */
unsigned char doc_all_have(pos_t lo, pos_t hi, unsigned char f);
void doc_tidy(pos_t lo, pos_t hi);      /* codes that do nothing out of [lo, hi) */
/* the paragraphs of [lo, hi] (hi > lo: up to hi's last character's)
   changed (PF_*); the named style v (PF_NAMED) changes their characters'
   size and flags too (doc_named) */
#define PF_NAMED   0                    /* the named style v's defaults */
#define PF_ALIGN   1                    /* align = v */
#define PF_INDENT  2                    /* left += (signed char)v, 0 .. 240 */
#define PF_SPACING 3                    /* spacing = v */
#define PF_AFTER   4                    /* after = v */
unsigned char FASTCALL par_map(unsigned char ps, unsigned char op, unsigned char v);
void doc_para(pos_t lo, pos_t hi, unsigned char op, unsigned char v);
void doc_named(pos_t lo, pos_t hi, unsigned char kind);
extern pos_t tp_a, tp_b;                /* places doc_tidy keeps right; after
                                           doc_format: the new selection */
extern pos_t tk_a, tk_b;                /* places every edit keeps right (moved
                                           with the bytes: doc_para, doc_named) */

/* the primitives (logged for undo, the layout told) */
unsigned char tx_ins(pos_t p, const unsigned char *s, unsigned int n);
void tx_del(pos_t p, pos_t n);
void FASTCALL tx_set(pos_t p, unsigned char c);

/*---------------------------------------------------------------
  undo.c: a log of the primitives in RAM banks of its own
 ---------------------------------------------------------------*/
#define UL_BANKS 4                      /* the log's banks at most */
extern struct cbuf ul;
extern unsigned char ul_lost;           /* the step being made cannot be undone */
void ul_init(void);
void ul_reset(void);
/* a step begins (the place and the selection's other end to go back to);
   kind U_TYPE runs on while the typing goes on at its end */
#define U_CMD   0
#define U_TYPE  1
void ul_step(unsigned char kind, pos_t cur, pos_t anchor);
void ul_break(void);                    /* the next typing starts a step */
void ul_log(unsigned char t, pos_t p, const unsigned char *s, unsigned int n);
void ul_log_del(pos_t p, pos_t n);      /* (the text's bytes, before they go) */
void FASTCALL ul_log_set(pos_t p, unsigned char old, unsigned char c);
/* 1 done: the place and the other end in ul_cur, ul_anchor */
unsigned char ul_undo(void);
unsigned char ul_redo(void);
unsigned char ul_can_undo(void);
unsigned char ul_can_redo(void);
extern pos_t ul_cur, ul_anchor;

/*---------------------------------------------------------------
  layout.c: word wrap and the line table
 ---------------------------------------------------------------*/
#define LT_SZ    7
#define LF_PARA  0x01                   /* the line ends its paragraph (CR) */
#define LF_FIRST 0x02                   /* the line starts its paragraph */
#define LF_PB    0x04                   /* (1.4) a page break's paragraph: its page ends after it */
#define LEADING  2                      /* pixels between lines */
#define TABW     40                     /* a tab stop every half inch */
#define MAXLINE  1024                   /* bytes: a longer line is broken */
struct line {
    unsigned int len;                   /* its bytes */
    unsigned char h;                    /* pixels */
    unsigned char a;                    /* the baseline's row from its top */
    unsigned char s;                    /* the style in effect at its start */
    unsigned char f;                    /* LF_ */
    unsigned char p;                    /* its paragraph's style */
    unsigned int w;                     /* its characters' width (no spaces at its end) */
};
/* the number of line i's paragraph in its numbered list (1 + the
   numbered paragraphs straight before it; 0: not numbered) */
unsigned int FASTCALL par_number(unsigned int i);
extern struct cbuf lt;
extern unsigned int lt_n;               /* lines (1 at least) */
extern unsigned int wrap_w;             /* the text's width, pixels */
void FASTCALL lt_get(unsigned int i, struct line *l);
/* the line holding p: lf_i, lf_pos (its start) */
void FASTCALL lt_find(pos_t p);
extern unsigned int lf_i;
extern pos_t lf_pos;
pos_t FASTCALL lt_start(unsigned int i); /* line i's start */
/* an edit: n0 bytes at p replaced by n1 */
void lay_dirty(pos_t p, pos_t n1, pos_t n0);
/* the lines again where they changed: lay_c0 .. lay_c1 (new) are new,
   the old ones from lay_c1 on moved by lay_shift lines; 0: no change */
unsigned char lay_fix(void);            /* (2: no room for the lines: some not in the table) */
extern unsigned int lay_c0, lay_c1;
/* the layout's changes for the view's pages (v/pages.c): each lay_fix
   since the view last looked: old lines pq_l .. pq_l + pq_o now pq_l ..
   pq_c; more than PQ_N, or lay_all: pg_lo the first line changed
   (0xFFFF: none) and pq_k 0xFF */
#define PQ_N 8
extern unsigned int pq_l[PQ_N], pq_o[PQ_N], pq_c[PQ_N];
extern unsigned char pq_k;
extern unsigned int pg_lo;
#define LT_LINES 65000u                 /* the lines a document may have (16-bit line numbers) */
extern int lay_shift;
unsigned char lay_all(void);            /* the whole text; 1: no room */
/* one line from p in style s: wr_len, wr_h, wr_a, wr_f, wr_s (the style
   at its end), wr_w its width; the paragraph it is in: lc_first (it
   starts one: its code read), lc_ps (then the line's, and lc_first the
   next line's) */
void wrap(pos_t p, unsigned char s);
extern unsigned int wr_len, wr_w;
extern unsigned char wr_h, wr_a, wr_f, wr_s;
extern unsigned char lc_first, lc_ps;
/* line l (starting at p): its text's left edge from the text's (lg_x0),
   the extra pixels each of its first lg_nsp spaces gets (justified:
   lg_ex, the first lg_rem of them one more) */
void FASTCALL line_geom(const struct line *l, pos_t p);
extern unsigned int lg_x0, lg_ex, lg_rem, lg_nsp;
/* (when not 0) called now and then during a long layout */
extern void (*lay_busy)(void);

/*---------------------------------------------------------------
  table.c, tabedit.c: simple tables (CWRD 1.6, DECISIONS W62-W66)

  A table is paragraphs: a cell one or more, a row its cells' in order.
  A row's first paragraph starts with [K_TROW f K_TROW] [K_TCELL w
  K_TCELL] (after its paragraph code, if any), each further cell's first
  paragraph with [K_TCELL w K_TCELL], a cell's further paragraphs with
  [K_TPARA 0 K_TPARA]. w: the cell's width in 256ths of the text's (a
  row's sum to 256 at most; a table narrower than the text is left
  aligned). A row is one line of the line table (its width TB_W), as
  tall as its tallest cell: its cells' lines (sub-lines) inside it
 ---------------------------------------------------------------*/
#define TR_NOGRID  0x01                 /* K_TROW's arg: no borders (a table's rows alike) */
#define TB_MAXC    8                    /* cells a row at most (a ninth: the eighth's paragraphs) */
#define TB_W       1023                 /* a table row's line's width (another's is 1,022 at most) */
#define TB_ROWMAX  2000                 /* a row's bytes (a line's are 2,047 at most): typing stops there */
#define TB_PADL    4                    /* a cell's text from its left border */
#define TB_PADX    7                    /*   its width less its cell's */
#define TB_PADT    3                    /*   from the row's top (its border and 2) */
#define TB_PADB    2                    /*   above its bottom (the last row: and its border) */
#define TB_DEFW    32                   /* a cell's width when its code is missing */
#define IS_TR(l)   ((l)->w == TB_W)
extern unsigned char tb_in;             /* word wrap inside a cell (main RAM) */
extern pos_t tb_p;                      /* tb_wrap: the paragraph at tb_p in style tb_s is */
extern unsigned char tb_s, tb_is;       /*   a row's first (tb_is 1: wr_* the row's line) */
extern unsigned char tb_ok;             /* the row tb_lay laid out last is the text's (lay_dirty clears it) */
void tb_wrap(void);
void tb_pstart(void), tb_pnext(void);  /* (tabpar.c: styles.c's par_start, par_next in a row: tb_p's) */
/* (tbhead.h, in table.c and tabedit.c: tb_head(q), the paragraph at q's
   table code: th_k 0 none, K_TROW, K_TCELL, K_TPARA; th_a its argument;
   th_w a row's first cell's width; th_ro, th_co the K_TROW's, K_TCELL's
   offsets from q, 0xFF none; th_po past its paragraph code) */
/* (table.c) the row at p (style s) laid out: its cells (tc_n; tc_x[k]
   cell k's left border from the text's left, tc_x[tc_n] the last one's
   right), its flags tc_f, its height tc_h, tc_last: its table's last row,
   tc_end: its end, tc_se: the style there; its cells' lines (sub-lines,
   sl_: sl_n of them, the first TB_SL; offsets from p, y from its top) */
#define TB_SL 48
struct sub {
    struct line l;                      /* the sub-line as a line (its paragraph's style, ...) */
    unsigned int off;                   /* its place: an offset from the row's */
    unsigned char y, c, num;            /* its top from the row's, its cell, its list number */
    unsigned char pad[2];               /* (16 bytes: tsl[j] a shift) */
};
extern unsigned char tc_n, tc_f, tc_h, tc_last, tc_se, tc_s;
extern unsigned int tc_x[TB_MAXC + 1];
extern pos_t tc_p, tc_end;
extern unsigned char sl_n;
extern struct sub tsl[TB_SL];
void FASTCALL tb_lay(pos_t p, unsigned char s);
unsigned char FASTCALL tb_sub(unsigned int off);   /* the sub-line holding offset off (0xFF: none) */
void tb_pdf(void);                      /* (pdf.c's: the row's numbers, its sub-lines, in tb_xb) */
#define TB_CW(c) (tc_x[(c) + 1] - tc_x[c] > TB_PADX + 8 ? tc_x[(c) + 1] - tc_x[c] - TB_PADX : 8)
/* (tabedit.c) the row holding p (from the line table: laid out): tr_lo
   .. tr_hi, its flags tr_f, its cells (tr_n: tr_c[k] cell k's first
   paragraph, tr_c[tr_n] = tr_hi; tr_w[k] its width); 0: not in a row */
extern pos_t tr_lo, tr_hi, tr_c[TB_MAXC + 1];
extern unsigned char tr_n, tr_f, tr_w[TB_MAXC];
unsigned char FASTCALL tb_row(pos_t p);
/* the table holding p: tt_lo .. tt_hi, its rows tt_n; 0: none */
extern pos_t tt_lo, tt_hi;
extern unsigned int tt_n;
unsigned char FASTCALL tb_table(pos_t p);
unsigned char FASTCALL tb_cell(pos_t p);   /* (after tb_row) the cell p is in */
pos_t FASTCALL tb_cstart(unsigned char k);  /* (after tb_row) cell k's text's start */
#define TB_CEND(k) (tr_c[(k) + 1] - 1)   /*   its last paragraph's CR */
/* the edits (the caller makes them one undo step); 0 done, else DE_ (or
   DE_TB: not here; tb_at: where the cursor goes) */
#define DE_TB 9
extern pos_t tb_at;
unsigned char tb_insert(pos_t p, unsigned char rows, unsigned char cols);   /* before p's paragraph */
unsigned char FASTCALL tb_addrow(pos_t p, unsigned char below);
unsigned char FASTCALL tb_addcol(pos_t p, unsigned char right);
unsigned char FASTCALL tb_delrow(pos_t p);
unsigned char FASTCALL tb_delcol(pos_t p);
unsigned char FASTCALL tb_deltable(pos_t p);
unsigned char tb_widths(pos_t p, const unsigned char *w, unsigned char n);   /* every row's first n cells */
unsigned char FASTCALL tb_grid(pos_t p, unsigned char on);
unsigned char tb_return(pos_t p, unsigned char t);   /* Return in a cell: a further paragraph of it */
/* a deletion of [lo, hi) leaves the tables whole: inside one cell, or
   outside every row (whole tables), 1 */
unsigned char tb_delok(pos_t lo, pos_t hi);
unsigned char tb_bs(pos_t p);           /* Backspace at p: 1 as always, 0 nothing, 2 the cursor to tb_at */
unsigned char tb_del(pos_t p);          /* Delete at p: 1 as always, 0 nothing */
unsigned char tb_next(pos_t p, unsigned char fwd);   /* the cell after (before) p's: tb_at; 0 none */
void FASTCALL tb_used(unsigned char *b);  /* the cells' widths the text has: a bit each in b[32] */

/*---------------------------------------------------------------
  page.c: the page setup (CWRD 1.4, the PAGE chunk: docs/FORMAT.md);
  hf.c: the header's and footer's fields
 ---------------------------------------------------------------*/
#define TW_PX      18                   /* twips (1/1440 inch) a pixel (80 an inch) */
#define PP_A4      0                    /* the paper */
#define PP_LETTER  1
#define PP_OTHER   2
#define PS_NOFIRST 0x01                 /* no header and footer on the first page */
#define HF_MAX     63                   /* a header's or footer's bytes at most */
#define TXW_MIN    240                  /* the text's width, pixels: 3 inches at least, */
#define TXW_MAX    528                  /*   6.6 at most (the screen's page) */
#define TXH_MIN    240                  /* its height on a page: 3 inches at least */
struct pgset {                          /* the PAGE chunk as the file has it (142 bytes) */
    unsigned char paper;                /* PP_ */
    unsigned char flags;                /* PS_ */
    unsigned short w, h;                /* the paper, twips */
    unsigned short top, bottom, left, right;   /* the margins, twips */
    char head[HF_MAX + 1], foot[HF_MAX + 1];   /* the header, the footer (0 ended) */
};
#define PGS_N 142
extern struct pgset pgs;                /* the document's */
extern struct pgset pgs_def;            /* a new document's (the user's default: the X16 reads it from the card) */
extern unsigned int pg_th;              /* the text's height on a page, pixels */
void pgs_default(void);                 /* pgs_def for the document, applied */
void pg_apply(void);                    /* pgs checked; wrap_w and pg_th from it (the caller lays the text out again) */
void FASTCALL pgs_paper(unsigned char k);   /* pgs's paper PP_A4 or PP_LETTER */
/* the header or footer s ("centre", "left|right" or "left|centre|right";
   the fields {page} {pages} {date} {name}) for page pg of n, in
   hf_part[0] (left), [1] (centre), [2] (right) */
#define HF_OUT 100
extern char hf_part[3][HF_OUT];
extern const char *hf_name;             /* {name}: the document's name */
extern char hf_date[24];                /* {date}: "23 November 2026" */
void FASTCALL hf_make(const char *s, unsigned int pg, unsigned int n);
extern unsigned char hf_np;             /* 1: a {pages} made (W55: v_all clears it) */
void hf_today(void);                    /* hf_date: today (the X16's clock) */
/* is the header or footer s shown on page pg? */
#define HF_ON(s, pg) ((s)[0] && ((pg) != 1 || !(pgs.flags & PS_NOFIRST)))

/*---------------------------------------------------------------
  pdf.c: the document as a PDF 1.4 file (DECISIONS W47)
 ---------------------------------------------------------------*/
#define PE_IO   1                       /* the file could not be written */
#define PE_MEM  2                       /* no bank for the cross-reference table's offsets */
#define PE_STOP 3                       /* stopped (plat_pdf_poll) */
extern unsigned int pdf_pages;          /* (the caller's) the pages, for {pages}: pg_first's count */
extern unsigned int pdf_page;           /* the page being written */
extern unsigned long pdf_size;          /* the file's bytes so far */
extern const char *pdf_title;           /* (the caller's) the PDF's title */
unsigned char pdf_write(void);          /* the document; 0 done, else PE_ */
/* the platform's: iobuf's first n bytes to the file (0 done); after each
   page (progress): 1 stop */
unsigned char FASTCALL plat_pdf_out(unsigned int n);
unsigned char plat_pdf_poll(void);
/* pdfhf.c: part k of the header (pdf_hfw 0) or footer (1) of page
   pdf_hfp of pdf_hfn, as PDF content in iobuf; its bytes */
extern unsigned char pdf_hfw;
extern unsigned int pdf_hfp, pdf_hfn;
unsigned int FASTCALL pdf_hf(unsigned char k);

/*---------------------------------------------------------------
  find.c: find, replace all, the counts
 ---------------------------------------------------------------*/
#define FD_MAX  40                      /* a pattern's characters at most */
#define FF_CASE 0x01                    /* match case */
#define FF_WORD 0x02                    /* whole words */
void find_set(const char *s, unsigned char flags);   /* the pattern */
extern unsigned char fd_len, fd_flags;
/* the first match in [p, end): fd_lo, fd_hi (after its last character) */
unsigned char find_from(pos_t p, pos_t end);
unsigned char find_here(pos_t lo, pos_t hi);   /* [lo, hi) is a match */
extern pos_t fd_lo, fd_hi;
/* every match replaced by r (rn characters, in each match's first
   character's style): their number (doc_err: no room, cut short) */
unsigned int replace_all(const unsigned char *r, unsigned char rn);
void doc_count(pos_t lo, pos_t hi);     /* cn_: words, characters, ... */
extern unsigned long cn_words, cn_chars, cn_nonsp, cn_paras;

/*---------------------------------------------------------------
  the fonts (the platform's: fonts_v.c on the X16, host.c)
 ---------------------------------------------------------------*/
/* style s's metrics: fl_w the widths of the NW slots (CSLOT: characters
   $20-$7E slots 0-94, $A0-$FF 96-191: the Veranda set, ISO-8859-16;
   another '?'), fl_bold 1 when
   bold (a pixel more each), fl_a the baseline's row, fl_d the rows from it
   down (the font's height - fl_a) */
void FASTCALL fl_style(unsigned char s);
void fl_flush(void);                    /* the styles' fonts changed */
extern const unsigned char *fl_w;
extern unsigned char fl_bold, fl_a, fl_d;
#define NW 192
#define CSLOT(c) ((c) < 0x7F ? (c) - 32 : (c) >= 0xA0 ? (c) - 64 : '?' - 32)
#define CHW(c) (fl_w[CSLOT(c)] + fl_bold)

/*---------------------------------------------------------------
  format.c: the file (docs/FORMAT.md)
 ---------------------------------------------------------------*/
#define CW_MAGIC "CWRD"
#define CW_MAJOR 1
#define CW_MINOR 6                      /* (1.3: INFO's charset byte, W38; 1.4: PAGE, W44; 1.5: INFO's language, W57; 1.6: tables, W62) */
#define CS_LATIN1  0                    /* INFO byte 6: the text's set: Latin-1 (1.0-1.2) */
#define CS_L10     1                    /*   ISO-8859-16, the Veranda set (1.3: Veranda's V135) */
typedef unsigned char (*cw_wfn)(const void *p, unsigned int n);   /* 0 written */
typedef unsigned int (*cw_rfn)(void *p, unsigned int n);          /* bytes read */
#define FE_IO      1                    /* a read or write failed */
#define FE_NOTCW   2                    /* not a Veranda Write file */
#define FE_NEWER   3                    /* a newer major version */
#define FE_MEM     4                    /* no room */
#define FE_BAD     5                    /* damaged */
#define FE_BIG     6                    /* more text than FMT_MAXTEXT */
#define FE_LINES   7                    /* more lines than the line table holds */
#define FMT_MAXTEXT 0x400000UL          /* 4 MB of text (DECISIONS W31) */
unsigned char fmt_save(cw_wfn w, pos_t cursor);
extern pos_t fmt_toff;                  /* (after fmt_save) the text's offset in the file */
unsigned char fmt_load(cw_rfn r);       /* the document replaced; fmt_cursor */
unsigned char fmt_text(cw_rfn r);       /* a plain text file (UTF-8, Latin-1) */
extern pos_t fmt_cursor;
/* the same a step at a time: fmt_begin (fmt_tbegin: plain text) reads up
   to the text; each fmt_step up to n bytes more of it (wrapped), and at
   its end the rest of the file; ld_kind LD_NONE when all is in */
unsigned char fmt_begin(cw_rfn r);
unsigned char fmt_tbegin(cw_rfn r);
unsigned char fmt_step(cw_rfn r, pos_t n);
#define LD_NONE  0
#define LD_CWRD  1
#define LD_PLAIN 2
extern unsigned char ld_kind;
extern pos_t ld_left, ld_at, ld_size;   /* the text's bytes to read, the file's offset, the text's size */
extern void (*ld_poll)(void);           /* (if not 0) between buffers; sets ld_stop to end the step */
extern unsigned char ld_stop;
/* an edit at p (or anything, p 0xFFFFFFFF) needs the text after it: the
   rest read first if it is near the end of what is in (the platform's:
   files.c, tests) */
void FASTCALL ld_need(pos_t p);
unsigned char FASTCALL ld_safe(pos_t p);   /* (format.c) an edit up to p needs nothing more */
extern unsigned int fmt_lost;           /* characters the set has not, made '?' (the last load) */
extern unsigned char fmt_cs;            /* the last loaded file's set (CS_LATIN1: converted) */
extern unsigned char fmt_lang;          /* the document's spelling language (1.5: 0 not said, 1 English, 2 Polish) */
/* Veranda's set (its vrcs.h, V135): code point u's byte, 0 for none (the
   X16: the SDK's vrcs.s in overlay 10, through glue.s; the host: host.c) */
#ifndef CW_HOST
#define cs_byte f_cs_byte
#endif
unsigned char FASTCALL cs_byte(unsigned int u);

/*---------------------------------------------------------------
  gwimport.c: geoWrite documents (DECISIONS W24-W27)
 ---------------------------------------------------------------*/
/* h: a file's first 508 bytes: 0 not a GEOS file, 1 a GEOS file that is
   not a geoWrite document, 2 a geoWrite document */
unsigned char FASTCALL gw_check(const unsigned char *h);
/* the geoWrite document (read from its start: the X16's stream h, the
   host's gw_rf) converted into the document; 0 done, else FE_ */
unsigned char FASTCALL gw_load(unsigned char h);
void gw_note(void);                     /* (the X16's) what was left out, said */
void gw_scan(void);                     /* (the X16's) the folder's geoWrite documents found */
unsigned char FASTCALL gw_filter(const char *nm);   /* (the X16's) the Open dialog's filter */
extern unsigned char gw_pages, gw_pics, gw_hf, gw_tabs, gw_sty;
extern unsigned int gw_bad;
#ifdef CW_HOST
extern cw_rfn gw_rf;
#endif
#define IOBUF 512
extern unsigned char iobuf[IOBUF];

/*---------------------------------------------------------------
  ai.c: AI help's text (DECISIONS W68; Veranda's docs/AIREQ.md): what
  goes to Veranda Ask (AI.IN) and its answer (AI.OUT) put in
 ---------------------------------------------------------------*/
#define AI_MAX   8192                   /* AI.IN's bytes at most */
#define AI_RUN   256                    /* (ai_add's runs: iobuf's first bytes; its input after them) */
extern pos_t ai_lo, ai_hi;              /* (layout.c: main RAM) the range asked about; after ai_end the answer */
extern unsigned int ai_gen;             /* (layout.c) the text's changes counted (lay_dirty, lay_all) */
extern unsigned int ai_n;               /* (ai_check) AI.IN's bytes */
#define AE_EMPTY 1                      /* ai_check: no characters (spaces only) */
#define AE_CELLS 2                      /*   not inside one table cell, or a table in it */
#define AE_LONG  3                      /*   over AI_MAX */
void FASTCALL ai_para(pos_t p);         /* ai_lo, ai_hi: the paragraph at p (its characters, not its CR) */
unsigned char ai_check(void);           /* [ai_lo, ai_hi) can be asked about: 0, else AE_ */
void ai_text(void);                     /* AI.IN's bytes: plat_ai_put, iobuf at a time */
void FASTCALL plat_ai_put(const unsigned char *b, unsigned int n);   /* (the platform's) */
#define AM_REPLACE 0                    /* ai_begin: the answer in place of the range */
#define AM_AFTER   1                    /*   as paragraphs after the range's paragraph */
#define DE_AI    10                     /* ai_end: the answer had no characters (nothing done) */
void FASTCALL ai_begin(unsigned char mode);
void ai_add(const unsigned char *b, unsigned int n);   /* (b not in iobuf's first AI_RUN bytes) */
unsigned char ai_end(void);             /* 0 done (else DE_: the caller undoes the step) */

#endif
