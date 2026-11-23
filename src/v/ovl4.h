/* Veranda Write on Veranda: layfix.c and find.c in overlay 4: its entry points
   renamed e_..., called from elsewhere through glue.c's wrappers.
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#define lay_fix        e_lay_fix
#define lay_all        e_lay_all
#define find_set       e_find_set
#define find_from      e_find_from
#define find_here      e_find_here
#define replace_all    e_replace_all
#define doc_count      e_doc_count
#pragma code-name ("OVERLAY4")
#pragma bss-name ("OVL4BSS")         /* (its variables: none read from elsewhere) */
