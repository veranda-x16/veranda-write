/* Veranda Write on Veranda: overlay 11 (cbufx.c, swap.c: the chunked
   buffer's edits, the pager, the paged cbufs' tables in its BSS): its
   entry points renamed e_..., called from elsewhere through glue.s.
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#define cb_init        e_cb_init
#define cb_initp       e_cb_initp
#define cb_clear       e_cb_clear
#define cb_insert      e_cb_insert
#define cb_delete      e_cb_delete
#define cb_write       e_cb_write
#define cb_append      e_cb_append
#define cb_room        e_cb_room
#define sw_init        e_sw_init
#define sw_in          e_sw_in
#define sw_take        e_sw_take
#define sw_take_lo     e_sw_take_lo
#define sw_age         e_sw_age
#define sw_saved       e_sw_saved
#define sw_aside       e_sw_aside
#pragma code-name ("OVERLAY11")
#pragma rodata-name ("OVL11RO")
#pragma bss-name ("OVL11BSS")         /* (the tables: cbuf.c maps the bank to read them) */
