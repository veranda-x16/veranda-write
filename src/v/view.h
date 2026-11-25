/* Veranda Write on Veranda: what view.c (main RAM) and view2.c
   (overlay 5) share. Copyright (c) 2026 Marcin Spoczynski. MIT licence
   (see LICENSE). */
#ifndef VIEW_H
#define VIEW_H
#include <vrwin.h>

extern unsigned char dm_on;             /* (layout.c: the damage) */
extern pos_t dm_lo, dm_hi, de_lo, de_hi;
extern long dm_delta;

extern const struct vrwin win;
extern pos_t s_lo, s_hi;                /* the selection as drawn */
extern unsigned int clr_y;              /* the paper from there down (view.c's clear_below) */
extern unsigned int clr_pg;             /* (its page) */
extern unsigned int hf_tot;             /* (the page count the bands were drawn with) */
extern unsigned char cur_on;
extern unsigned int cy;                 /* (the cursor's top as drawn) */
extern pos_t w_hit;
extern int v_n;
void __fastcall__ clip(unsigned char on);
void rows_build(void);                  /* (pgview.c) */
unsigned int __fastcall__ row_bot(unsigned char i);
void row_sel(unsigned char i, pos_t lo, pos_t hi);
void __fastcall__ draw_row(unsigned char i);
void clear_below(void);                 /* (pgview.c) */
unsigned int walk(pos_t p0, const struct line *l, unsigned char mode, pos_t t);

/* view2.c (overlay 5) */
void v_all(void), v_frame(void), v_refresh(void);
void v_fix_cursor(void), v_scroll(void), v_title(void);
#endif
