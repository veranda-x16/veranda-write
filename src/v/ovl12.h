/* Veranda Write on Veranda: overlay 12 (swapwh.c, swapio.c: where the
   paged chunks are on the card, the swap's files): its entry points
   renamed e_..., called from elsewhere through glue.s.
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#define wh_dirty       e_wh_dirty
#define wh_ins         e_wh_ins
#define wh_del         e_wh_del
#define wh_app         e_wh_app
#define wh_clear       e_wh_clear
#define wh_set         e_wh_set
#define sw_out         e_sw_out
#define sw_rd          e_sw_rd
#define sw_ulout       e_sw_ulout
#define sw_ulin        e_sw_ulin
#define plat_sw_fail   e_plat_sw_fail
#define sw_start       e_sw_start
#define sw_end         e_sw_end
#define sw_doc         e_sw_doc
#define sw_dopen       e_sw_dopen
#define sw_dclose      e_sw_dclose
#pragma code-name ("OVERLAY12")
#pragma rodata-name ("OVL12RO")
#pragma bss-name ("OVL12BSS")
