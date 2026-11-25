; Veranda Write on Veranda: the exporter's templates expanded (overlay
; 22, with src/oparts.c; DECISIONS W52, W54). op_next: the template on
; top of oparts.c's stack, its bytes into the exporter's buffer (iobuf +
; 256, at oe_on) until a value (its number returned; 31: op_x the next
; byte), the buffer full ($FF) or its end (0, the stack popped). A byte
; $80 and up is a piece of the dictionary (d_off, d_bytes: oparts.h),
; copied; one below $20 but $0A a value. About 30 cycles a byte (C: 400).
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _e_op_next
.import _op_at, _op_end, _op_sd, _op_pd, _op_pn, _oe_on, _op_x, _d_off, _d_bytes, _iobuf
.importzp ptr1, ptr2, ptr3, tmp1

OE_N = 255
OBUF = _iobuf + 256

.segment "OVERLAY22"

_e_op_next:
	ldx _op_sd
	bne :+
	lda #0
	tax
	rts
:	dex
	txa
	asl
	tay                     ; (the level's word index)
	sty tmp1
	lda _op_at,y
	sta ptr1
	lda _op_at+1,y
	sta ptr1+1
	lda _op_end,y
	sta ptr2
	lda _op_end+1,y
	sta ptr2+1
	ldx _oe_on
@lp:	cpx #OE_N
	bne :+
	jmp @full
:	lda _op_pn
	beq @t
	lda _op_pd              ; a piece's next bytes
	sta ptr3
	lda _op_pd+1
	sta ptr3+1
@pc:	lda (ptr3)
	sta OBUF,x
	inx
	inc ptr3
	bne :+
	inc ptr3+1
:	dec _op_pn
	beq @pe
	cpx #OE_N
	bne @pc
@pe:	lda ptr3
	sta _op_pd
	lda ptr3+1
	sta _op_pd+1
	jmp @lp
@t:	lda ptr1                ; the template's next byte
	cmp ptr2
	bne @nb
	lda ptr1+1
	cmp ptr2+1
	beq @end
@nb:	lda (ptr1)
	inc ptr1
	bne :+
	inc ptr1+1
:	cmp #$80
	bcs @piece
	cmp #$20
	bcs @ch
	cmp #$0A
	beq @ch
	cmp #31                 ; a value
	bne @val
	lda (ptr1)
	sta _op_x
	inc ptr1
	bne :+
	inc ptr1+1
:	lda #31
@val:	stx _oe_on
	pha
	jsr save
	pla
	ldx #0
	rts
@ch:	sta OBUF,x
	inx
	jmp @lp
@piece:	and #$7F                ; d_off[c - $80]: where it starts, the next: its end
	asl
	tay
	lda _d_off,y
	clc
	adc #<_d_bytes
	sta _op_pd
	lda _d_off+1,y
	adc #>_d_bytes
	sta _op_pd+1
	lda _d_off+2,y
	sec
	sbc _d_off,y
	sta _op_pn              ; (a piece is shorter than 256 bytes)
	jmp @lp
@full:	stx _oe_on
	jsr save
	lda #$FF
	ldx #0
	rts
@end:	stx _oe_on
	dec _op_sd
	lda #0
	tax
	rts

save:	ldy tmp1
	lda ptr1
	sta _op_at,y
	lda ptr1+1
	sta _op_at+1,y
	rts
