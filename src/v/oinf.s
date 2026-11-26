; Veranda Write on Veranda: inflate's codes (DEFLATE's Huffman-coded
; blocks), for src/ozip.c (overlay 17; DECISIONS W71, may be renumbered).
; The C read a bit at a time through a cc65 function and found each code
; by counts (some 200 cycles a bit, 6.5 s for 300 KB of XML); here the bit
; buffer is a byte with a mark (lsr: the next bit in the carry; 0 after:
; the mark, a byte more), a code's length found by comparing it with the
; first code past its length's (ozip.c's build: llim, dlim) and its
; symbol's place the code plus that length's difference (ldel, ddel):
; some 30 cycles a bit. A match goes to the window and the text by
; v/owin.s (main RAM), the literals before it first.
;
;   zcodes      a block's codes from the text's place zo (literals written
;               there, matches copied) till ze, the block's end (zst
;               ST_HEAD, or ST_DONE after the last: zlast) or an error
;               (zerr); a match not done is zclen, zcdist; the literals
;               from zfl not yet in the window
;   zbits(n)    n bits (up to 13: the headers', ozip.c)
;   zdec_d      a symbol of the distance table (ozip.c's code lengths' code
;               too); -1 for none (zerr)
;   zgetb       the stream's next byte (a stored block's: the bits left
;               passed over first, zhold 0)
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _zcodes, _zbits, _zdec_d, _zgetb, _zcopy
.export _zi, _zn, _zhold, _zerr, _zo, _zfl, _ze, _zclen, _zcdist, _zst, _zlast
.import _iobuf, _zfill, _lsym, _lhi, _dsym, _llim, _ldel, _dlim, _ddel
.import _ow_flush, _ow_copy, _ow_src, _ow_n, _ow_dist
.importzp ptr1

ZBUF = _iobuf                   ; (office.h's)
ST_HEAD = 0
ST_DONE = 3

.segment "OVL17BSS"
_zi:     .res 1                 ; ZBUF's next byte
_zn:     .res 1                 ;   its bytes
_zhold:  .res 1                 ; the bits not yet read, the mark above them
_zerr:   .res 1
_zo:     .res 2                 ; the text's next byte
_zfl:    .res 2                 ;   the first not yet in the window
_ze:     .res 2                 ;   past the last
_zclen:  .res 2                 ; a match not done: its bytes left
_zcdist: .res 2                 ;   its distance
_zst:    .res 1
_zlast:  .res 1
code:    .res 2
sym:     .res 2
val:     .res 2
msk:     .res 2
ysv:     .res 1
xsv:     .res 1

.segment "OVL17RO"
lbase:	.word 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51
	.word 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
lext:	.byte 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4
	.byte 5, 5, 5, 5, 0
dbase:	.word 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513
	.word 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
dext:	.byte 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10
	.byte 11, 11, 12, 12, 13, 13
bitm:	.byte 1, 2, 4, 8, 16, 32, 64, 128

.segment "OVERLAY17"

; unsigned char zgetb(void) (cc65's C wants X 0 with a byte)
_zgetb:	jsr getb
	ldx #0
	rts

; the stream's next byte in A (0 and zerr at its end: ozip.c's zfill);
; X and Y kept
getb:	sty ysv
	ldy _zi
	cpy _zn
	bne @have
	stx xsv
	jsr _zfill              ; (C: ZBUF filled, zi 0; 0: none)
	ldx xsv
	tay
	bne :+
	ldy ysv
	rts                     ; (A 0)
:	ldy #0
@have:	lda ZBUF,y
	iny
	sty _zi
	ldy ysv
	rts

; (zhold was the mark: 0 now) a byte more: its first bit in the carry
refill:	jsr getb
	sec
	ror a
	sta _zhold
	rts

; the next bit in the carry
.macro GETBIT
	lsr _zhold
	bne :+
	jsr refill
:
.endmacro

; val = A bits (LSB first); A, X, Y lost
gbits:	ldx #0
	stx val
	stx val+1
	stx msk+1
	inx
	stx msk
	tax
	beq @r
@lp:	GETBIT
	bcc :+
	lda val
	ora msk
	sta val
	lda val+1
	ora msk+1
	sta val+1
:	asl msk
	rol msk+1
	dex
	bne @lp
@r:	rts

; unsigned int __fastcall__ zcopy(unsigned int n): a stored block's next
; bytes, n at most, to zo as far as ze; how many
_zcopy:	sta val
	stx val+1
	stz msk
	stz msk+1
@lp:	lda msk                 ; (msk: the bytes copied)
	cmp val
	lda msk+1
	sbc val+1
	bcs @r
	lda _zo
	cmp _ze
	lda _zo+1
	sbc _ze+1
	bcs @r
	jsr getb
	ldx _zerr
	bne @r
	ldx _zo
	stx ptr1
	ldx _zo+1
	stx ptr1+1
	sta (ptr1)
	inc _zo
	bne :+
	inc _zo+1
:	inc msk
	bne @lp
	inc msk+1
	bra @lp
@r:	lda msk
	ldx msk+1
	rts

; unsigned int __fastcall__ zbits(unsigned char n)
_zbits:	jsr gbits
	lda val
	ldx val+1
	rts

; a code of the literal and length table: its symbol in sym (0 .. 287),
; carry set for none
declit:	lda #0
	sta code
	sta code+1
	tay
@b:	GETBIT
	rol code
	rol code+1
	iny
	iny
	cpy #32
	bcs @r                  ; (16 bits: none)
	lda code
	cmp _llim,y
	lda code+1
	sbc _llim+1,y
	bcs @b
	clc                     ; its place: code + ldel
	lda code
	adc _ldel,y
	sta sym
	lda code+1
	adc _ldel+1,y
	sta sym+1
	; its symbol: lsym's byte, lhi's bit the ninth
	clc
	lda sym
	adc #<_lsym
	sta ptr1
	lda sym+1
	adc #>_lsym
	sta ptr1+1
	lda sym
	and #7
	tay
	lda sym+1
	lsr a
	lda sym
	ror a
	lsr a
	lsr a
	tax
	lda (ptr1)
	sta sym
	lda _lhi,x
	and bitm,y
	cmp #1                  ; (carry: the bit)
	lda #0
	rol a
	sta sym+1
	clc
@r:	rts

; a code of the distance table: its symbol in A, carry set for none
decdist:
	lda #0
	sta code
	sta code+1
	tay
@b:	GETBIT
	rol code
	rol code+1
	iny
	iny
	cpy #32
	bcs @r
	lda code
	cmp _dlim,y
	lda code+1
	sbc _dlim+1,y
	bcs @b
	clc
	lda code
	adc _ddel,y
	tax
	lda _dsym,x
	clc
@r:	rts

; int zdec_d(void)
_zdec_d:
	jsr decdist
	bcs :+
	ldx #0
	rts
:	lda #1
	sta _zerr
	lda #$FF
	tax
	rts

; the literals from zfl to zo into the window
flush:	lda _zo
	cmp _zfl
	bne :+
	lda _zo+1
	cmp _zfl+1
	beq @r
:	lda _zfl
	sta _ow_src
	lda _zfl+1
	sta _ow_src+1
	sec
	lda _zo
	sbc _zfl
	sta _ow_n
	lda _zo+1
	sbc _zfl+1
	sta _ow_n+1
	jsr _ow_flush
	lda _zo
	sta _zfl
	lda _zo+1
	sta _zfl+1
@r:	rts

bad:	lda #1
	sta _zerr
	rts

_zcodes:
@next:	lda _zerr
	bne @out
	sec                     ; the text full: out
	lda _zo
	sbc _ze
	lda _zo+1
	sbc _ze+1
	bcs @out
	lda _zclen
	ora _zclen+1
	bne @match
	jsr declit
	bcs bad
	lda sym+1
	bne @len
	lda _zo                 ; a literal
	sta ptr1
	lda _zo+1
	sta ptr1+1
	lda sym
	sta (ptr1)
	inc _zo
	bne @next
	inc _zo+1
	bra @next
@out:	rts
@len:	lda sym
	bne :+
	jmp @eob                ; (256: the block's end)
:
	sec
	sbc #1                  ; (257 ..: 0 ..)
	cmp #29
	bcs bad
	tax
	asl a
	pha
	lda lext,x
	jsr gbits
	pla
	tax
	clc
	lda lbase,x
	adc val
	sta _zclen
	lda lbase+1,x
	adc val+1
	sta _zclen+1
	jsr decdist
	bcs bad
	cmp #30
	bcs bad
	tax
	asl a
	pha
	lda dext,x
	jsr gbits
	pla
	tax
	clc
	lda dbase,x
	adc val
	sta _zcdist
	lda dbase+1,x
	adc val+1
	sta _zcdist+1
@match:	jsr flush
	; k = min(ze - zo, zclen)
	sec
	lda _ze
	sbc _zo
	sta _ow_n
	lda _ze+1
	sbc _zo+1
	sta _ow_n+1
	lda _zclen
	cmp _ow_n
	lda _zclen+1
	sbc _ow_n+1
	bcs :+
	lda _zclen
	sta _ow_n
	lda _zclen+1
	sta _ow_n+1
:	lda _zo
	sta _ow_src
	lda _zo+1
	sta _ow_src+1
	lda _zcdist
	sta _ow_dist
	lda _zcdist+1
	sta _ow_dist+1
	sec
	lda _zclen
	sbc _ow_n
	sta _zclen
	lda _zclen+1
	sbc _ow_n+1
	sta _zclen+1
	clc
	lda _zo
	adc _ow_n
	sta _zo
	sta _zfl
	lda _zo+1
	adc _ow_n+1
	sta _zo+1
	sta _zfl+1
	jsr _ow_copy            ; (ow_n 0 after it: read first)
	jmp @next
@eob:	lda #ST_DONE
	ldx _zlast
	bne :+
	lda #ST_HEAD
:	sta _zst
	rts
