/* Veranda Write on Veranda: tabedit.c and v/tbui.c in overlay 28: the
   entry points other overlays call renamed e_..., called through
   glue.s's wrappers (v/tbcmd.c's, overlay 29; clip.c's).
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#define tb_delok    e_tb_delok
