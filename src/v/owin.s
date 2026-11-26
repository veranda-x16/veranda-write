; Veranda Write on Veranda: inflate's 32 KB window in four RAM banks
; (src/ozip.c, overlay 17; DECISIONS W49). Main RAM: a bank mapped at
; $A000 would map the overlay out, so the window is written and read by
; this code. The window's place p (0 .. 32767) is bank ow_bank[p >> 13],
; address $A000 + (p & $1FFF).
;
;   ow_flush   ow_n bytes from ow_src (main RAM) to the window at ow_pos
;   ow_copy    ow_n bytes from ow_dist back in the window to its end and
;              to ow_src (a match: the bytes may overlap what it writes)
;
; Both leave ow_pos after what they wrote and map the caller's bank back.
; (W71) A run at a time: up to 255 bytes that cross no bank's end, in a
; loop of their own (the source's bank and the window's the same: no
; mapping in it); the places worked out again from ow_pos after each run.
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _ow_bank, _ow_pos, _ow_n, _ow_dist, _ow_src, _ow_flush, _ow_copy
.importzp ptr1, ptr2, ptr3, tmp1, tmp2

.segment "BSS"
_ow_bank: .res 4
_ow_pos:  .res 2
_ow_n:    .res 2
_ow_dist: .res 2
_ow_src:  .res 2
sbank:    .res 1                ; the source's bank (ow_copy)
dbank:    .res 1                ; the window's end's bank

.segment "CODE"
.assert ptr3 = ptr2 + 2, error, "owin.s: ptr3 after ptr2"

; A (high byte of a place, 0 .. $7F) -> ptr+1's $A0-$BF in A, its bank in X
hbank:	pha
	lsr
	lsr
	lsr
	lsr
	lsr
	tax
	lda _ow_bank,x
	tax
	pla
	and #$1F
	ora #$A0
	rts

; the run's length tmp1 cut to the bytes left in ptr's bank (Y: 0 ptr2, 2 ptr3)
cut:	lda ptr2+1,y
	cmp #$BF
	bne @r
	lda #0
	sec
	sbc ptr2,y
	beq @r                  ; (256)
	cmp tmp1
	bcs @r
	sta tmp1
@r:	rts

_ow_flush:
	lda #0
	.byte $2C               ; (bit abs: the next two bytes passed over)
_ow_copy:
	lda #$80
	sta tmp2                ; (bit 7: a copy)
	lda $00
	pha
	lda _ow_src
	sta ptr1
	lda _ow_src+1
	sta ptr1+1
@run:	lda _ow_n
	ora _ow_n+1
	bne :+
	jmp @done
:	; the places: ptr2 the window's end, ptr3 the source
	lda _ow_pos
	sta ptr2
	lda _ow_pos+1
	and #$7F
	sta _ow_pos+1
	jsr hbank
	sta ptr2+1
	stx dbank
	sec
	lda _ow_pos
	sbc _ow_dist
	sta ptr3
	lda _ow_pos+1
	sbc _ow_dist+1
	and #$7F
	jsr hbank
	sta ptr3+1
	stx sbank
	; the run: ow_n, 255 at most, inside both banks
	ldx #255
	lda _ow_n+1
	bne :+
	ldx _ow_n
:	stx tmp1
	ldy #0
	jsr cut
	ldy #0
	bit tmp2
	bpl @fl
	ldy #2
	jsr cut
	ldy #0
	lda sbank
	cmp dbank
	bne @slow
	sta $00
@fast:	lda (ptr3),y
	sta (ptr2),y
	sta (ptr1),y
	iny
	cpy tmp1
	bne @fast
	beq @next
@slow:	lda sbank
	sta $00
	lda (ptr3),y
	ldx dbank
	stx $00
	sta (ptr2),y
	sta (ptr1),y
	iny
	cpy tmp1
	bne @slow
	beq @next
@fl:	lda dbank
	sta $00
@flp:	lda (ptr1),y
	sta (ptr2),y
	iny
	cpy tmp1
	bne @flp
	; ow_pos, ptr1 on, ow_n less, by the run
@next:	clc
	lda _ow_pos
	adc tmp1
	sta _ow_pos
	bcc :+
	inc _ow_pos+1
:	clc
	lda ptr1
	adc tmp1
	sta ptr1
	bcc :+
	inc ptr1+1
:	sec
	lda _ow_n
	sbc tmp1
	sta _ow_n
	bcs :+
	dec _ow_n+1
:	jmp @run
@done:	lda _ow_pos+1
	and #$7F
	sta _ow_pos+1
	pla
	sta $00
	rts
