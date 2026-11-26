; Veranda Write on Veranda: a font row's glyphs, for the font cache
; (v/fcache.c, overlay 8; DECISIONS W71, may be renumbered). The C took
; some 2.4 s for Gate Sans 27 (each glyph's bytes shifted by cc65's
; runtime, a bk_wr a glyph); this takes the glyphs of the bitmap's row in
; iobuf (the window: fx_have bytes from the row's byte fx_wb, two 0s
; after) one after another from fx_k, each left aligned, (width + 7) / 8
; bytes, into fx_gl (main RAM, FX_GL bytes at most: fcache.c writes them
; to the font's bank) at fx_n.
;
;   unsigned char fc_xrow(void): 0 the row done; 1 fx_gl full (written
;   out, fx_n 0, called again); 2 the next glyph is past the window
;   (fx_wb its first byte: the window read from there, called again)
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _fc_xrow, _fx_idx, _fx_k, _fx_n, _fx_wb, _fx_have, _fx_setw, _fx_ng
.import _iobuf
.importzp ptr1, ptr2, tmp1, tmp2, tmp3, tmp4

FX_GL = 128                     ; (fcache.c's: iobuf + 384)
fx_gl = _iobuf + 384

.segment "OVL8BSS"
_fx_idx:  .res 386              ; each glyph's first pixel in the row (193 words)
_fx_k:    .res 1                ; the next glyph
_fx_n:    .res 1                ; fx_gl's bytes
_fx_wb:   .res 2                ; the window's first byte in the row
_fx_have: .res 2                ; its bytes
_fx_setw: .res 2                ; the row's bytes
_fx_ng:   .res 1                ; the glyphs
gw:       .res 1                ; the glyph's width
gb:       .res 1                ; its bytes
bb:       .res 2                ; its first byte in the row

.segment "OVERLAY8"

_fc_xrow:
@glyph:	lda _fx_k
	cmp _fx_ng
	bcc :+
	lda #0                  ; (the row done)
	tax                     ; (cc65's C wants X 0 with a byte)
	rts
:	asl a                   ; ptr1 = idx + 2k
	sta ptr1
	lda #0
	rol a
	sta ptr1+1
	clc
	lda ptr1
	adc #<_fx_idx
	sta ptr1
	lda ptr1+1
	adc #>_fx_idx
	sta ptr1+1
	; w = idx[k + 1] - idx[k] (48 at most); bit = idx[k]
	ldy #2
	lda (ptr1),y            ; idx[k + 1]
	ldy #0
	sec
	sbc (ptr1),y
	sta gw
	clc
	adc #7
	lsr
	lsr
	lsr
	sta gb
	bne :+
	inc _fx_k               ; (a space: no bytes)
	bne @glyph
:	lda (ptr1),y
	sta tmp3                ; (bit & 7: the shift)
	sta bb
	iny
	lda (ptr1),y
	lsr
	ror bb
	lsr
	ror bb
	lsr
	ror bb
	sta bb+1
	; past the window: b + gb + 1 > wb + have, and wb + have < setw
	clc
	lda _fx_wb
	adc _fx_have
	sta tmp1
	lda _fx_wb+1
	adc _fx_have+1
	sta tmp2
	sec                     ; (b + gb + 1: b + gb with the carry)
	lda bb
	adc gb
	tax
	lda bb+1
	adc #0
	cmp tmp2
	bne :+
	cpx tmp1
:	beq @in
	bcc @in
	lda tmp1                ; (and the row goes on past the window)
	cmp _fx_setw
	lda tmp2
	sbc _fx_setw+1
	bcs @in
	lda bb
	sta _fx_wb
	lda bb+1
	sta _fx_wb+1
	lda #2
	ldx #0
	rts
@in:	; room in fx_gl
	lda _fx_n
	clc
	adc gb
	cmp #FX_GL + 1
	bcc :+
	lda #1
	ldx #0
	rts
:	; p = iobuf + (b - wb)
	sec
	lda bb
	sbc _fx_wb
	tax
	lda bb+1
	sbc _fx_wb+1
	sta ptr2+1
	txa
	clc
	adc #<_iobuf
	sta ptr2
	lda ptr2+1
	adc #>_iobuf
	sta ptr2+1
	lda tmp3
	and #7
	sta tmp3
	ldy #0
	ldx _fx_n
@byte:	iny
	lda (ptr2),y            ; p[n + 1]
	dey
	sta tmp4
	lda (ptr2),y            ; p[n]
	sta tmp2
	lda tmp4
	stx tmp1
	ldx tmp3
	beq @sh0
@sh:	asl a
	rol tmp2
	dex
	bne @sh
@sh0:	ldx tmp1
	lda tmp2
	sta fx_gl,x
	inx
	iny
	cpy gb
	bne @byte
	; the last byte's bits past the width cleared
	lda gw
	and #7
	beq @done
	tay
	lda #$FF
:	lsr a
	dey
	bne :-
	eor #$FF
	and fx_gl-1,x
	sta fx_gl-1,x
@done:	stx _fx_n
	inc _fx_k
	jmp @glyph
