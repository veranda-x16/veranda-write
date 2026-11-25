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
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _ow_bank, _ow_pos, _ow_n, _ow_dist, _ow_src, _ow_flush, _ow_copy
.importzp ptr1, ptr2, ptr3

.segment "BSS"
_ow_bank: .res 4
_ow_pos:  .res 2
_ow_n:    .res 2
_ow_dist: .res 2
_ow_src:  .res 2
sbank:    .res 1                ; the source's bank (ow_copy)
sx:       .res 1                ; its index in ow_bank
dbank:    .res 1                ; the window's end's bank

.segment "CODE"

; ptr2 and $00: the window at ow_pos
wset:	lda _ow_pos+1
	lsr
	lsr
	lsr
	lsr
	lsr
	and #3
	tax
	lda _ow_bank,x
	sta dbank
	sta $00
	lda _ow_pos
	sta ptr2
	lda _ow_pos+1
	and #$1F
	ora #$A0
	sta ptr2+1
	rts

; ow_pos and ptr2 one on (the next bank after the last byte of one)
wnext:	inc _ow_pos
	bne :+
	inc _ow_pos+1
:	inc ptr2
	bne @r
	inc ptr2+1
	lda ptr2+1
	cmp #$C0
	bne @r
	lda _ow_pos+1
	and #$7F
	sta _ow_pos+1
	jsr wset
@r:	rts

; ow_n -1; Z set when it was 0 (nothing more)
cnt:	lda _ow_n
	ora _ow_n+1
	beq @z
	lda _ow_n
	bne :+
	dec _ow_n+1
:	dec _ow_n
	lda #1                  ; (Z clear)
@z:	rts

_ow_flush:
	lda $00
	pha
	lda _ow_src
	sta ptr1
	lda _ow_src+1
	sta ptr1+1
	jsr wset
	ldy #0
@lp:	jsr cnt
	beq @done
	lda (ptr1),y
	sta (ptr2),y
	inc ptr1
	bne :+
	inc ptr1+1
:	jsr wnext
	jmp @lp
@done:	pla
	sta $00
	rts

_ow_copy:
	lda $00
	pha
	lda _ow_src
	sta ptr1
	lda _ow_src+1
	sta ptr1+1
	; the source: (ow_pos - ow_dist) & $7FFF
	sec
	lda _ow_pos
	sbc _ow_dist
	sta ptr3
	lda _ow_pos+1
	sbc _ow_dist+1
	and #$7F
	tax
	and #$1F
	ora #$A0
	sta ptr3+1
	txa
	lsr
	lsr
	lsr
	lsr
	lsr
	sta sx
	tax
	lda _ow_bank,x
	sta sbank
	jsr wset
	ldy #0
@lp:	jsr cnt
	beq @done
	lda sbank
	sta $00
	lda (ptr3),y
	ldx dbank
	stx $00
	sta (ptr2),y
	sta (ptr1),y
	inc ptr1
	bne :+
	inc ptr1+1
:	jsr wnext
	inc ptr3
	bne @lp
	inc ptr3+1
	lda ptr3+1
	cmp #$C0
	bne @lp
	lda #$A0                ; (the source's next bank)
	sta ptr3+1
	lda sx
	clc
	adc #1
	and #3
	sta sx
	tax
	lda _ow_bank,x
	sta sbank
	jmp @lp
@done:	pla
	sta $00
	rts
