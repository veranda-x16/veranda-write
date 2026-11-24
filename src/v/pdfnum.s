; Veranda Write on Veranda: a number in decimal for the PDF (pdf.c,
; overlay 15; DECISIONS W47). cc65's 32-bit division took some 1,500
; cycles a digit, and a PDF has a number or two for every run of text:
; this turns the 32 bits into binary-coded decimal in the 6502's decimal
; mode (a bit at a time: shifted out of the number, the BCD doubled with
; it), the leading zero bytes skipped: about 1,500 cycles a number.
;
;   in   _pv: the number (4 bytes, low first)
;   out  _pd: its digits (ASCII, no leading zeros, "0" for 0), a 0 after;
;        a: how many; _pv lost
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _pdf_dec, _pv, _pd

.segment "OVL15BSS"
_pv:	.res 4
bcd:	.res 5                  ; 10 digits, the last two first
_pd:	.res 11

.segment "OVERLAY15"

_pdf_dec:
	stz bcd
	stz bcd+1
	stz bcd+2
	stz bcd+3
	stz bcd+4
	ldx #32
@sk:	lda _pv+3               ; (a top byte of 0: 8 bits fewer to turn)
	bne @go
	lda _pv+2
	sta _pv+3
	lda _pv+1
	sta _pv+2
	lda _pv
	sta _pv+1
	stz _pv
	txa
	sec
	sbc #8
	tax
	bne @sk
	lda #'0'                ; (0)
	sta _pd
	stz _pd+1
	lda #1
	ldx #0
	rts
@go:	sed
@l:	asl _pv
	rol _pv+1
	rol _pv+2
	rol _pv+3
	lda bcd
	adc bcd
	sta bcd
	lda bcd+1
	adc bcd+1
	sta bcd+1
	lda bcd+2
	adc bcd+2
	sta bcd+2
	lda bcd+3
	adc bcd+3
	sta bcd+3
	lda bcd+4
	adc bcd+4
	sta bcd+4
	dex
	bne @l
	cld
	ldy #0                  ; the digits, the most significant first
	ldx #4
@u:	lda bcd,x
	lsr a
	lsr a
	lsr a
	lsr a
	jsr dig
	lda bcd,x
	and #$0F
	jsr dig
	dex
	bpl @u
	lda #0
	sta _pd,y
	tya
	ldx #0
	rts

dig:	bne @nz
	cpy #0
	beq @no                 ; (a leading zero)
@nz:	ora #'0'
	sta _pd,y
	iny
@no:	rts

; ---------------------------------------------------------------------
; pdf_mul: _pm_r = _pm_a (16 bits) x _pm_b (8 bits), 24 bits (the 4th
; byte 0): a run's width, its table units times 4 x its size (cc65's
; 32-bit multiplication: some 1,000 cycles; this about 300)
.export _pdf_mul, _pm_a, _pm_b, _pm_r

.segment "OVL15BSS"
_pm_a:	.res 2
_pm_b:	.res 1
_pm_r:	.res 4
mt:	.res 3

.segment "OVERLAY15"
_pdf_mul:
	stz _pm_r
	stz _pm_r+1
	stz _pm_r+2
	stz _pm_r+3
	lda _pm_a
	sta mt
	lda _pm_a+1
	sta mt+1
	stz mt+2
	ldx #8
@l:	lsr _pm_b
	bcc @s
	clc
	lda _pm_r
	adc mt
	sta _pm_r
	lda _pm_r+1
	adc mt+1
	sta _pm_r+1
	lda _pm_r+2
	adc mt+2
	sta _pm_r+2
@s:	asl mt
	rol mt+1
	rol mt+2
	dex
	bne @l
	rts

; ---------------------------------------------------------------------
; pdf_acc: a line's characters for pdf.c's scan, from _lb[_pa_j] while
; they are characters (a code, a tab or CR, < $20, stops it), _pa_rw
; under $E000 and (writing) _pa_rb not full: each one's width from the
; style's table (_pa_wt: Helvetica's / 4, 0 Courier: 150) added to
; _pa_rw; a space counted (_pa_rs, _pa_pend), any other the last
; visible one (_pa_nsp += _pa_pend, _pa_pend 0, _pa_rv = _pa_rw, _pa_rvs
; = _pa_rs, _pa_rvis 1); writing (_pa_mode), each one put in _pa_rb at
; _pa_rn. _pa_j: where it stopped. The C took some 750 cycles a
; character, a call and 16-bit sums each; this about 60.
.macpack longbranch
.export _pdf_acc
.import _lb, _pa_j, _pa_k, _pa_wt, _pa_mode, _pa_rb, _pa_rn, _pa_rw, _pa_rv
.import _pa_rs, _pa_rvs, _pa_rvis, _pa_pend, _pa_nsp
.importzp ptr1

_pdf_acc:
	lda _pa_wt
	sta ptr1
	lda _pa_wt+1
	sta ptr1+1
	ldx _pa_j
@next:	cpx _pa_k
	jcs @done
	lda _pa_rw+1
	cmp #$E0
	jcs @done               ; (a long run: its width kept first)
	lda _lb,x
	cmp #' '
	jcc @done               ; (a code, a tab, CR)
	ldy _pa_mode
	beq @w
	ldy _pa_rn
	cpy #64
	jcs @done               ; (rb full)
	sta _pa_rb,y
	inc _pa_rn
@w:	ldy ptr1+1
	beq @cour
	cmp #$7F
	bcs @hi
	sbc #$1F                ; (c clear: - $20)
	bra @t
@hi:	cmp #$A0
	bcs @h2
	lda #'?' - $20
	bra @t
@h2:	sbc #$40                ; (c set)
@t:	tay
	lda (ptr1),y
	bra @add
@cour:	lda #150
@add:	clc
	adc _pa_rw
	sta _pa_rw
	bcc @1
	inc _pa_rw+1
@1:	lda _lb,x
	inx
	cmp #' '
	bne @vis
	inc _pa_rs
	inc _pa_pend
	jne @next
	inc _pa_pend+1
	jmp @next
@vis:	clc                     ; a visible one: the spaces before it are between words
	lda _pa_nsp
	adc _pa_pend
	sta _pa_nsp
	lda _pa_nsp+1
	adc _pa_pend+1
	sta _pa_nsp+1
	stz _pa_pend
	stz _pa_pend+1
	lda _pa_rw
	sta _pa_rv
	lda _pa_rw+1
	sta _pa_rv+1
	lda _pa_rs
	sta _pa_rvs
	lda #1
	sta _pa_rvis
	jmp @next
@done:	stx _pa_j
	rts

; ---------------------------------------------------------------------
; pdf_esc: a run's bytes, _pa_rb[_pa_e .. _pa_rn), into iobuf at
; _pdf_obn, '(' ')' '\' escaped with '\', while two bytes still fit
; (510); _pa_e and _pdf_obn after them
.export _pdf_esc
.import _iobuf, _pdf_obn, _pa_e
.importzp ptr2, tmp1

_pdf_esc:
	clc
	lda #<_iobuf
	adc _pdf_obn
	sta ptr2
	lda #>_iobuf
	adc _pdf_obn+1
	sta ptr2+1
	ldx _pa_e
	ldy #0
@l:	cpx _pa_rn
	bcs @d
	tya                     ; (room: obn + y < 510)
	clc
	adc _pdf_obn
	sta tmp1
	lda _pdf_obn+1
	adc #0
	cmp #>510
	bcc @ok
	bne @d
	lda tmp1
	cmp #<510
	bcs @d
@ok:	lda _pa_rb,x
	cmp #'('
	beq @e
	cmp #')'
	beq @e
	cmp #'\'
	bne @p
@e:	pha
	lda #'\'
	sta (ptr2),y
	iny
	pla
@p:	sta (ptr2),y
	iny
	inx
	bra @l
@d:	stx _pa_e
	tya
	clc
	adc _pdf_obn
	sta _pdf_obn
	bcc @r
	inc _pdf_obn+1
@r:	rts

; ---------------------------------------------------------------------
; pdf_pc (a: a byte), pdf_ps (a/x: a string, 0 ended): into iobuf at
; _pdf_obn; when it is full, pdf.c's pdf_flush writes it out
.export _pdf_pc, _pdf_ps
.import _pdf_flush
.importzp ptr3

_pdf_pc:
	tax
	clc
	lda #<_iobuf
	adc _pdf_obn
	sta ptr3
	lda #>_iobuf
	adc _pdf_obn+1
	sta ptr3+1
	txa
	sta (ptr3)
	inc _pdf_obn
	bne @1
	inc _pdf_obn+1
@1:	lda _pdf_obn+1
	cmp #>512
	bne @r
	jmp _pdf_flush
@r:	rts

_pdf_ps:
	sta sp_s
	stx sp_s+1
@l:	lda sp_s
	sta @g+1
	lda sp_s+1
	sta @g+2
@g:	lda $FFFF
	beq @r
	jsr _pdf_pc
	inc sp_s
	bne @l
	inc sp_s+1
	bra @l
@r:	rts

.segment "OVL15BSS"
sp_s:	.res 2
