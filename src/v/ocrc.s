; Veranda Write on Veranda: CRC-32 for the ZIP of a .docx or an .odt
; (overlay 21, with src/oexp.c; DECISIONS W52). The table (1 KB, its
; four bytes in four pages) made at the start of an export; then each
; byte: x = (crc ^ b) & $FF, crc = (crc >> 8) ^ T[x], about 40 cycles
; (cc65's C for it: some 400).
;
;   crc_init      the table
;   crc_add(n)    OBUF's (iobuf + 256) first n bytes into oe_crc
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _crc_init, _crc_add, _oe_crc
.import _iobuf
.importzp ptr1

.segment "OVL21BSS"
t0:	.res 256
t1:	.res 256
t2:	.res 256
t3:	.res 256
_oe_crc: .res 4
cnt:	.res 2

.segment "OVERLAY21"

_crc_init:
	ldx #0
@n:	stx _oe_crc
	stz _oe_crc+1
	stz _oe_crc+2
	stz _oe_crc+3
	ldy #8
@b:	lsr _oe_crc+3
	ror _oe_crc+2
	ror _oe_crc+1
	ror _oe_crc
	bcc @nx
	lda _oe_crc+3
	eor #$ED
	sta _oe_crc+3
	lda _oe_crc+2
	eor #$B8
	sta _oe_crc+2
	lda _oe_crc+1
	eor #$83
	sta _oe_crc+1
	lda _oe_crc
	eor #$20
	sta _oe_crc
@nx:	dey
	bne @b
	lda _oe_crc
	sta t0,x
	lda _oe_crc+1
	sta t1,x
	lda _oe_crc+2
	sta t2,x
	lda _oe_crc+3
	sta t3,x
	inx
	bne @n
	rts

_crc_add:
	sta cnt
	stx cnt+1
	lda #<(_iobuf + 256)
	sta ptr1
	lda #>(_iobuf + 256)
	sta ptr1+1
@lp:	lda cnt
	ora cnt+1
	beq @done
	lda (ptr1)
	eor _oe_crc
	tax
	lda _oe_crc+1
	eor t0,x
	sta _oe_crc
	lda _oe_crc+2
	eor t1,x
	sta _oe_crc+1
	lda _oe_crc+3
	eor t2,x
	sta _oe_crc+2
	lda t3,x
	sta _oe_crc+3
	inc ptr1
	bne :+
	inc ptr1+1
:	lda cnt
	bne :+
	dec cnt+1
:	dec cnt
	jmp @lp
@done:	rts
