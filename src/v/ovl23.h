/* Veranda Write on Veranda: overlay 23, spelling (spell.c, v/splive.c,
   v/spio.c): its entry points renamed e_..., called from elsewhere
   through glue.s's wrappers. Its variables initialised (an overlay's BSS
   is not cleared: W48), so in OVL23DATA.
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#define sp_ready   e_sp_ready
#define sp_check   e_sp_check
#define sp_look    e_sp_look
#define sp_probe   e_sp_probe
#define sp_ignore  e_sp_ignore
#define sp_adduser e_sp_adduser
#define sp_flush   e_sp_flush
#define sp_drop    e_sp_drop
#define sp_end     e_sp_end
#define sp_aside   e_sp_aside
#define sp_nodic   e_sp_nodic
#define sp_case    e_sp_case
#define sp_tokat   e_sp_tokat
#define sp_tok     e_sp_tok
#define sp_tokpos  e_sp_tokpos
#define sp_abbr    e_sp_abbr
#define mk_pre     e_mk_pre
#define mk_dots    e_mk_dots
#define sp_idle    e_sp_idle
#define sp_next    e_sp_next
#pragma code-name ("OVERLAY23")
#pragma rodata-name ("OVL23RO")
#pragma bss-name ("OVL23BSS")
#pragma data-name ("OVL23DATA")
