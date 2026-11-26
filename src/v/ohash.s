; Veranda Write on Veranda: a name's hash (src/ohelp.c's, tools/oxnames.py's:
; h = 5381, then h = (h * 33) ^ c for each byte, 16 bits) for the .docx and
; .odt readers: assembled twice (OVL 19: dx_hash, 20: od_hash), a copy in
; each reader's overlay. About 60 cycles a byte (cc65's C: some 250).
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.importzp ptr1, ptr2, ptr3, ptr4, tmp1, tmp2, tmp3, tmp4
.import _xm_hash, _xm_nlen
.if OVL = 19
	.export _dx_hash, _dx_nlook
_dx_hash = hash
_dx_nlook = nlook
.else
	.export _od_hash, _od_nlook
_od_hash = hash
_od_nlook = nlook
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

; (W71) unsigned char __fastcall__ nlook(const struct ntab *t): src/ohelp.c's,
; the token's name (its hash and length: xm_hash, xm_nlen, v/oxtok.s's)
; in table t: its slot the hash's low bits, then the next ones, to an
; empty one; the name's number, 0 for none (the C: some 360 cycles)
nlook:	sta ptr1
	stx ptr1+1
	ldy #0
	lda (ptr1),y            ; the hashes
	sta ptr2
	iny
	lda (ptr1),y
	sta ptr2+1
	iny
	lda (ptr1),y            ; the lengths
	sta ptr3
	iny
	lda (ptr1),y
	sta ptr3+1
	iny
	lda (ptr1),y            ; the numbers
	sta ptr4
	iny
	lda (ptr1),y
	sta ptr4+1
	iny
	lda (ptr1),y            ; the mask (127 at most)
	sta tmp1
	lda _xm_hash
	and tmp1
	tay
@lp:	lda (ptr4),y
	beq @r                  ; (an empty slot: none)
	sta tmp2
	lda (ptr3),y
	cmp _xm_nlen
	bne @nx
	sty tmp3
	tya
	asl a
	tay
	lda (ptr2),y
	cmp _xm_hash
	bne @nh
	iny
	lda (ptr2),y
	cmp _xm_hash+1
	bne @nh
	lda tmp2
	ldx #0
	rts
@nh:	ldy tmp3
@nx:	iny
	tya
	and tmp1
	tay
	bra @lp
@r:	ldx #0
	rts
