; Veranda Write on Veranda: text drawn its own way (main RAM: it maps
; the font cache's banks and the row bank at $A000; DECISIONS W19).
;
; A row of the page is put together in the row bank (rr_bank): RBW bytes a
; pixel row (the page's 68 cards), RBH rows (the band of the line where
; its glyphs are), each glyph's rows ORed in from the font cache (fcache.c:
; a font's glyphs left aligned, a row block of every glyph a row), shifted
; into place through the shift tables (the row bank too: so one bank switch
; a glyph row); bold ORs it again a pixel right, italic shifts each row
; right by (baseline + 2 - row) / 2 as GEOS does (the overhang drawn: the
; kernel's text cuts it at the run's advance, Veranda's g/text.s Flush),
; underline a line under the baseline. Glyphs up to 48 pixels wide: a row in
; pieces of 24. Then rr_blit writes the line's
; rows to both screens through VERA's two data ports (VRAM_FG and VRAM_BG,
; 80 bytes a row, the SDK's x16hw.inc), the rows outside the band blank.
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.include "x16hw.inc"
.export _rr_tables, _rr_clear, _rr_run, _rr_blit, _rr_dot
.export _rr_bank, _rr_x, _rr_s, _rr_n, _rr_top, _rr_mode, _rr_fb, _rr_fb2, _rr_split, _rr_fb3, _rr_split2, _rr_fbase, _rr_rowlen, _rr_h, _rr_a, _rr_w
.export _rr_y, _rr_bt, _rr_bh, _rr_lh, _rr_ylo, _rr_yhi
.importzp ptr1, ptr2, ptr3, ptr4, tmp1, tmp2, tmp3, tmp4

RB      = $A000                 ; the row buffer
RBW     = 68
RBH     = 48
TABS    = $AD00                 ; s = 1-7: b >> s at TABS + (s - 1) * 512, b << (8 - s) 256 on
ROWT    = $BB00                 ; each buffer row's address: low bytes, high at + 64
				; ($BB80-$BFFF: the font slots' widths, fonts.c's WT_ADDR)
NW      = 192                   ; a font's glyphs (slots: CSLOT, cw.h)

.segment "BSS"
_rr_bank:   .res 1              ; the row bank
_rr_x:      .res 2              ; x in the buffer (pixels from the page's left)
_rr_s:      .res 2              ; the run's characters (main RAM)
_rr_n:      .res 1              ; how many
_rr_top:    .res 1              ; the buffer row of the font's row 0 (signed)
_rr_mode:   .res 1              ; 1 bold, 2 italic, 4 underline
_rr_fb:     .res 1              ; the font cache: its bank,
_rr_fb2:    .res 1              ;   its second (a font larger than a bank),
_rr_split:  .res 1              ;   the first row there ($FF: none),
_rr_fb3:    .res 1              ;   its third (W38: a font larger than two),
_rr_split2: .res 1              ;   the first row there ($FF: none),
_rr_fbase:  .res 2              ;   its place there (the offsets' low bytes, their
				;   high bytes at + NW, the rows at + 2 * NW),
_rr_rowlen: .res 2              ;   a row block's bytes,
_rr_h:      .res 1              ;   its rows,
_rr_a:      .res 1              ;   its baseline row,
_rr_w:      .res 2              ;   its widths (main RAM)
_rr_y:      .res 2              ; rr_blit: the line's top on the screen,
_rr_bt:     .res 1              ;   the band's first row in the line,
_rr_bh:     .res 1              ;   its rows,
_rr_lh:     .res 1              ;   the line's rows,
_rr_ylo:    .res 2              ;   the screen rows that may be drawn
_rr_yhi:    .res 2
gw:     .res 1
gb:     .res 1                  ; the glyph's bytes a row
pg:     .res 1                  ; put's: those of this piece (1-3)
ck:     .res 1                  ; the piece's first byte
fbk:    .res 1                  ; the font's bank of this row
gx:     .res 2
r:      .res 1
row:    .res 1
ci:     .res 1
b0:     .res 1
b1:     .res 1
b2:     .res 1
ux:     .res 2
ux2:    .res 2
j:      .res 1
ya:     .res 3

.segment "CODE"

; the shift tables and the row addresses into the row bank (once)
_rr_tables:
	lda $00
	pha
	lda _rr_bank
	sta $00
	lda #>TABS
	sta ptr1+1
	stz ptr1
	ldx #1                  ; s
@s:	ldy #0
@b:	tya                     ; hi: b >> s
	phx
@h:	lsr
	dex
	bne @h
	plx
	sta (ptr1),y
	inc ptr1+1
	tya                     ; lo: b << (8 - s)
	phx
	pha
	txa
	eor #7
	tax
	inx                     ; 8 - s
	pla
@l:	asl
	dex
	bne @l
	plx
	sta (ptr1),y
	dec ptr1+1
	iny
	bne @b
	inc ptr1+1
	inc ptr1+1
	inx
	cpx #8
	bne @s
	lda #<RB                ; the rows
	sta ptr1
	lda #>RB
	sta ptr1+1
	ldy #0
@r:	lda ptr1
	sta ROWT,y
	lda ptr1+1
	sta ROWT + 64,y
	clc
	lda ptr1
	adc #RBW
	sta ptr1
	bcc @r1
	inc ptr1+1
@r1:	iny
	cpy #RBH
	bne @r
	pla
	sta $00
	rts

; the buffer's first rr_bh rows cleared
_rr_clear:
	lda $00
	pha
	lda _rr_bank
	sta $00
	ldx #0
@r:	cpx _rr_bh
	beq @e
	lda ROWT,x
	sta ptr1
	lda ROWT + 64,x
	sta ptr1+1
	lda #0
	ldy #RBW - 1
@c:	sta (ptr1),y
	dey
	bpl @c
	inx
	bra @r
@e:	pla
	sta $00
	rts

; a run of rr_n characters of one font and mode at rr_x (advanced)
_rr_run:
	lda $00
	pha
	lda _rr_s
	sta ptr3
	lda _rr_s+1
	sta ptr3+1
	lda _rr_w
	sta ptr4
	lda _rr_w+1
	sta ptr4+1
	lda _rr_x
	sta ux
	lda _rr_x+1
	sta ux+1
	stz ci
@char:	ldy ci
	cpy _rr_n
	bcc @c1
	jmp @end
@c1:	lda (ptr3),y
	cmp #$7F
	bcc @c2
	cmp #$A0                ; $A0-$FF: slots 96-191 (the Veranda set, V135;
	bcs @c4                 ; '?''s glyph where the font has none)
	lda #'?'                ; ($7F-$9F: ?)
@c2:	sec
	sbc #32
	bcs @c3
	lda #'?' - 32
	bra @c3
@c4:	sbc #64                 ; (c set)
@c3:	tay
	lda (ptr4),y            ; its width
	sta gw
	cpy #0
	bne @g
	jmp @adv                ; (a space: nothing to draw)
@g:	lda gw
	bne @g0
	jmp @adv
@g0:	clc                     ; its bytes a row
	adc #7
	lsr
	lsr
	lsr
	sta gb
	lda _rr_fb              ; its rows: the cache's offset (2 bytes a glyph)
	sta $00
	lda _rr_fbase
	sta ptr1
	lda _rr_fbase+1
	sta ptr1+1
	lda (ptr1),y            ; (y the slot) the offset's low byte
	sta tmp3
	clc
	lda ptr1
	adc #NW
	sta ptr1
	bcc @o0
	inc ptr1+1
@o0:	lda (ptr1),y            ; its high byte
	sta tmp4
	clc                     ; ptr1 = base + 2 * NW + offset
	lda ptr1
	adc #NW
	bcc @o1
	inc ptr1+1
	clc
@o1:	adc tmp3
	sta ptr1
	lda ptr1+1
	adc tmp4
	sta ptr1+1
	lda _rr_fb
	sta fbk
	stz r
	lda _rr_mode            ; not italic, up to 24 pixels: the fast way
	and #2
	bne @row
	lda gb
	cmp #4
	bcs @row
	lda ptr1                ; (bold: twice, the second a pixel right)
	sta ux2
	lda ptr1+1
	sta ux2+1
	jsr fast
	bcs @row                ; (c set: the slow way after all)
	lda _rr_mode
	lsr
	bcc @fx
	lda ux2
	sta ptr1
	lda ux2+1
	sta ptr1+1
	stz r
	lda _rr_fb
	sta fbk
	inc _rr_x
	bne @fy
	inc _rr_x+1
@fy:	jsr fast
	lda _rr_x
	bne @fz
	dec _rr_x+1
@fz:	dec _rr_x
@fx:	jmp @adv
@row:	lda r
	cmp _rr_h
	bcc @r0
	jmp @adv
@r0:	clc
	adc _rr_top
	sta row
	bmi @skip               ; (above the band)
	cmp #RBH
	bcc @in
@skip:	jmp @next               ; (below it)
@in:	lda _rr_x
	sta gx
	lda _rr_x+1
	sta gx+1
	lda _rr_mode
	and #2
	beq @ni
	lda _rr_a               ; italic: (baseline + 2 - row) / 2 to the right
	clc
	adc #2
	sec
	sbc r
	bcc @ni
	lsr
	clc
	adc gx
	sta gx
	bcc @ni
	inc gx+1
@ni:	stz ck
@pc:	ldy ck                  ; a piece of the row: up to 3 bytes (the font's bank mapped)
	lda gb
	sec
	sbc ck
	cmp #4
	bcc @p3
	lda #3
@p3:	sta pg
	lda (ptr1),y
	sta b0
	stz b1
	stz b2
	lda pg
	cmp #2
	bcc @rd
	iny
	lda (ptr1),y
	sta b1
	lda pg
	cmp #3
	bcc @rd
	iny
	lda (ptr1),y
	sta b2
@rd:	lda _rr_bank
	sta $00
	jsr put
	lda _rr_mode
	lsr
	bcc @nb
	inc gx                  ; bold: again, a pixel right
	bne @b1
	inc gx+1
@b1:	jsr put
	lda gx
	bne @b2
	dec gx+1
@b2:	dec gx
@nb:	lda fbk
	sta $00
	lda ck                  ; the next piece: 24 pixels on
	clc
	adc #3
	sta ck
	cmp gb
	bcs @next
	clc
	lda gx
	adc #24
	sta gx
	bcc @pc
	inc gx+1
	bra @pc
@next:	clc
	lda ptr1
	adc _rr_rowlen
	sta ptr1
	lda ptr1+1
	adc _rr_rowlen+1
	sta ptr1+1
	inc r
	lda r                   ; (a font in two or three banks: its rows from
	cmp _rr_split           ; the next one's start)
	beq @sw
	cmp _rr_split2
	bne @nx
	lda _rr_fb3
	bra @sw2
@sw:	lda _rr_fb2
@sw2:	sta fbk
	sta $00
	lda tmp3
	sta ptr1
	lda tmp4
	clc
	adc #$A0
	sta ptr1+1
@nx:	jmp @row
@adv:	lda _rr_mode            ; x on by its width (bold: one more)
	and #1
	clc
	adc gw
	clc
	adc _rr_x
	sta _rr_x
	bcc @a1
	inc _rr_x+1
@a1:	inc ci
	jmp @char
@end:	lda _rr_bank
	sta $00
	lda _rr_mode
	and #4
	beq @done
	lda _rr_top             ; underline: the row under the baseline
	clc
	adc _rr_a
	inc a
	bmi @done
	cmp #RBH
	bcs @done
	sta row
@u:	lda ux                  ; ux .. rr_x - 1, a pixel at a time
	cmp _rr_x
	lda ux+1
	sbc _rr_x+1
	bcs @done
	lda ux
	sta gx
	lda ux+1
	sta gx+1
	jsr pixel
	inc ux
	bne @u
	inc ux+1
	bra @u
@done:	pla
	sta $00
	rts

; the glyph at rr_x, plain, gb <= 3, in one bank (the font's bank fbk,
; its rows at ptr1): its byte and shift, the shift tables patched, the
; rows inside the band found once; then each row: its bytes read, the
; row bank, ORed in (a blank row of a narrow glyph skipped), both
; places on by a row (about 70 cycles a row)
fast:	lda _rr_split
	cmp #$FF
	beq @f00
	sec                     ; (a font in two banks: the slow way)
	rts
@f00:	lda _rr_x+1
	sta tmp2
	lda _rr_x
	lsr tmp2
	ror
	lsr tmp2
	ror
	lsr tmp2
	ror
	sta tmp1                ; the byte
	clc
	adc gb
	cmp #RBW
	bcc @f0
	clc
	rts                     ; (past the page's edge)
@f0:	lda _rr_x
	and #7
	sta ck                  ; the shift (0: on a byte's edge)
	beq @ft
	dec a
	asl
	clc
	adc #>TABS
	sta @h0+2
	sta @h1+2
	sta @h2+2
	inc a
	sta @l0+2
	sta @l1+2
	sta @l2+2
@ft:	lda _rr_top             ; the rows inside the band: from r (0, or
	bpl @t1                 ; those above it skipped) ...
@t0:	clc
	lda ptr1
	adc _rr_rowlen
	sta ptr1
	lda ptr1+1
	adc _rr_rowlen+1
	sta ptr1+1
	inc r
	lda r
	clc
	adc _rr_top
	bmi @t0
@t1:	lda r                   ; ... while top + r < RBH and r < h
	cmp _rr_h
	bcc @t2
	clc
	rts
@t2:	clc
	adc _rr_top
	sta row
	cmp #RBH
	bcc @t3
	clc
	rts
@t3:	lda #RBH                ; pg: the rows (min(h - r, RBH - row))
	sec
	sbc row
	sta pg
	lda _rr_h
	sec
	sbc r
	cmp pg
	bcs @t4
	sta pg
@t4:	lda _rr_bank            ; ptr2: the first row's byte
	sta $00
	ldy row
	lda ROWT,y
	clc
	adc tmp1
	sta ptr2
	lda ROWT + 64,y
	adc #0
	sta ptr2+1
	ldx gb
@fr:	lda fbk
	sta $00
	ldy #0
	lda (ptr1),y
	sta b0
	cpx #2
	bcs @f2
	tay                     ; (one byte: a blank row is nothing to OR)
	bne @fb
	jmp @fn
@f2:	iny
	lda (ptr1),y
	sta b1
	cpx #3
	bcc @fb
	iny
	lda (ptr1),y
	sta b2
@fb:	lda _rr_bank
	sta $00
	ldy #0
	lda ck
	bne @fs
	lda b0                  ; on the edge: as they are
	ora (ptr2),y
	sta (ptr2),y
	cpx #2
	bcc @fn
	iny
	lda b1
	ora (ptr2),y
	sta (ptr2),y
	cpx #3
	bcc @fn
	iny
	lda b2
	ora (ptr2),y
	sta (ptr2),y
	bra @fn
@fs:	phx
	ldx b0
@h0:	lda TABS,x
	ora (ptr2),y
	sta (ptr2),y
	iny
@l0:	lda TABS + 256,x
	ora (ptr2),y
	sta (ptr2),y
	pla
	tax
	cpx #2
	bcc @fn
	phx
	ldx b1
@h1:	lda TABS,x
	ora (ptr2),y
	sta (ptr2),y
	iny
@l1:	lda TABS + 256,x
	ora (ptr2),y
	sta (ptr2),y
	plx
	cpx #3
	bcc @fn
	phx
	ldx b2
@h2:	lda TABS,x
	ora (ptr2),y
	sta (ptr2),y
	iny
@l2:	lda TABS + 256,x
	ora (ptr2),y
	sta (ptr2),y
	plx
@fn:	clc                     ; the next row: both places on
	lda ptr1
	adc _rr_rowlen
	sta ptr1
	lda ptr1+1
	adc _rr_rowlen+1
	sta ptr1+1
	clc
	lda ptr2
	adc #RBW
	sta ptr2
	bcc @fm
	inc ptr2+1
@fm:	dec pg
	beq @fe
	jmp @fr
@fe:	clc
	rts

; a pixel at gx in buffer row 'row' (the row bank mapped)
pixel:	jsr where
	bcs @r
	lda gx
	and #7
	tax
	lda bits,x
	ora (ptr2)
	sta (ptr2)
@r:	rts

bits:	.byte $80, $40, $20, $10, $08, $04, $02, $01

; ptr2 = the byte of gx in buffer row 'row'; c set: past the buffer's end
where:	lda gx+1
	sta tmp2
	lda gx
	lsr tmp2
	ror
	lsr tmp2
	ror
	lsr tmp2
	ror
	sta tmp1
	lda tmp2                ; (past 2047)
	bne @far
	lda tmp1
	cmp #RBW
	bcs @r
	ldy row
	clc
	adc ROWT,y
	sta ptr2
	lda ROWT + 64,y
	adc #0
	sta ptr2+1
	clc
@r:	rts
@far:	sec
	rts

; b0, b1, b2 (gb of them, the glyph's row left aligned) ORed at gx in
; buffer row 'row' (the row bank mapped)
put:	jsr where
	bcs @out
	lda tmp1                ; (the bytes, and the one they spill into, in the row)
	clc
	adc pg
	cmp #RBW
	bcs @out
	lda gx
	and #7
	beq @s0
	dec a                   ; the tables for this shift
	asl
	clc
	adc #>TABS
	sta @h0+2
	sta @h1+2
	sta @h2+2
	inc a
	sta @l0+2
	sta @l1+2
	sta @l2+2
	ldx b0
	ldy #0
@h0:	lda TABS,x
	ora (ptr2),y
	sta (ptr2),y
	iny
@l0:	lda TABS + 256,x
	ora (ptr2),y
	sta (ptr2),y
	lda pg
	cmp #2
	bcc @out
	ldx b1
@h1:	lda TABS,x
	ora (ptr2),y
	sta (ptr2),y
	iny
@l1:	lda TABS + 256,x
	ora (ptr2),y
	sta (ptr2),y
	lda pg
	cmp #3
	bcc @out
	ldx b2
@h2:	lda TABS,x
	ora (ptr2),y
	sta (ptr2),y
	iny
@l2:	lda TABS + 256,x
	ora (ptr2),y
	sta (ptr2),y
@out:	rts
@s0:	ldy #0                  ; (on a byte's edge)
	lda b0
	ora (ptr2),y
	sta (ptr2),y
	iny
	lda b1
	ora (ptr2),y
	sta (ptr2),y
	iny
	lda b2
	ora (ptr2),y
	sta (ptr2),y
	rts

; a list's bullet: 4 x 4 pixels at gx (rr_x), buffer rows rr_top .. + 3
_rr_dot:
	lda $00
	pha
	lda _rr_bank
	sta $00
	lda _rr_top
	sta row
	ldx #4
@r:	phx
	lda row
	bmi @n
	cmp #RBH
	bcs @n
	lda _rr_x
	sta gx
	lda _rr_x+1
	sta gx+1
	ldx #4
@p:	phx
	jsr pixel
	inc gx
	plx
	dex
	bne @p
@n:	inc row
	plx
	dex
	bne @r
	pla
	sta $00
	rts

; the line's rows 0 .. rr_lh - 1 at screen rows rr_y ..: those in rr_ylo
; .. rr_yhi written to both screens (the page's cards 6-73), the band's
; (rr_bt .. + rr_bh) from the buffer, the others blank
_rr_blit:
	lda $00
	pha
	lda _rr_bank
	sta $00
	lda _rr_y               ; ya = rr_y * 80 + 6 (the front screen's address)
	sta ya
	lda _rr_y+1
	sta ya+1
	stz ya+2
	asl ya                  ; * 4
	rol ya+1
	asl ya
	rol ya+1
	clc                     ; + rr_y: * 5
	lda ya
	adc _rr_y
	sta ya
	lda ya+1
	adc _rr_y+1
	sta ya+1
	ldx #4                  ; * 16
@m:	asl ya
	rol ya+1
	rol ya+2
	dex
	bne @m
	clc
	lda ya
	adc #6
	sta ya
	bcc @m1
	inc ya+1
	bne @m1
	inc ya+2
@m1:	lda _rr_y
	sta gx                  ; (gx: the screen row)
	lda _rr_y+1
	sta gx+1
	stz j
@l:	lda j
	cmp _rr_lh
	bcc @l0
	jmp @done
@l0:	lda gx                  ; in rr_ylo .. rr_yhi?
	cmp _rr_ylo
	lda gx+1
	sbc _rr_ylo+1
	bcc @next
	lda _rr_yhi
	cmp gx
	lda _rr_yhi+1
	sbc gx+1
	bcc @next
	stz VERA_CTRL           ; port 0: the front screen
	lda ya
	sta VERA_ADDR_L
	lda ya+1
	sta VERA_ADDR_M
	lda ya+2
	ora #$10
	sta VERA_ADDR_H
	lda #1                  ; port 1: the back screen
	sta VERA_CTRL
	clc
	lda ya
	adc #<VRAM_BG
	sta VERA_ADDR_L
	lda ya+1
	adc #>VRAM_BG
	sta VERA_ADDR_M
	lda ya+2
	adc #^VRAM_BG
	ora #$10
	sta VERA_ADDR_H
	lda j                   ; a band row?
	sec
	sbc _rr_bt
	bcc @blank
	cmp _rr_bh
	bcs @blank
	tay
	lda ROWT,y
	sta ptr1
	lda ROWT + 64,y
	sta ptr1+1
	ldy #0
@c:	lda (ptr1),y
	sta VERA_DATA0
	sta VERA_DATA1
	iny
	cpy #RBW
	bne @c
	bra @next
@blank:	ldy #RBW
@z:	stz VERA_DATA0
	stz VERA_DATA1
	dey
	bne @z
@next:	clc                     ; the next row: + 80
	lda ya
	adc #80
	sta ya
	bcc @n1
	inc ya+1
	bne @n1
	inc ya+2
@n1:	inc gx
	bne @n2
	inc gx+1
@n2:	inc j
	jmp @l
@done:	stz VERA_CTRL
	pla
	sta $00
	rts
