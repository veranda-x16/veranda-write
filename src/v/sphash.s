; Veranda Write on Veranda: spelling's hash of a word and the scanner of
; the text's words (overlay 23, for spell.c; W59: the C took some 250
; cycles a letter and 430 a byte).
;
; h1 = h1 * 33 + c, h2 = (h2 rotated left 3) xor c, from h1 = the
; length, h2 = 7 times it (the host's C, spell.c, the same); the set's
; offset (h1 xor h1 >> 9) & 511, times 12.
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.import _sp_lw, _sp_wl
.export _sp_hash, _h1, _h2, _hset

.segment "OVL23BSS"
_h1:	.res 2
_h2:	.res 2
_hset:	.res 2
t:	.res 2

.segment "OVERLAY23"
_sp_hash:
	lda _sp_wl
	sta _h1
	stz _h1+1
	asl                     ; 7 x the length (31 at most: 217)
	asl
	asl
	sec
	sbc _sp_wl
	sta _h2
	stz _h2+1
	ldy #0
@c:	cpy _sp_wl
	beq @done
	lda _h1                 ; h1 * 32
	sta t
	lda _h1+1
	sta t+1
	ldx #5
@s:	asl t
	rol t+1
	dex
	bne @s
	clc                     ; + h1 + c
	lda t
	adc _h1
	sta t
	lda t+1
	adc _h1+1
	sta t+1
	clc
	lda t
	adc _sp_lw,y
	sta _h1
	lda t+1
	adc #0
	sta _h1+1
	ldx #3                  ; h2 rotated left 3
@r:	lda _h2+1
	asl
	rol _h2
	rol _h2+1
	dex
	bne @r
	lda _h2
	eor _sp_lw,y
	sta _h2
	iny
	bra @c
@done:	lda _h1+1               ; set = (h1 ^ h1 >> 9) & 511
	lsr
	eor _h1
	sta t
	lda _h1+1
	and #1
	sta t+1
	asl t                   ; * 4
	rol t+1
	asl t
	rol t+1
	lda t                   ; * 12: * 4 + * 8
	asl
	sta _hset
	lda t+1
	rol
	sta _hset+1
	clc
	lda _hset
	adc t
	sta _hset
	lda _hset+1
	adc t+1
	sta _hset+1
	rts

; ---------------------------------------------------------------------
; the words of a window of the text (lb, main RAM: 128 bytes at most),
; for spell.c's sp_tok (W59: the C took some 430 cycles a byte). From
; tk_wi to tk_wn: when tk_mode is 0 the non-word bytes are passed over
; (codes, 3 bytes each, too) to a word's first letter or digit, its
; offset (tk_base + the index) put in tk_wso; in a word (tk_mode 1) each
; letter or digit goes into sp_word (as it is) and sp_lw (lower case,
; sp_lc), an apostrophe too when a letter follows it, codes are passed
; over; tk_n counts the characters, tk_nb the bytes from its start,
; tk_last the bytes to its last character, tk_dig: a digit in it.
; Returns 1: a word ended (tk_mode 0 again); 0: the window ended (the
; caller reads the next, tk_mode as it is); 3: an apostrophe is the
; window's last byte and more text follows (read on from it).
.import _lb, _sp_lc, _sp_word
.export _tk_wi, _tk_wn, _tk_mode, _tk_n, _tk_dig, _tk_nb, _tk_last, _tk_wso, _tk_base, _tk_more, _sp_scanw

.segment "OVL23BSS"
_tk_wi:   .res 1
_tk_wn:   .res 1
_tk_mode: .res 1
_tk_n:    .res 1
_tk_dig:  .res 1
_tk_more: .res 1                ; text after the window
_tk_nb:   .res 2
_tk_last: .res 2
_tk_wso:  .res 2
_tk_base: .res 2
cv:       .res 1

.segment "OVERLAY23"
_sp_scanw:
	ldx _tk_wi
	lda _tk_mode
	bne inword
skip:	cpx _tk_wn
	bcc :+
	jmp out0
:
	lda _lb,x
	sec
	sbc #$10                ; a code (K_FIRST .. K_LAST): 3 bytes
	cmp #8
	bcs @nk
	inx
	inx
	inx
	jmp skip
@nk:	ldy _lb,x
	lda _sp_lc,y
	bne @st
	tya
	sec
	sbc #'0'
	cmp #10
	bcc @st
	inx
	jmp skip
@st:	lda #1                  ; a word starts at x
	sta _tk_mode
	stz _tk_n
	stz _tk_dig
	stz _tk_nb
	stz _tk_nb+1
	stz _tk_last
	stz _tk_last+1
	txa
	clc
	adc _tk_base
	sta _tk_wso
	lda _tk_base+1
	adc #0
	sta _tk_wso+1
inword:	cpx _tk_wn
	bcc :+
	jmp out0
:
	lda _lb,x
	sec
	sbc #$10
	cmp #8
	bcs @nk
	inx
	inx
	inx
	clc
	lda _tk_nb
	adc #3
	sta _tk_nb
	bcc :+
	inc _tk_nb+1
:	jmp inword
@nk:	ldy _lb,x
	sty cv
	lda _sp_lc,y
	bne letter
	tya
	sec
	sbc #'0'
	cmp #10
	bcs @nd
	lda #1
	sta _tk_dig
	tya
	bra letter
@nd:	cpy #$27                ; an apostrophe between letters
	bne endw
	lda _tk_n
	beq endw
	inx
	cpx _tk_wn
	bcc @la
	dex                     ; (the window's last byte)
	lda _tk_more
	beq endw
	stx _tk_wi
	lda #3
	rts
@la:	ldy _lb,x
	dex
	lda _sp_lc,y
	beq endw
	lda #$27
letter:	ldy _tk_n               ; a = its lower case, cv the byte
	cpy #31
	bcs @long
	sta _sp_lw,y
	lda cv
	sta _sp_word,y
@long:	inc _tk_n
	bne @n1
	dec _tk_n               ; (255 at most: too long anyway)
@n1:	inx
	inc _tk_nb
	bne @n2
	inc _tk_nb+1
@n2:	lda _tk_nb
	sta _tk_last
	lda _tk_nb+1
	sta _tk_last+1
	jmp inword
endw:	stz _tk_mode
	stx _tk_wi
	lda #1
	rts
out0:	stx _tk_wi
	lda #0
	rts
