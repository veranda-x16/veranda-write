; Veranda Write on Veranda: word wrap's inner loop (layfix.c's wrap):
; plain characters (not a space, tab, hyphen, CR or code) from
; wr_rb[wr_ri] while they fit wr_lim, their widths from fl_w (+ fl_bold).
; The C version is layfix.c's (the host's); this one is about six times
; faster, and wrapping is what opening a document and reflowing it cost.
;
;   in   wr_rb, wr_ri, wr_rn, wr_x, wr_any, wr_lim, fl_w, fl_bold
;   out  wr_ri, wr_x, wr_any; wr_n characters taken; wr_ovf 1: stopped at
;        one that does not fit (a line with a character already)
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _wr_scan
.import _wr_rb, _wr_ri, _wr_rn, _wr_x, _wr_n, _wr_any, _wr_ovf
.import _wr_lim, _fl_w, _fl_bold
.importzp ptr1, tmp1

.segment "OVERLAY4"

_wr_scan:
	lda _fl_w
	sta ptr1
	lda _fl_w+1
	sta ptr1+1
	stz _wr_n
	stz _wr_ovf
	ldx _wr_ri
@next:	cpx _wr_rn
	bcs @done
	lda _wr_rb,x
	cmp #'-'
	beq @done
	cmp #' ' + 1
	bcc @done               ; a space, a tab, CR, a code
	cmp #$7F
	bcs @hi                 ; ($7F-$FF: CSLOT's slots, out of the way)
	sbc #32 - 1             ; (c clear: - 32)
@t:
	tay
	lda (ptr1),y
	clc
	adc _fl_bold
	sta tmp1                ; the width
	clc
	adc _wr_x
	tay                     ; (the new x, low)
	lda _wr_x+1
	adc #0
	; past wrap_w? (x + w > wrap_w) and a character on the line already
	cmp _wr_lim+1
	bcc @fits
	bne @over
	cpy _wr_lim
	bcc @fits
	beq @fits
@over:	pha
	lda _wr_any
	beq @take
	pla
	lda #1
	sta _wr_ovf
	bra @done
@take:	pla
@fits:	sta _wr_x+1
	sty _wr_x
	lda #1
	sta _wr_any
	inx
	inc _wr_n
	bra @next
@done:	stx _wr_ri
	rts
@hi:	cmp #$A0                ; $A0-$FF: slots 96-191 (the Veranda set); $7F-$9F '?'
	bcs @h
	lda #'?' - 32
	bra @t
@h:	sbc #64                 ; (c set)
	bra @t
