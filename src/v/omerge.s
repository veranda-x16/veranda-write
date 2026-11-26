; Veranda Write on Veranda: src/oemit.c's pr_merge (overlay 18; DECISIONS
; W71, may be renumbered): what d says nothing of (P_NO, first P_NOS, a
; flag not set, size 0) taken from s. The C took some 640 cycles, called
; three times a run (Word's documents: a run a few words).
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _pr_merge, _e_st_find
.import _st_hl, _st_hh, _sty_n
.import popax
.importzp ptr1, ptr2, tmp1, tmp2, tmp3, tmp4

P_NO = $FF
P_NOS = $80
P_BYTES = 12                    ; (oimp.h's struct prop: the bytes, then
F_FIRST = 12                    ;  first, fset, fval, size)
F_FSET = 13
F_FVAL = 14
F_SIZE = 15

.segment "OVERLAY18"

; void __fastcall__ pr_merge(struct prop *d, const struct prop *s)
_pr_merge:
	sta ptr2
	stx ptr2+1
	jsr popax
	sta ptr1
	stx ptr1+1
	ldy #P_BYTES - 1
@lp:	lda (ptr1),y
	cmp #P_NO
	bne :+
	lda (ptr2),y
	sta (ptr1),y
:	dey
	bpl @lp
	ldy #F_FIRST
	lda (ptr1),y
	cmp #P_NOS
	bne :+
	lda (ptr2),y
	sta (ptr1),y
:	; fval = (d.fval & d.fset) | (s.fval & s.fset & ~d.fset); fset |= s.fset
	ldy #F_FSET
	lda (ptr1),y
	sta tmp3                ; d.fset
	lda (ptr2),y
	sta tmp1                ; s.fset
	lda tmp3
	eor #$FF
	and tmp1
	sta tmp2
	ldy #F_FVAL
	lda (ptr1),y
	and tmp3
	sta tmp4
	lda (ptr2),y
	and tmp2
	ora tmp4
	sta (ptr1),y
	ldy #F_FSET
	lda tmp3
	ora tmp1
	sta (ptr1),y
	ldy #F_SIZE
	lda (ptr1),y
	bne :+
	lda (ptr2),y
	sta (ptr1),y
:	rts

; (W71) unsigned char __fastcall__ st_find(unsigned int h): the first
; style whose name's hash is h, 0xFF for none (the C: some 25 cycles a
; style; an .odt has hundreds, looked for at each paragraph and span)
_e_st_find:
	sta tmp1
	stx tmp2
	ldy #0
@lp:	cpy _sty_n
	bcs @none
	lda _st_hl,y
	cmp tmp1
	beq @lo
@nx:	iny
	bne @lp
@none:	lda #$FF
	ldx #0
	rts
@lo:	lda _st_hh,y
	cmp tmp2
	bne @nx
	tya
	ldx #0
	rts
