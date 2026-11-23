; Veranda Write on Veranda: the chunked buffer's reads in assembly
; (main RAM; cbuf.c's C is the host's): locate, res_i, cb_byte, cb_read.
; cc65's code for them spent most of its time in the runtime's 32-bit
; helpers (about 800 cycles a cb_byte; DECISIONS W14 named them the next
; to do); these take about 150. They work as cbuf.c's: the cbuf copied
; into W, its cache (ci, cs) used and put back, the tables mapped
; (sw_tbank: the paged cbufs' are overlay 11's) while they are read, a
; chunk on the card read back by sw_in (glue.s: overlay 11), each use of
; a paged chunk's bank stamped for the pager (DECISIONS W29).
;
; struct cbuf: +0 bank, +2 len, +4 max, +6 n, +8 ci, +10 cs, +14 total,
; +18 pg (19 bytes).
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _locate, _res_i, _cb_byte, _cb_read
.import _W, _ci, _cs, _off, _sw_tbank, _sw_last, _sw_clk, _sw_scan, _sw_ci, _sw_stamp
.import _sw_in, _sw_age
.importzp c_sp, sreg, ptr1, ptr2, ptr3
.import incsp2, incsp8

CB_BANK = 0
CB_LEN  = 2
CB_N    = 6
CB_CI   = 8
CB_CS   = 10
CB_TOT  = 14
CB_PG   = 18
CB_SIZE = 19

.segment "BSS"
at:	.res 4                  ; the place
cnt:	.res 2                  ; cb_read: bytes still to read
done:	.res 2                  ; ... read
kk:	.res 2                  ; ... from this chunk
dst:	.res 2
ri:	.res 2
keep:	.res 1                  ; the caller's bank

.segment "CODE"

; ptr3 = &L[ci]
lptr:	lda _ci
	asl
	sta ptr3
	lda _ci+1
	rol
	sta ptr3+1
	clc
	lda ptr3
	adc _W+CB_LEN
	sta ptr3
	lda ptr3+1
	adc _W+CB_LEN+1
	sta ptr3+1
	rts

; locate: at (A/X/sreg) -> ci, cs: the chunk holding it (the last one at
; the end), off; W.n > 0, the tables mapped
_locate:
	sta at
	stx at+1
	lda sreg
	sta at+2
	lda sreg+1
	sta at+3
loc:	lda _ci                 ; ci >= n: from the start
	cmp _W+CB_N
	lda _ci+1
	sbc _W+CB_N+1
	bcc @back
	stz _ci
	stz _ci+1
	stz _cs
	stz _cs+1
	stz _cs+2
	stz _cs+3
@back:	lda at                  ; at < cs: one chunk back
	cmp _cs
	lda at+1
	sbc _cs+1
	lda at+2
	sbc _cs+2
	lda at+3
	sbc _cs+3
	bcs @fwd
	lda _ci
	bne @b1
	dec _ci+1
@b1:	dec _ci
	jsr lptr
	sec
	lda _cs
	sbc (ptr3)
	sta _cs
	ldy #1
	lda _cs+1
	sbc (ptr3),y
	sta _cs+1
	lda _cs+2
	sbc #0
	sta _cs+2
	lda _cs+3
	sbc #0
	sta _cs+3
	bra @back
@fwd:	jsr lptr                ; ci + 1 < n and at >= cs + L[ci]: on
@f1:	lda _ci
	clc
	adc #1
	tax
	lda _ci+1
	adc #0
	cpx _W+CB_N
	sbc _W+CB_N+1
	bcs @here
	clc                     ; ptr1:sreg = cs + L[ci]
	lda _cs
	adc (ptr3)
	sta ptr1
	ldy #1
	lda _cs+1
	adc (ptr3),y
	sta ptr1+1
	lda _cs+2
	adc #0
	sta ptr2
	lda _cs+3
	adc #0
	sta ptr2+1
	lda at
	cmp ptr1
	lda at+1
	sbc ptr1+1
	lda at+2
	sbc ptr2
	lda at+3
	sbc ptr2+1
	bcc @here
	lda ptr1
	sta _cs
	lda ptr1+1
	sta _cs+1
	lda ptr2
	sta _cs+2
	lda ptr2+1
	sta _cs+3
	inc _ci
	bne @f2
	inc _ci+1
@f2:	lda ptr3
	clc
	adc #2
	sta ptr3
	bcc @f1
	inc ptr3+1
	bra @f1
@here:	sec                     ; off = at - cs
	lda at
	sbc _cs
	sta _off
	lda at+1
	sbc _cs+1
	sta _off+1
	rts

; res_i: chunk i (A/X) of W in RAM: its bank (A); the tables mapped
_res_i:	sta ri
	stx ri+1
	clc
	lda _W+CB_BANK
	adc ri
	sta ptr3
	lda _W+CB_BANK+1
	adc ri+1
	sta ptr3+1
	lda (ptr3)
	ldx _W+CB_PG
	beq @r                  ; (not paged)
	cmp #0
	bne @have
	lda ri                  ; on the card: read back (sw_in, overlay 11)
	sta _sw_ci
	lda ri+1
	sta _sw_ci+1
	jsr _sw_in
	clc
	lda _W+CB_BANK
	adc ri
	sta ptr3
	lda _W+CB_BANK+1
	adc ri+1
	sta ptr3+1
	lda (ptr3)
@have:	cmp _sw_last
	beq @r
	sta _sw_last
	inc _sw_clk
	bne @c1
	inc _sw_clk+1
	bne @c1
	jsr _sw_age             ; (the clock wrapped)
	lda _sw_last
@c1:	asl                     ; sw_stamp[b] = sw_scan ? 0 : sw_clk
	sta ptr3
	lda #0
	rol
	sta ptr3+1
	clc
	lda ptr3
	adc #<_sw_stamp
	sta ptr3
	lda ptr3+1
	adc #>_sw_stamp
	sta ptr3+1
	lda _sw_scan
	beq @c2
	lda #0
	sta (ptr3)
	ldy #1
	sta (ptr3),y
	bra @c3
@c2:	lda _sw_clk
	sta (ptr3)
	ldy #1
	lda _sw_clk+1
	sta (ptr3),y
@c3:	lda _sw_last
@r:	ldx #0
	rts

; the cbuf at (ptr2) into W, its cache into ci, cs
wload:	ldy #CB_SIZE - 1
@1:	lda (ptr2),y
	sta _W,y
	dey
	bpl @1
	lda _W+CB_CI
	sta _ci
	lda _W+CB_CI+1
	sta _ci+1
	ldx #3
@2:	lda _W+CB_CS,x
	sta _cs,x
	dex
	bpl @2
	rts

; ci, cs back into the cbuf at (ptr2)
wstore:	ldy #CB_CI
	lda _ci
	sta (ptr2),y
	iny
	lda _ci+1
	sta (ptr2),y
	ldx #0
@1:	iny
	lda _cs,x
	sta (ptr2),y
	inx
	cpx #4
	bne @1
	rts

; at >= the cbuf's total (at (ptr2))? carry set if so
past:	ldy #CB_TOT
	lda at
	cmp (ptr2),y
	iny
	lda at+1
	sbc (ptr2),y
	iny
	lda at+2
	sbc (ptr2),y
	iny
	lda at+3
	sbc (ptr2),y
	rts

; unsigned char cb_byte(struct cbuf *c, pos_t at)
_cb_byte:
	sta at
	stx at+1
	lda sreg
	sta at+2
	lda sreg+1
	sta at+3
	ldy #1
	lda (c_sp),y
	sta ptr2+1
	lda (c_sp)
	sta ptr2
	jsr past
	bcc @in
	lda #0
	ldx #0
	jmp incsp2
@in:	jsr wload
	lda $00
	sta keep
	lda _sw_tbank
	sta $00
	jsr loc
	lda _ci
	ldx _ci+1
	jsr _res_i
	sta $00
	clc
	lda _off
	sta ptr1
	lda _off+1
	adc #$A0
	sta ptr1+1
	lda (ptr1)
	tax
	lda keep
	sta $00
	phx
	ldy #1                  ; (the cbuf again: sw_in may have used ptr2)
	lda (c_sp),y
	sta ptr2+1
	lda (c_sp)
	sta ptr2
	jsr wstore
	pla
	ldx #0
	jmp incsp2

; unsigned int cb_read(struct cbuf *c, pos_t at, unsigned char *dst, unsigned int n)
_cb_read:
	sta cnt
	stx cnt+1
	ldy #7                  ; the stack: dst, at, c
	lda (c_sp),y
	sta ptr2+1
	dey
	lda (c_sp),y
	sta ptr2
	ldx #3
	ldy #5
@a:	lda (c_sp),y
	sta at,x
	dey
	dex
	bpl @a
	lda (c_sp)
	sta dst
	ldy #1
	lda (c_sp),y
	sta dst+1
	stz done
	stz done+1
	jsr past
	bcc @in
	lda #0
	ldx #0
	jmp incsp8
@in:	ldy #CB_TOT             ; cnt = min(cnt, total - at)
	sec
	lda (ptr2),y
	sbc at
	sta ptr1
	iny
	lda (ptr2),y
	sbc at+1
	sta ptr1+1
	iny
	lda (ptr2),y
	sbc at+2
	tax
	iny
	lda (ptr2),y
	sbc at+3
	bne @w
	txa
	bne @w
	lda ptr1
	cmp cnt
	lda ptr1+1
	sbc cnt+1
	bcs @w
	lda ptr1
	sta cnt
	lda ptr1+1
	sta cnt+1
@w:	jsr wload
	lda $00
	sta keep
@loop:	lda cnt
	ora cnt+1
	bne @more
	jmp @end
@more:	lda _sw_tbank
	sta $00
	jsr loc
	jsr lptr                ; kk = L[ci] - off
	sec
	lda (ptr3)
	sbc _off
	sta kk
	ldy #1
	lda (ptr3),y
	sbc _off+1
	sta kk+1
	lda _ci
	ldx _ci+1
	jsr _res_i
	sta $00
	lda cnt                 ; kk = min(kk, cnt)
	cmp kk
	lda cnt+1
	sbc kk+1
	bcs @k
	lda cnt
	sta kk
	lda cnt+1
	sta kk+1
@k:	clc                     ; ptr1 = $A000 + off, ptr2 = dst
	lda _off
	sta ptr1
	lda _off+1
	adc #$A0
	sta ptr1+1
	lda dst
	sta ptr2
	lda dst+1
	sta ptr2+1
	ldy #0
	ldx kk+1
	beq @part
@page:	lda (ptr1),y
	sta (ptr2),y
	iny
	bne @page
	inc ptr1+1
	inc ptr2+1
	dex
	bne @page
@part:	ldx kk
	beq @moved
@byte:	lda (ptr1),y
	sta (ptr2),y
	iny
	dex
	bne @byte
@moved:	clc                     ; dst, at, done += kk; cnt -= kk
	lda dst
	adc kk
	sta dst
	lda dst+1
	adc kk+1
	sta dst+1
	clc
	lda at
	adc kk
	sta at
	lda at+1
	adc kk+1
	sta at+1
	bcc @m1
	inc at+2
	bne @m1
	inc at+3
@m1:	clc
	lda done
	adc kk
	sta done
	lda done+1
	adc kk+1
	sta done+1
	sec
	lda cnt
	sbc kk
	sta cnt
	lda cnt+1
	sbc kk+1
	sta cnt+1
	jmp @loop
@end:	lda keep
	sta $00
	ldy #7
	lda (c_sp),y
	sta ptr2+1
	dey
	lda (c_sp),y
	sta ptr2
	jsr wstore
	lda done
	ldx done+1
	jmp incsp8
