; Veranda Write on Veranda: the line table's entries read in assembly
; (main RAM; layout.c's C is the host's): lt_get, lt_len_of. The entries
; come from layout.c's window in main RAM (lw: LW entries of LT_SZ bytes
; from line lw_i, lw_n of them); a line outside it is read in by layout.c's
; lw_at, as before. cc65's code took about 1,100 cycles an lt_get (its
; window's test, the multiplication by 7, each field through the C
; stack); a row of the view, a step of lt_find's walk, needs one (DECISIONS
; W43). These take about 150.
;
; struct line: +0 len, +2 h, +3 a, +4 s, +5 f, +6 p, +7 w. An entry:
; +0 the length's low byte; +1 its high bits (0-2), LF_PARA, LF_FIRST
; (3-4), the width's high bits (5-6), LF_PB (7: struct line's f bit 2); +2 the width's low byte, +3 the height, +4 the
; baseline, +5 the style, +6 the paragraph's style.
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _lt_get, _lt_len_of
.import _lw, _lw_i, _lw_n, _lw_at
.importzp c_sp, ptr1, ptr2, tmp1, tmp2
.import incsp2

LW      = 32

.segment "BSS"
lsav:	.res 2

.segment "RODATA"
mul7:
.repeat LW, k
	.byte k * 7
.endrepeat

.segment "CODE"

; ptr1 = line a/x's entry (in the window, else lw_at reads it in)
ent:	sta tmp1
	stx tmp2
	sec
	sbc _lw_i
	tay
	txa
	sbc _lw_i+1
	bne @miss               ; (before the window, or 256 past it)
	cpy _lw_n
	bcs @miss
	lda mul7,y
	clc
	adc #<_lw
	sta ptr1
	lda #>_lw
	adc #0
	sta ptr1+1
	rts
@miss:	lda tmp1
	ldx tmp2
	jsr _lw_at              ; (C: a/x the entry)
	sta ptr1
	stx ptr1+1
	rts

; unsigned int __fastcall__ lt_len_of(unsigned int i)
_lt_len_of:
	jsr ent
	ldy #1
	lda (ptr1),y
	and #7
	tax
	lda (ptr1)
	rts

; void __fastcall__ lt_get(unsigned int i, struct line *l)
_lt_get:
	sta lsav
	stx lsav+1
	ldy #1
	lda (c_sp),y
	tax
	lda (c_sp)
	jsr ent
	lda lsav
	sta ptr2
	lda lsav+1
	sta ptr2+1
	lda (ptr1)              ; the length
	sta (ptr2)
	ldy #1
	lda (ptr1),y
	and #7
	sta (ptr2),y
	lda (ptr1),y            ; LF_: +1 >> 3, LF_PB (its bit 4 now) to bit 2
	lsr
	lsr
	lsr
	tax
	and #$10
	lsr
	lsr
	sta tmp1
	txa
	and #3
	ora tmp1
	ldy #5
	sta (ptr2),y
	txa                     ; the width's high bits
	lsr
	lsr
	and #3
	ldy #8
	sta (ptr2),y
	ldy #2                  ; the width's low byte
	lda (ptr1),y
	ldy #7
	sta (ptr2),y
	ldy #3                  ; the height, the baseline, the style: +2 .. +4
	lda (ptr1),y
	dey
	sta (ptr2),y
	ldy #4
	lda (ptr1),y
	dey
	sta (ptr2),y
	ldy #5
	lda (ptr1),y
	dey
	sta (ptr2),y
	ldy #6                  ; the paragraph's style
	lda (ptr1),y
	sta (ptr2),y
	jmp incsp2
