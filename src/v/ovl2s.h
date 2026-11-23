/* Veranda Write on Veranda: styles.c in overlay 13 (with the pages; overlay 2 until it was full): its entry points
   renamed e_..., called through glue.s's wrappers (its variables, sbuf
   among them, in main RAM: cbuf.c reads into it).
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#define font_index     e_font_index
#define sty_find       e_sty_find
#define sty_reset      e_sty_reset
#define par_find       e_par_find
#define par_reset      e_par_reset
#define fmt_map        e_fmt_map
#define codes_before   e_codes_before
#define par_start      e_par_start
#define par_next       e_par_next
#define par_at         e_par_at
#define style_at       e_style_at
#define style_typing   e_style_typing
#pragma code-name ("OVERLAY13")
#pragma rodata-name ("OVL13RO")
