; Veranda Write on Veranda: the file's text checked as it is read
; (format.c's load_text; the host's is the C there): iobuf[ld_from ..
; ld_k) filtered in place to iobuf[0 ..): characters (TAB, CR, $20-$7E,
; $A0-$FF) and whole codes [kind arg kind] (kinds $10-$17; a style code's
; argument a style there is: below nsty; a paragraph code's a paragraph
; style there is, not 0: 1 .. npar - 1) kept, every other byte dropped.
; It stops at a code the buffer's end cuts (ld_i: where; the C goes on
; with it); returns the bytes kept. Six times the C's speed: opening a
; document is mostly this and word wrap.
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _ld_filter, _ld_from, _ld_k, _ld_i
.import _iobuf, _nsty, _npar
.importzp ptr1, ptr2, tmp1, tmp2

.segment "BSS"
_ld_from: .res 2
_ld_k:	.res 2
_ld_i:	.res 2
left:	.res 2

.segment "OVERLAY2"

_ld_filter:
	clc                     ; ptr1: the next byte in, ptr2: the next out
	lda #<_iobuf
	adc _ld_from
	sta ptr1
	lda #>_iobuf
	adc _ld_from+1
	sta ptr1+1
	lda #<_iobuf
	sta ptr2
	lda #>_iobuf
	sta ptr2+1
	sec                     ; left = k - from
	lda _ld_k
	sbc _ld_from
	sta left
	lda _ld_k+1
	sbc _ld_from+1
	sta left+1
@loop:	lda left
	ora left+1
	bne @more
	jmp @end
@more:
	lda (ptr1)
	cmp #$10
	bcc @ctl
	cmp #$18
	bcc @code
	cmp #$20
	bcc @drop
	cmp #$7F
	bcc @keep
	cmp #$A0
	bcs @keep
@drop:	jsr in1
	jmp @loop
@ctl:	cmp #$09
	beq @keep
	cmp #$0D
	bne @drop
@keep:	sta (ptr2)
	inc ptr2
	bne @k1
	inc ptr2+1
@k1:	jsr in1
	jmp @loop
@code:	sta tmp1                ; the kind: three bytes here?
	lda left+1
	bne @c3
	lda left
	cmp #3
	bcs @c3
	jmp @end                ; (cut by the buffer's end: the C's)
@c3:	ldy #2
	lda (ptr1),y
	cmp tmp1
	bne @bad
	dey
	lda (ptr1),y
	sta tmp2
	lda tmp1
	cmp #$11
	beq @ps
	cmp #$10
	bne @good
	lda tmp2
	cmp _nsty
	bcs @bad
	bra @good
@ps:	lda tmp2
	beq @bad
	cmp _npar
	bcs @bad
@good:	lda tmp1
	sta (ptr2)
	ldy #1
	lda tmp2
	sta (ptr2),y
	iny
	lda tmp1
	sta (ptr2),y
	clc
	lda ptr2
	adc #3
	sta ptr2
	bcc @bad
	inc ptr2+1
@bad:	jsr in1                 ; (a bad code: its three bytes dropped)
	jsr in1
	jsr in1
	jmp @loop
@end:	sec                     ; ld_i: where it stopped
	lda ptr1
	sbc #<_iobuf
	sta _ld_i
	lda ptr1+1
	sbc #>_iobuf
	sta _ld_i+1
	sec                     ; the bytes kept
	lda ptr2
	sbc #<_iobuf
	pha
	lda ptr2+1
	sbc #>_iobuf
	tax
	pla
	rts

in1:	inc ptr1
	bne @1
	inc ptr1+1
@1:	lda left
	bne @2
	dec left+1
@2:	dec left
	rts
