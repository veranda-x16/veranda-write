/* Veranda Write: what cbuf.c (the reads: main RAM), cbufx.c (the edits)
   and swap.c (the pager: both in overlay 11 on the X16, with the paged
   cbufs' tables) share. Copyright (c) 2026 Marcin Spoczynski. MIT licence
   (see LICENSE). */
#ifndef CBPRIV_H
#define CBPRIV_H
#include "cw.h"

/* the cbuf being worked on (a copy: load, store), its cache while working,
   the place's offset in chunk ci */
extern struct cbuf W;
#define B W.bank
#define L W.len
extern unsigned int ci;
extern pos_t cs;
extern unsigned int off;
extern struct cbuf *cb_cur;             /* (cbufx.c: the cbuf W is, while an edit changes its chunks) */

/* the bank of the paged cbufs' tables (overlay 11's; 0 on the host) */
extern unsigned char sw_tbank;
#ifdef CW_HOST
#define TB_MAP()
#else
#define TB_MAP() BANK_MAP(sw_tbank)
#endif

void FASTCALL locate(pos_t at);         /* ci, cs, off: the chunk holding at (W.n > 0); tables mapped */
unsigned char FASTCALL res_i(unsigned int i);   /* chunk i in RAM (read back if it was not): its bank */
#define res() res_i(ci)

/* bytes in banks (main RAM: they map the banks; the caller's bank back) */
void bm_move(unsigned char b, unsigned int to, unsigned int from, unsigned int n);
void bm_copy(unsigned char sb, unsigned int so, unsigned char db, unsigned int dof, unsigned int n);
void bm_in(unsigned char b, unsigned int o, const unsigned char *src, unsigned int n);

/* the pager (swap.c) */
#define PG_TX 1                         /* W.pg: the text's, the line table's chunks */
#define PG_LT 2
#define TX_MAX 592                      /* the text's chunks (4 MB of text: 517, and room to edit) */
#define LT_MAX 72                       /* the line table's (65,000 lines: 455 KB) */
extern unsigned int sw_stamp[256];      /* a bank's last use (sw_clk) */
extern unsigned char sw_bd[256];        /* a paged chunk's bank: 1 its bytes have no copy on the card */
extern unsigned char sw_last;           /* the bank last touched */
extern unsigned int sw_clk;
extern unsigned int sw_ci;              /* sw_in's chunk (of W) */
extern unsigned char sw_pin1, sw_pin2;  /* banks an edit is using: not put out */
void sw_in(void);                       /* W's chunk sw_ci into RAM */
void sw_age(void);                      /* sw_clk wrapped */

/* swapwh.c (overlay 12): where the paged chunks are on the card */
void FASTCALL wh_dirty(unsigned char pg, unsigned int i);
void wh_ins(unsigned char pg, unsigned int i, unsigned int n);   /* chunk i new (WH_NONE); n chunks after it */
void wh_del(unsigned char pg, unsigned int i, unsigned int n);   /* chunk i gone; n after it */
unsigned char wh_app(unsigned char pg, unsigned int i, unsigned int l);  /* cb_src at chunk i's l: 1 clean */
void FASTCALL wh_clear(unsigned char pg);
void wh_set(unsigned char pg, unsigned int i, pos_t v);
unsigned char sw_out(unsigned char pg, unsigned int i, unsigned char b, unsigned int n);   /* 0 done */
unsigned char sw_rd(unsigned char pg, unsigned int i, unsigned char b, unsigned int n);    /* 0 done */
#endif
