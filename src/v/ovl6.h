/* Veranda Write on Veranda: overlay 6 (edit.c, undo.c): their entry
   points renamed e_..., called from elsewhere through glue.c's wrappers
   (which map the overlay); calls inside the overlay go straight.
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
#define doc_new        e_doc_new
#define doc_insert     e_doc_insert
#define doc_insert_raw e_doc_insert_raw
#define doc_delete     e_doc_delete
#define doc_tidy       e_doc_tidy
#define doc_format     e_doc_format
#define doc_all_have   e_doc_all_have
#define doc_para       e_doc_para
#define doc_named      e_doc_named
#define tx_ins         e_tx_ins
#define tx_del         e_tx_del
#define tx_set         e_tx_set
#define ul_init        e_ul_init
#define ul_reset       e_ul_reset
#define ul_step        e_ul_step
#define ul_break       e_ul_break
#define ul_undo        e_ul_undo
#define ul_redo        e_ul_redo
#define ul_can_undo    e_ul_can_undo
#define ul_can_redo    e_ul_can_redo
#pragma code-name ("OVERLAY6")
