; Veranda Write on Veranda: the overlays' entry points called from
; anywhere (main RAM). Each name is three words: jsr far, the e_ function
; in its overlay, the variable that holds the overlay's bank. far maps the
; overlay (and makes it appBank, for the dialog boxes it may open), calls
; the function with A, X, sreg and the C stack as the caller left them,
; and maps the caller's bank back with the result in A, X, sreg. So a
; wrapper is 7 bytes and costs no copying (glue.c's C wrappers took 1.5 KB
; of main RAM: DECISIONS W17). ovl4.h and ovl6.h rename the functions
; themselves e_...
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.include "veranda.inc"
.importzp ptr1

.import _bank2, _bank4, _bank5, _bank6, _bank7, _bank8, _bank10, _bank11, _bank12, _bank13, _bank14, _bank15

.macro FAR name, target, bank
.export name
.import target
name:	jsr far
	.addr target
	.addr bank
.endmacro

.segment "BSS"
fa:	.res 1
fx:	.res 1

.segment "CODE"

; the ROM's jiffies (RDTIM, $FFDE: 1/60 s; the ROM's interrupt handler
; counts them under Veranda too), the low byte: the cursor's blink clock
; (view.c, DECISIONS W42)
.export _cw_jiffies
_cw_jiffies:
	jsr $FFDE
	ldx #0
	rts

; keys waiting in the ROM's queue (kbdbuf_peek, $FEBD: x the count).
; Veranda's main loop takes one key from it a pass (KeyPoll's GETIN) and
; its own queue is empty again when the program has it, so pressFlag
; alone says nothing of the keys still to come: the drawing waits for
; these too (ui.c's tick, DECISIONS W43)
.export _cw_keys
_cw_keys:
	jsr $FEBD
	txa
	ldx #0
	rts

; Esc waiting in the ROM's key queue (kbdbuf_peek: a the first key, x
; the keys): taken (GETIN), 1; else 0. A long command's stop (the PDF:
; pdfx.c, DECISIONS W47), the keys being the ROM's while the main loop
; waits
.export _cw_esc
_cw_esc:
	jsr $FEBD
	cpx #0
	beq @no
	cmp #$1B
	bne @no
	jsr $FFE4
	lda #1
	ldx #0
	rts
@no:	lda #0
	tax
	rts

far:	sta fa
	stx fx
	pla                     ; the inline words - 1
	sta ptr1
	pla
	sta ptr1+1
	ldy #1
	lda (ptr1),y
	sta @jp+1
	iny
	lda (ptr1),y
	sta @jp+2
	iny
	lda (ptr1),y
	sta @bk+1
	iny
	lda (ptr1),y
	sta @bk+2
	lda $00
	pha
	lda appBank
	pha
@bk:	lda $FFFF
	sta appBank
	sta $00
	lda fa
	ldx fx
@jp:	jsr $FFFF
	sta fa
	pla
	sta appBank
	pla
	sta $00
	lda fa
	rts

; overlay 6: the edits, undo (edit.c, undo.c)
FAR _doc_new, _e_doc_new, _bank6
FAR _doc_insert, _e_doc_insert, _bank6
FAR _doc_insert_raw, _e_doc_insert_raw, _bank6
FAR _doc_delete, _e_doc_delete, _bank6
FAR _doc_tidy, _e_doc_tidy, _bank6
FAR _doc_format, _e_doc_format, _bank6
FAR _doc_all_have, _e_doc_all_have, _bank6
FAR _doc_para, _e_doc_para, _bank6
FAR _doc_named, _e_doc_named, _bank6
FAR _tx_ins, _e_tx_ins, _bank6
FAR _tx_del, _e_tx_del, _bank6
FAR _tx_set, _e_tx_set, _bank6
FAR _ul_init, _e_ul_init, _bank6
FAR _ul_reset, _e_ul_reset, _bank6
FAR _ul_break, _e_ul_break, _bank6
FAR _ul_step, _e_ul_step, _bank6
FAR _ul_undo, _e_ul_undo, _bank6
FAR _ul_redo, _e_ul_redo, _bank6
FAR _ul_can_undo, _e_ul_can_undo, _bank6
FAR _ul_can_redo, _e_ul_can_redo, _bank6
; overlay 13: the styles and paragraphs (styles.c)
FAR _font_index, _e_font_index, _bank13
FAR _sty_find, _e_sty_find, _bank13
FAR _sty_reset, _e_sty_reset, _bank13
FAR _par_find, _e_par_find, _bank13
FAR _par_reset, _e_par_reset, _bank13
FAR _fmt_map, _e_fmt_map, _bank13
FAR _codes_before, _e_codes_before, _bank13
FAR _par_start, _e_par_start, _bank13
FAR _par_next, _e_par_next, _bank13
FAR _par_at, _e_par_at, _bank13
FAR _style_at, _e_style_at, _bank13
FAR _style_typing, _e_style_typing, _bank13
; overlay 4: word wrap, find
FAR _lay_fix, _e_lay_fix, _bank4
FAR _lay_all, _e_lay_all, _bank4
FAR _find_set, _e_find_set, _bank4
FAR _find_from, _e_find_from, _bank4
FAR _find_here, _e_find_here, _bank4
FAR _replace_all, _e_replace_all, _bank4
FAR _doc_count, _e_doc_count, _bank4
; overlay 5: walking a line (view2.c)
FAR _walk, _e_walk, _bank5
FAR _line_x, _e_line_x, _bank5
FAR _line_hit, _e_line_hit, _bank5
FAR _row_sel, _e_row_sel, _bank5
; overlay 8: the font cache
FAR _fc_load, _e_fc_load, _bank8
FAR _fc_reset, _e_fc_reset, _bank8
FAR _fc_setup, _e_fc_setup, _bank8
; overlay 10: geoWrite documents (gwimport.c)
FAR _gw_check, _e_gw_check, _bank10
FAR _gw_load, _e_gw_load, _bank10
FAR _gw_note, _e_gw_note, _bank10
FAR _gw_scan, _e_gw_scan, _bank10
FAR _gw_filter, _e_gw_filter, _bank10
; overlay 7: the SDK's window chrome (vrwin.c, its names o_...: the Makefile)
FAR _f_cs_byte, _cs_byte, _bank10       ; (W38: the SDK's vrcs.s in overlay 10)
FAR _l1_conv, _e_l1_conv, _bank10       ; (W38: v/latin.s)
FAR _l1_byte, _e_l1_byte, _bank10
FAR _l1_note, _e_l1_note, _bank10
FAR _vr_win, _o_vr_win, _bank7
FAR _vr_win_title, _o_vr_win_title, _bank7
FAR _vr_win_status, _o_vr_win_status, _bank7
; overlay 13: the pages (pages.c)
FAR _pg_at, _e_pg_at, _bank13
FAR _pg_idle, _e_pg_idle, _bank13
FAR _pg_first, _e_pg_first, _bank13
; overlay 11: the chunked buffer's edits, the pager (cbufx.c, swap.c)
FAR _cb_insert, _e_cb_insert, _bank11
FAR _cb_delete, _e_cb_delete, _bank11
FAR _cb_write, _e_cb_write, _bank11
FAR _cb_append, _e_cb_append, _bank11
FAR _sw_init, _e_sw_init, _bank11
FAR _sw_in, _e_sw_in, _bank11
FAR _sw_take, _e_sw_take, _bank11
FAR _sw_take_lo, _e_sw_take_lo, _bank11
FAR _sw_age, _e_sw_age, _bank11
FAR _sw_saved, _e_sw_saved, _bank11
FAR _sw_aside, _e_sw_aside, _bank11
; overlay 12: the card's side (swapwh.c, swapio.c)
FAR _wh_dirty, _e_wh_dirty, _bank12
FAR _wh_ins, _e_wh_ins, _bank12
FAR _wh_del, _e_wh_del, _bank12
FAR _wh_app, _e_wh_app, _bank12
FAR _wh_clear, _e_wh_clear, _bank12
FAR _wh_set, _e_wh_set, _bank12
FAR _sw_out, _e_sw_out, _bank12
FAR _sw_rd, _e_sw_rd, _bank12
FAR _sw_ulout, _e_sw_ulout, _bank12
FAR _sw_ulin, _e_sw_ulin, _bank12
FAR _plat_sw_fail, _e_plat_sw_fail, _bank12
FAR _sw_start, _e_sw_start, _bank12
FAR _sw_end, _e_sw_end, _bank12
FAR _sw_doc, _e_sw_doc, _bank12
FAR _sw_dopen, _e_sw_dopen, _bank12
FAR _sw_dclose, _e_sw_dclose, _bank12
; overlay 2: the rest of a document being opened (files.c)
FAR _ld_need, _e_ld_need, _bank2
; overlay 14: the page setup (page.c), the rows, the bands, the header and
; footer (pgview.c); 13: the view's pages; 15: the PDF (W44-W47)
FAR _pgs_default, _e_pgs_default, _bank14
FAR _pg_apply, _e_pg_apply, _bank14
FAR _ld_safe, _e_ld_safe, _bank14
FAR _rows_build, _e_rows_build, _bank14
FAR _clear_below, _e_clear_below, _bank14
FAR _v_band, _e_v_band, _bank14
FAR _v_pbreak, _e_v_pbreak, _bank14
FAR _f_pdf_hf, _e_pdf_hf, _bank14
FAR _pg_view, _e_pg_view, _bank13
FAR _pdf_write, _e_pdf_write, _bank15
