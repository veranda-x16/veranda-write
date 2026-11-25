/* Veranda Write on Veranda: overlay 24, spelling's suggestions
   (sugg.c), its files and banks (v/spio.c): the entry points renamed
   e_..., called through v/spell.s's wrappers.
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#define sp_suggest e_sp_suggest
#define sp_fopen   e_sp_fopen
#define sp_fclose  e_sp_fclose
#define sp_fseek   e_sp_fseek
#define sp_fread   e_sp_fread
#define sp_uload   e_sp_uload
#define sp_usave   e_sp_usave
#define sp_bank    e_sp_bank
#define sp_bfree   e_sp_bfree
#define sp_nbanks  e_sp_nbanks
#define sp_deflang e_sp_deflang
#pragma code-name ("OVERLAY24")
#pragma rodata-name ("OVL24RO")
#pragma bss-name ("OVL24BSS")
#pragma data-name ("OVL24DATA")
