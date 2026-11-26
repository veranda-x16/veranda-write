; Veranda Write on Veranda: the XML tokenizer's loops over bytes, for
; src/oxml.c (overlay 17; DECISIONS W71, may be renumbered). cc65's C
; took some 60-100 cycles a byte (a call for each, the byte put back
; tested as an int); here a byte of XIN is some 20 cycles in line, the
; others through getc (the byte put back, XIN filled again by oxml.c's
; xfill). A name's hash (src/ohelp.c's: h = h * 33 ^ c) is made as it is
; read, for the readers' look-up (xm_hash, xm_nlen: main RAM). The
; characters past ASCII and the references go through oxml.c's xm_chr.
;
;   int rd(void)                the next byte, EOS (-1) at the part's end
;   int sp(void)                the next byte not a space
;   void xs_name(void)          a name into XNAME, the byte after it put back
;   unsigned char xs_skipa(void)  a start tag's rest: 0 >, 1 />, 2 the end
;   void xs_value(unsigned char q)  a value (after its quote q) into XVAL
;   void xs_text(unsigned char c)   a text from its byte c into XVAL
;   void xs_toend(void)         to the next >
;   unsigned char xs_skipc(void)  content passed over to the next tag
;                               (oxml.c's: 0 the end, 1 an end tag, 2 a
;                               start tag, 3 an empty one, 4 <!, 5 <?)
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _xi, _xn, _txt, _ahead
.export _rd, _sp, _xs_name, _xs_skipa, _xs_value, _xs_text, _xs_toend, _xs_skipc
.import _iobuf, _xfill, _xm_chr, _xm_hash, _xm_nlen, _xm_vlen
.importzp tmp1, tmp2, tmp3, tmp4

XIN = _iobuf + 96               ; (office.h's)
XNAME = _iobuf + 344
XNAMEN = 40
XVAL = _iobuf + 384
XVALN = 128

.segment "OVL17BSS"
_xi:    .res 1
_xn:    .res 1
_txt:   .res 1
_ahead: .res 2                  ; ($FFFF: none)
q:      .res 1
n:      .res 1
t:      .res 1
ch:     .res 1

.segment "OVL17RO"
; the bytes below 'A': 1 a name's end (a space, > / = a quote), 2 a
; start tag's rest's (> / a quote)
x32:	.byte 0, 32, 64, 96, 128, 160, 192, 224
cls:	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
	.byte 0, 3, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 0, 0


.segment "OVERLAY17"

; the next byte in A, the carry clear; the carry set at the part's end
; (Y lost; A, X, Y and the zero page by xfill when XIN is filled)
getc:	lda _ahead+1
	bmi @buf
	lda _ahead
	ldx #$FF
	stx _ahead
	stx _ahead+1
	clc
	rts
@buf:	ldy _xi
	cpy _xn
	bcc @have
	jsr _xfill              ; (A: XIN's bytes, 0 at the end)
	tax
	bne @buf
	sec
	rts
@have:	lda XIN,y
	iny
	sty _xi
	clc
	rts

; getc's common case in line: a byte of XIN, none put back
.macro NEXTC
	ldy _xi
	cpy _xn
	bcs :+
	bit _ahead+1
	bpl :+
	lda XIN,y
	iny
	sty _xi
	clc
	bcc :++
:	jsr getc
:
.endmacro

_rd:	jsr getc
	bcs eos
	ldx #0
	rts
eos:	lda #$FF
	tax
	rts

_sp:
@lp:	NEXTC
	bcs eos
	cmp #' '
	beq @lp
	cmp #$0A
	beq @lp
	cmp #$0D
	beq @lp
	cmp #$09
	beq @lp
	ldx #0
	rts

; A put back
back:	sta _ahead
	stz _ahead+1
	rts

; (the hash in the zero page while a name is read: tmp3, tmp4; the
; table's 8 multiples of 32 for h << 5: some 60 cycles a byte, 90 before)
HL = tmp3
HH = tmp4
_xs_name:
	lda #<5381
	sta HL
	lda #>5381
	sta HH
	stz n
	bit _ahead+1
	bmi @go
	jsr getc                ; (the byte put back first: never XIN filled)
	ldy _xi
	bra @have
@go:	ldy _xi                 ; (XIN's place in Y: _xi when it is left)
@lp:	cpy _xn
	bcs @fill
	lda XIN,y
	iny
@have:	cmp #'A'
	bcs @keep
	tax
	lda cls,x               ; (a space, > / = or a quote: the name's end)
	lsr a
	bcs @termx
	txa
@keep:	ldx n
	cpx #XNAMEN - 1
	bcs @lp
	sta XNAME,x
	inx
	stx n
	sta tmp2                ; h = (h + (h << 5)) ^ c
	lda HH
	and #7
	tax
	lda x32,x
	sta tmp1
	lda HL
	lsr a
	lsr a
	lsr a
	ora tmp1
	sta tmp1                ; ((h << 5) >> 8)
	lda HL
	and #7
	tax
	lda x32,x               ; ((h << 5) & $FF)
	clc
	adc HL
	sta HL
	lda tmp1
	adc HH
	sta HH
	lda HL
	eor tmp2
	sta HL
	jmp @lp
@fill:	sty _xi
	lda HL                  ; (xfill's C: the zero page)
	sta _xm_hash
	lda HH
	sta _xm_hash+1
	jsr getc
	php
	pha
	lda _xm_hash
	sta HL
	lda _xm_hash+1
	sta HH
	pla
	plp
	bcs @eos
	ldy _xi
	bra @have
@termx:	sty _xi
	txa
	jsr back
@eos:	ldx n
	stz XNAME,x
	stx _xm_nlen
	lda HL
	sta _xm_hash
	lda HH
	sta _xm_hash+1
	rts

_xs_skipa:
	bit _ahead+1
	bmi @go
	jsr getc                ; (the byte put back first)
	ldy _xi
	bra @have
@go:	ldy _xi
@scan:	cpy _xn
	bcs @fill
	lda XIN,y
	iny
@have:	cmp #$40
	bcs @scan               ; (letters: on)
	tax
	lda cls,x
	and #2
	beq @scan
	cpx #'>'
	beq @gt
	cpx #'/'
	beq @sl
	stx q                   ; (a quote: to the next)
@ql:	cpy _xn
	bcs @qfill
	lda XIN,y
	iny
	cmp q
	bne @ql
	bra @scan
@fill:	sty _xi
	jsr getc
	bcs @eos
	ldy _xi
	bra @have
@qfill:	sty _xi
	jsr getc
	bcs @eos
	ldy _xi
	cmp q
	bne @ql
	bra @scan
@gt:	sty _xi
	lda #0
	tax
	rts
@sl:	sty _xi
	jsr getc                ; (its >)
	lda #1
	ldx #0
	rts
@eos:	lda #2
	ldx #0
	rts

; void __fastcall__ xs_value(unsigned char q)
_xs_value:
	sta q
	stz _txt
	stz n
@lp:	NEXTC
	bcs @done
	cmp q
	beq @done
	cmp #$80
	bcs @chr
	cmp #'&'
	beq @chr
@st:	ldx n
	cpx #XVALN - 1
	bcs @lp
	sta XVAL,x
	inc n
	bra @lp
@chr:	ldx #0
	jsr _xm_chr             ; (C: the set's byte; EOS at the end)
	cpx #$FF
	bne @st
@done:	ldx n
	stz XVAL,x
	stx _xm_vlen
	rts

; void __fastcall__ xs_text(unsigned char c)
_xs_text:
	stz n
	ldx #1
	stx _txt
@lp:	cmp #'<'
	beq @back
	ldx n
	cpx #XVALN - 4
	bcs @back
	cmp #$80
	bcs @chr
	cmp #'&'
	beq @chr
@st:	ldx n
	sta XVAL,x
	inc n
	NEXTC
	bcc @lp
	bra @done
@chr:	ldx #0
	jsr _xm_chr
	cpx #$FF
	bne @st
	bra @done
@back:	jsr back
@done:	ldx n
	stz XVAL,x
	stx _xm_vlen
	rts

_xs_toend:
@lp:	NEXTC
	bcs @r
	cmp #'>'
	bne @lp
@r:	rts

_xs_skipc:
	lda #1
	sta _txt
@lp:	NEXTC
	bcs @eos
	cmp #'<'
	beq @lt
	cmp #$80
	bcs @chr
	cmp #'&'
	bne @lp
@chr:	ldx #0
	jsr _xm_chr
	cpx #$FF
	bne @lp
@eos:	lda #0
	tax
	rts
@lt:	NEXTC
	bcs @eos
	cmp #'/'
	bne @nend
	jsr _xs_toend
	lda #1
	ldx #0
	rts
@nend:	cmp #'!'
	bne :+
	lda #4
	ldx #0
	rts
:	cmp #'?'
	bne @tag
	lda #5
	ldx #0
	rts
@tag:	cmp #'>'
	bne :+
	lda #2
	ldx #0
	rts
:	cmp #'/'
	bne :+
	jsr getc                ; (its >)
	lda #3
	ldx #0
	rts
:	cmp #'"'
	beq @q
	cmp #$27
	bne @nx
@q:	sta q
@ql:	NEXTC
	bcs @e2
	cmp q
	bne @ql
@nx:	NEXTC
	bcs @e2
	jmp @tag
@e2:	lda #0
	tax
	rts
