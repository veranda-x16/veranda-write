; Veranda Write on Veranda: 1.0-1.2 documents' Latin-1 in Veranda's set,
; ISO-8859-16 (DECISIONS W38): the 40 places where it has another letter
; made '?' and counted in fmt_lost (format.c; the host's is the C there).
; Overlay 10, with the SDK's vrcs.s, called through glue.s (overlay 2,
; where format.c is, has no room).
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _e_l1_conv, _e_l1_byte
.import _iobuf, _fmt_lost
.importzp ptr1, tmp1, tmp2

.segment "OVERLAY10"

; void l1_conv(unsigned int n): iobuf[0 .. n) (n <= 512) in place
_e_l1_conv:
	sta tmp1
	stx tmp2                ; (whole pages)
	lda #<_iobuf
	sta ptr1
	lda #>_iobuf
	sta ptr1+1
	ldy #0
@pg:	lda tmp2
	beq @last
	ldx #0                  ; (256)
	jsr scan
	inc ptr1+1
	dec tmp2
	bra @pg
@last:	ldx tmp1
	beq @r
	jsr scan
@r:	rts
scan:	lda (ptr1),y
	cmp #$A0
	bcs @hi
@nx:	iny
	dex
	bne scan
	rts
@hi:	jsr one
	sta (ptr1),y
	bra @nx

; unsigned char l1_byte(unsigned char c): c in the set ('?', counted)
_e_l1_byte:
	cmp #$A0
	bcc @r
	jsr one
@r:	ldx #0
	rts

; a >= $A0: a, or '?' (fmt_lost counted); x kept
one:	phx
	tax
	lda tab - $A0,x
	beq @same
	plx
	inc _fmt_lost
	bne @q
	inc _fmt_lost+1
@q:	lda #'?'
	rts
@same:	txa
	plx
	rts

tab:	; 1: Latin-1's character there is not in ISO-8859-16 ($A0-$FF)
	.byte 0,1,1,1,1,1,1,0, 1,0,1,0,1,0,1,1
	.byte 0,0,1,1,1,1,0,0, 1,1,1,0,1,1,1,1
	.byte 0,0,0,1,0,1,0,0, 0,0,0,0,0,0,0,0
	.byte 1,1,0,0,0,1,0,1, 1,0,0,0,0,1,1,0
	.byte 0,0,0,1,0,1,0,0, 0,0,0,0,0,0,0,0
	.byte 1,1,0,0,0,1,0,1, 1,0,0,0,0,1,1,0
