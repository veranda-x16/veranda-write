/* Veranda Write: what layout.c (the line table) and layfix.c (the
   wrap) share. Copyright (c) 2026 Marcin Spoczynski. MIT licence (see
   LICENSE). */
#ifndef LAYPRIV_H
#define LAYPRIV_H
extern unsigned int an_i;               /* the anchor: a line, its start */
extern pos_t an_pos;
extern unsigned int ae_i;               /* the last edit's anchor */
extern pos_t ae_pos;
extern unsigned char dirty;             /* the edits since lay_fix */
extern pos_t d_lo, d_hi;
extern long d_delta;
extern unsigned char dm_on;
extern pos_t dm_lo, dm_hi, de_lo, de_hi;
extern long dm_delta;
unsigned int FASTCALL lt_len_of(unsigned int i);
extern unsigned char lw_n;              /* the window of the table: 0 empties it */
#endif
