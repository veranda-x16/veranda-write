; Veranda Write on Veranda: scrolling the view (overlay 5, with view2.c;
; DECISIONS W73). In assembly for the overlay's room: cc65's C for this
; was 1.7 KB.
;
; v_scroll (v_n lines: down > 0, up < 0): the rows that stay are moved on
; both screens (vscroll.s, overlay 10), the rows the move uncovers made
; paper and drawn, a few frames' worth a pass (view2.c's vw_rows: the
; rest in the passes after, the main loop running between; scrolling that
; comes meanwhile joins in, the rows still to draw moving with the rest
; as paper, as long as they are all at the bottom scrolling down, all at
; the top scrolling up). The moved rows keep their spelling marks
; (vr[].mk: W58's "what it was drawn with"), the line table is walked to
; the new top from the old one (lt_start's anchor: an_i, an_pos), and a
; screen or more is vw_page's (the view drawn again a pass at a time).
;
; v_draw (row k): draw_row's, but while the view scrolls (v_scr) the row
; the row bank still holds (put together alone since rr_clear last
; counted rb_gen: no band, no table) is written again from it (v_rb):
; the row cut at the bottom, a line or a notch on, is not put together
; twice.
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _v_scroll, _v_draw, _vp_i, _vp_e
.import _v_n, _top_line, _lt_n, _vn, _vr, _v_more, _an_i, _an_pos, _ta_bot
.import _sp_pend, _clr_y, _dm_on, _rb_gen, _v_rb, _v_scr
.import _draw_row, _view_cursor, _rows_build, _vw_page, _vw_rows, _vw_bar, _vw_move
.importzp ptr1, tmp1, tmp2, tmp3

TY0     = 64 + 6                ; (src/v/cwv.h: the first row's top)
VMAX    = 40
TB_W    = 1023                  ; (src/cw.h: a table row's width)
; struct vrow (cwv.h): pos 0, y 4, n 6, l 8 (len 8, h 10, a 11, s 12,
; f 13, p 14, w 15), g 17, pg 18, mk 20; 21 bytes (view2.c checks it)
V_Y     = 4
V_H     = 10
V_W     = 15
V_G     = 17
V_MK    = 20
V_SZ    = 21

.segment "OVL5BSS"
_vp_i:	.res 1                  ; rows vp_i .. vp_e - 1 still to draw (v_more)
_vp_e:	.res 1
omk:	.res VMAX               ; the rows' marks before the scroll
sn:	.res 2                  ; the lines (signed)
sm:	.res 1                  ; how many
dy:	.res 2                  ; the rows' move (signed)
lim:	.res 2
lo:	.res 1
k:	.res 1
dk:	.res 1
vg:	.res 1
rb_g:	.res 1                  ; the row bank's row: rb_gen when it was put together,
rb_mk:	.res 1                  ;   its marks,
rbimg:	.res 17                 ;   its place, y, number and entry (vr's bytes 0-16)

.segment "OVERLAY5"

; ptr1 = &vr[a] (a < 64)
rowp:	sta tmp1
	stz tmp2
	asl a                   ; 4a
	asl a
	sta tmp3
	asl a                   ; 16a
	rol tmp2
	asl a
	rol tmp2
	clc
	adc tmp3                ; + 4a
	bcc @1
	inc tmp2
	clc
@1:	adc tmp1                ; + a: 21a
	bcc @2
	inc tmp2
	clc
@2:	adc #<_vr
	sta ptr1
	lda tmp2
	adc #>_vr
	sta ptr1+1
	rts

; the next row: ptr1 + 21
rownx:	clc
	lda ptr1
	adc #V_SZ
	sta ptr1
	bcc @r
	inc ptr1+1
@r:	rts

_v_scroll:
	lda _v_n
	sta sn
	lda _v_n+1
	sta sn+1
	bmi @neg
	clc                     ; down: no further than the last line at the top
	lda _top_line
	adc sn
	tax
	lda _top_line+1
	adc sn+1
	cmp _lt_n+1
	bcc @d1
	bne @d0
	cpx _lt_n
	bcc @d1
@d0:	lda _lt_n               ; sn = lt_n - 1 - top_line
	clc
	sbc _top_line
	sta sn
	lda _lt_n+1
	sbc _top_line+1
	sta sn+1
	bmi @ret
	ora sn
	beq @ret
@d1:	lda sn+1
	bne @page
	ldx _vn
	dex
	cpx sn                  ; vn - 1 <= sn: a screen
	bcc @page
	beq @page
	lda _v_more             ; (rows to draw not all at the bottom)
	beq @down
	lda _vp_e
	cmp _vn
	bcc @page
	bra @down
@ret:	rts
@neg:	lda _top_line           ; up: no further than the first line
	clc
	adc sn
	lda _top_line+1
	adc sn+1
	bcs @u1                 ; (top_line + sn >= 0)
	sec
	lda #0
	sbc _top_line
	sta sn
	lda #0
	sbc _top_line+1
	sta sn+1
@u1:	lda sn
	ora sn+1
	beq @ret
	lda sn+1
	cmp #$FF
	bne @page
	lda sn                  ; (-256: a screen too)
	beq @page
	lda #0
	sec
	sbc sn                  ; m
	ldx _vn
	dex
	stx tmp1
	cmp tmp1                ; m >= vn - 1: a screen
	bcs @page
	ldx _v_more             ; (rows to draw not all at the top)
	beq @up0
	ldx _vp_i
	bne @page
@up0:	jmp up
@page:	clc                     ; top_line += sn, the view drawn again
	lda _top_line
	adc sn
	sta _top_line
	lda _top_line+1
	adc sn+1
	sta _top_line+1
	jmp _vw_page

@down:	jsr keep
	lda sn
	sta sm
	jsr rowp                ; dy = vr[m].y - vr[m].g - TY0
	ldy #V_G
	lda (ptr1),y
	sta tmp1
	ldy #V_Y
	lda (ptr1),y
	sec
	sbc tmp1
	tax
	iny
	lda (ptr1),y
	sbc #0
	tay
	txa
	sec
	sbc #TY0
	sta dy
	tya
	sbc #0
	sta dy+1
	ldy #3                  ; the anchor: line top_line + m at vr[m].pos
@an:	lda (ptr1),y
	sta _an_pos,y
	dey
	bpl @an
	clc
	lda _top_line
	adc sm
	sta _top_line
	sta _an_i
	lda _top_line+1
	adc #0
	sta _top_line+1
	sta _an_i+1
	jsr _rows_build
	lda _ta_bot             ; lim = TA_BOT - dy: rows ending below it drawn
	sec
	sbc dy
	sta lim
	lda _ta_bot+1
	sbc dy+1
	sta lim+1
	lda #VMAX               ; lo: rows to draw from (v_more: vp_i - m)
	ldx _v_more
	beq @d3
	lda _vp_i
	sec
	sbc sm
	bcs @d3
	lda #0
@d3:	sta lo
	stz k
	lda #0
	jsr rowp
@dl:	lda k
	cmp _vn
	bcs @dd
	cmp lo
	bcs @dd
	ldy #V_H                ; bottom = y + h - 1 > lim: stop
	lda (ptr1),y
	ldy #V_Y
	clc
	adc (ptr1),y
	tax
	iny
	lda (ptr1),y
	adc #0
	tay
	txa
	bne @d4
	dey
@d4:	dex                     ; y.x = bottom
	cpy lim+1
	bcc @d5
	bne @dd
	cpx lim
	beq @d5
	bcs @dd
@d5:	lda k                   ; moved: its marks
	clc
	adc sm
	tax
	lda omk,x
	ldy #V_MK
	sta (ptr1),y
	inc k
	jsr rownx
	bra @dl
@dd:	lda k
	sta _vp_i
	lda _vn
	sta _vp_e
	sec                     ; the rows go up by dy
	lda #0
	sbc dy
	sta dy
	lda #0
	sbc dy+1
	sta dy+1
	jmp go

; the marks kept, the cursor hidden
keep:	lda #0
	jsr _view_cursor
	lda #0
	jsr rowp
	ldx #0
@k:	cpx _vn
	bcs @r
	ldy #V_MK
	lda (ptr1),y
	sta omk,x
	inx
	jsr rownx
	bra @k
@r:	rts

up:	sta sm
	jsr keep
	lda _top_line           ; the anchor: the old top
	sta _an_i
	lda _top_line+1
	sta _an_i+1
	ldy #3
@an:	lda _vr,y
	sta _an_pos,y
	dey
	bpl @an
	lda _top_line
	sec
	sbc sm
	sta _top_line
	bcs @u2
	dec _top_line+1
@u2:	jsr _rows_build         ; (the bands: the old top row is row m now)
	lda sm
	cmp _vn
	bcc @u7
	jmp _vw_page
@u7:	jsr rowp                ; dy = vr[m].y - vr[m].g - TY0
	ldy #V_G
	lda (ptr1),y
	sta tmp1
	ldy #V_Y
	lda (ptr1),y
	sec
	sbc tmp1
	tax
	iny
	lda (ptr1),y
	sbc #0
	tay
	txa
	sec
	sbc #TY0
	sta dy
	tya
	sbc #0
	sta dy+1
	lda _ta_bot             ; dy >= TA_BOT - TA_TOP - 16: drawn again
	sec
	sbc #64 + 16
	tax
	lda _ta_bot+1
	sbc #0
	cmp dy+1
	bcc @u8
	bne @u3
	cpx dy
	beq @u8
	bcs @u3
@u8:	jmp _vw_page
@u3:	clc                     ; lim = TY0 + dy: rows starting (their bands) above drawn
	lda dy
	adc #TY0
	sta lim
	lda dy+1
	adc #0
	sta lim+1
	stz k
	lda #0
	jsr rowp
@ul:	lda k
	cmp _vn
	bcs @ue
	ldy #V_G                ; y - g < lim: on
	lda (ptr1),y
	sta tmp1
	ldy #V_Y
	lda (ptr1),y
	sec
	sbc tmp1
	tax
	iny
	lda (ptr1),y
	sbc #0
	cmp lim+1
	bcc @u4
	bne @ue
	cpx lim
	bcs @ue
@u4:	inc k
	jsr rownx
	bra @ul
@ue:	lda _v_more             ; (rows still to draw: vp_e + m at most)
	beq @u5
	lda _vp_e
	clc
	adc sm
	cmp _vn
	bcc @u6
	lda _vn
@u6:	cmp k
	bcc @u5
	sta k
@u5:	stz _vp_i
	lda k
	sta _vp_e
	jsr rowp
@um:	lda k                   ; the rows after: moved, their marks
	cmp _vn
	bcs go
	sec
	sbc sm
	tax
	lda omk,x
	ldy #V_MK
	sta (ptr1),y
	inc k
	jsr rownx
	bra @um

; the rows moved by dy, the rest drawn
go:	lda #0
	jsr rowp
	ldx #0
@m:	cpx _vn
	bcs @m2
	ldy #V_MK
	lda (ptr1),y
	bpl @m1
	lda #1
	sta _sp_pend            ; (W58: a row's words still to look up)
@m1:	inx
	jsr rownx
	bra @m
@m2:	lda dy
	ldx dy+1
	jsr _vw_move
	lda #$FF
	sta _clr_y
	sta _clr_y+1
	stz _dm_on
	lda #1
	sta _v_more
	jsr _vw_bar             ; (the thumb first: it says where the view goes)
	lda #0
	jmp _vw_rows

;---------------------------------------------------------------
; void __fastcall__ v_draw(unsigned char k)
;---------------------------------------------------------------
_v_draw:
	sta dk
	jsr rowp
	lda _v_scr
	beq @full
	lda rb_g
	cmp _rb_gen
	bne @full
	ldy #0                  ; its place, number and entry the row bank's?
@c:	cpy #V_Y
	bne @c1
	ldy #V_Y + 2
@c1:	lda (ptr1),y
	cmp rbimg,y
	bne @full
	iny
	cpy #17
	bne @c
	lda rb_mk               ; yes: written again, its marks as they were
	ldy #V_MK
	sta (ptr1),y
	lda #1
	sta _v_rb
	lda dk
	jsr _draw_row
	stz _v_rb
	rts
@full:	lda _rb_gen
	sta vg
	lda dk
	jsr _draw_row
	lda _rb_gen             ; put together alone (rr_clear once): kept
	sec
	sbc vg
	cmp #1
	bne @r
	lda dk
	jsr rowp
	ldy #V_W + 1
	lda (ptr1),y
	cmp #>TB_W
	bne @k
	dey
	lda (ptr1),y
	cmp #<TB_W
	beq @r                  ; (a table's row: its cells, not the bank's)
@k:	lda _rb_gen
	sta rb_g
	ldy #16
@cp:	lda (ptr1),y
	sta rbimg,y
	dey
	bpl @cp
	ldy #V_MK
	lda (ptr1),y
	sta rb_mk
@r:	rts
