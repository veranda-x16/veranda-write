; Veranda Write on Veranda: find's scanner (find.c's fs_scan; the host's
; is the C there): from iobuf[fs_i] on, the first byte whose fold
; (fd_fold) is the pattern's first character, codes passed over (the bytes
; of one the window cut: fs_skip; a character style code's argument kept
; in fs_sty, the style before it in fs_prev; fs_c the kind of the code
; being passed). iobuf[fs_n] holds that character (find.c puts it there:
; a sentinel, so the loop counts nothing). A = 1 found (fs_i there), 0
; none (fs_i = fs_n). 27 cycles a byte.
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _fs_scan
.import _iobuf, _fs_i, _fs_n, _fs_skip, _fs_sty, _fs_c, _fs_prev, _fd_fold, _fd_pat
.importzp ptr1, tmp1, tmp2

.segment "OVERLAY4"

_fs_scan:
	clc                     ; ptr1 = iobuf + fs_i, Y = 0
	lda #<_iobuf
	adc _fs_i
	sta ptr1
	lda #>_iobuf
	adc _fs_i+1
	sta ptr1+1
	ldy #0
	lda _fs_skip
	bne @skip
@l:	lda (ptr1),y
	cmp #$18
	bcs @ch
	cmp #$10
	bcs @code
@ch:	tax
	lda _fd_fold,x
	cmp _fd_pat
	beq @hit
	iny
	bne @l
	inc ptr1+1
	bra @l
@hit:	jsr where               ; the sentinel: none
	lda _fs_i
	cmp _fs_n
	lda _fs_i+1
	sbc _fs_n+1
	bcs @none
	lda #1
	ldx #0
	rts
@none:	lda _fs_n
	sta _fs_i
	lda _fs_n+1
	sta _fs_i+1
	lda #0
	tax
	rts
@code:	sta _fs_c               ; a code: its two bytes more passed over
	lda #2
	sta _fs_skip
	jsr step
@skip:	jsr where               ; (the window's end: the rest in the next)
	lda _fs_i
	cmp _fs_n
	lda _fs_i+1
	sbc _fs_n+1
	bcs @none
	lda _fs_skip
	cmp #2
	bne @s1
	lda _fs_c               ; (the argument: a style code's kept)
	cmp #$10
	bne @s1
	lda _fs_sty
	sta _fs_prev
	lda (ptr1),y
	sta _fs_sty
@s1:	jsr step
	dec _fs_skip
	bne @skip
	bra @l

step:	iny
	bne @1
	inc ptr1+1
@1:	rts

; fs_i = ptr1 + Y - iobuf
where:	tya
	clc
	adc ptr1
	sta tmp1
	lda ptr1+1
	adc #0
	sta tmp2
	sec
	lda tmp1
	sbc #<_iobuf
	sta _fs_i
	lda tmp2
	sbc #>_iobuf
	sta _fs_i+1
	rts
