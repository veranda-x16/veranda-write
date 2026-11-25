; Veranda Write on Veranda: a name's hash (src/ohelp.c's, tools/oxnames.py's:
; h = 5381, then h = (h * 33) ^ c for each byte, 16 bits) for the .docx and
; .odt readers: assembled twice (OVL 19: dx_hash, 20: od_hash), a copy in
; each reader's overlay. About 60 cycles a byte (cc65's C: some 250).
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.importzp ptr1, tmp1, tmp2, tmp3, tmp4
.if OVL = 19
	.export _dx_hash
_dx_hash = hash
.else
	.export _od_hash
_od_hash = hash
.endif

.segment .sprintf("OVERLAY%d", OVL)

; unsigned int __fastcall__ hash(const char *s)
hash:	sta ptr1
	stx ptr1+1
	lda #<5381
	sta tmp3
	lda #>5381
	sta tmp4
	ldy #0
@lp:	lda (ptr1),y
	beq @done
	pha
	lda tmp3                ; t = h << 5
	sta tmp1
	lda tmp4
	asl tmp1
	rol a
	asl tmp1
	rol a
	asl tmp1
	rol a
	asl tmp1
	rol a
	asl tmp1
	rol a
	sta tmp2
	clc                     ; h += t
	lda tmp3
	adc tmp1
	sta tmp3
	lda tmp4
	adc tmp2
	sta tmp4
	pla                     ; h ^= c
	eor tmp3
	sta tmp3
	iny
	bne @lp
@done:	lda tmp3
	ldx tmp4
	rts
