; Veranda Write on Veranda: the page's rows moved up or down, VRAM to
; VRAM, and the back screen's text area made the front's again (DECISIONS
; W73: scrolling; overlay 10, called through callb from view2.c).
;
; The kernel's ScrollRect took 63 ms to move the text area a line on
; both screens. Here the page's geometry is fixed: the cards PG_C0 ..
; + PG_CW - 1 (6-73: 68 bytes a row, starting 2 bytes into a group of 4,
; as every row does at 80 bytes a row), the rows stepped by 80; port 1
; reads the source, port 0 writes the target. With VERA FX (firmware
; 0.3.1 or later, DCSEL 63's version, as Veranda's FxCheck reads it) the
; 32-bit cache: 4 reads of DATA1 fill it, one write of DATA0 stores 4
; bytes (the first and last group masked: bytes 0-1 and 2-3 of theirs
; kept); the cache only with interrupts off, one row at a time (as the
; kernel's FX copies, Veranda's V29, V80). Without FX, byte by byte.
; Scrolling moves the front screen only (the back screen is made the
; front's again once the scrolling stops: vs_sync), which halves it.
;
;   e_vs_move: r11 the area's first row, r12 its last, r13 the rows to
;   move by (signed, not 0: < 0 up, the text going up as it does
;   scrolling down), r14L 0 the front screen, 1 both. The rows it
;   uncovers are left as they were (view2.c makes them paper)
;   e_vs_sync: rows r11 .. r12 of the front screen copied to the back
;
;   e_vw_move (view2.c's vw_move, the clip off): the text area's rows
;   TY0 .. TA_BOT moved by r13 lines, those uncovered made paper (r13 0:
;   all of them), on the front screen only (v_stale; mouseVector made
;   cw_mouse), on both while a menu is open (it comes back from the back
;   screen); e_v_sync: the back screen's text area made the front's
;   again, the cursor hidden meanwhile (when the scrolling stops: ui.c's
;   tick; before a key but an arrow key, a button change, Alt+Tab)
;

; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.include "veranda.inc"
.include "x16hw.inc"
.importzp regsave, ptr1, ptr2, ptr3, ptr4, tmp1   ; (scratch here: no C runs inside)
sA      = ptr1                  ; the source row's address (17 bits)
dA      = ptr1 + 3              ; the target row's
st      = ptr1 + 6              ; the step: +80 or -80
k       = regsave               ; (rows left)
scr     = regsave + 2           ; (the source's screen: 0 front, 1 back)
ctl     = regsave + 3           ; (VERA_CTRL's DCSEL)

.export _e_vs_move, _e_vs_sync, _e_vw_move, _e_v_sync, _cw_mouse
.import _ta_bot, _v_stale, _v_sj, _view_cursor, _cw_jiffies, _v_sync

TY0     = 64 + 6                ; (src/v/cwv.h: the first row's top)
PG_L    = 48                    ; (the page's pixels)
PG_R    = 591

PG_C0   = 6                     ; (src/v/cwv.h)
GROUPS  = 16                    ; the middle groups of 4: 2 + 64 + 2 = 68 bytes

.segment "OVL10BSS"
fx:	.res 1                  ; 0 not looked at yet, $80 FX, 1 none
n:	.res 2                  ; rows to move
sR:	.res 2                  ; the first source row,
dR:	.res 2                  ; the first target row
dscr:	.res 1                  ; the target's screen
tmp:	.res 3
both:	.res 1

; (main RAM: mouseVector) a button change: the back screen made the
; front's first if scrolling left it behind (a menu or a box opened by it
; comes back from the back screen), then Veranda's own handling
.segment "CODE"
_cw_mouse:
	lda _v_stale
	beq @d
	jsr _v_sync
@d:	jmp DefMouse

.segment "OVERLAY10"
.assert ptr2 = ptr1 + 2 && ptr3 = ptr1 + 4 && ptr4 = ptr1 + 6 && tmp1 = ptr1 + 8, error, "sA, dA, st: cc65's zero page in a row"

; r11, r12: the text area's rows
area:	lda #TY0
	sta r11
	stz r11+1
	lda _ta_bot
	sta r12
	lda _ta_bot+1
	sta r12+1
	rts

; the text area moved by r13 (see the top)
_e_vw_move:
	jsr area
	ldx #0
	lda menuNumber
	beq @1
	inx
@1:	stx both
	stx r14L
	lda r13
	ora r13+1
	beq @p
	jsr _e_vs_move
@p:	lda #ST_WR_FORE | ST_WR_BACK
	ldx both
	bne @2
	lda #<_cw_mouse         ; (the back screen behind: made the front's
	sta mouseVector         ;  before a button's change is handled)
	lda #>_cw_mouse
	sta mouseVector+1
	lda #1
	sta _v_stale
	lda #ST_WR_FORE
@2:	sta dispBufferOn
	jsr _cw_jiffies
	sta _v_sj
	lda #0
	jsr SetPattern
	lda #<PG_L              ; the rows uncovered: paper
	sta r3
	stz r3+1
	lda #<PG_R
	sta r4
	lda #>PG_R
	sta r4+1
	lda r13+1
	bmi @up
	ora r13
	beq @all
	clc                     ; down: TY0 .. TY0 + dy - 1
	lda r11
	adc r13
	sta r12
	lda #0
	adc r13+1
	sta r12+1
	lda r12
	bne @3
	dec r12+1
@3:	dec r12
	bra @all
@up:	sec                     ; up: TA_BOT + 1 + dy .. TA_BOT
	lda r12
	adc r13
	sta r11
	lda r12+1
	adc r13+1
	sta r11+1
@all:	jsr WRectangle
	lda #ST_WR_FORE | ST_WR_BACK
	sta dispBufferOn
	rts

_e_v_sync:
	lda #0
	jsr _view_cursor
	jsr area
	jsr _e_vs_sync
	stz _v_stale
	stz mouseVector
	stz mouseVector+1
	lda #1
	jmp _view_cursor

; rows r11 .. r12, front to back
_e_vs_sync:
	jsr fxcheck
	lda r11
	sta sR
	sta dR
	lda r11+1
	sta sR+1
	sta dR+1
	lda #80
	sta st
	stz st+1
	stz st+2
	lda r12
	sec
	sbc r11
	sta n
	lda r12+1
	sbc r11+1
	sta n+1
	stz scr
	lda #1
	sta dscr
	stz r14L                ; (one pass)
	bra rows

_e_vs_move:
	jsr fxcheck
	lda r13+1               ; |dy|: the rows moved = bottom - top + 1 - |dy|
	bmi @up
	lda r12                 ; down: rows top .. bottom - dy to top + dy ..,
	sec                     ; the last first
	sbc r13
	sta sR
	lda r12+1
	sbc r13+1
	sta sR+1
	lda r12
	sta dR
	lda r12+1
	sta dR+1
	lda #<-80
	sta st
	lda #>-80
	sta st+1
	lda #$FF
	sta st+2
	lda sR                  ; n = (bottom - dy) - top + 1
	sec
	sbc r11
	sta n
	lda sR+1
	sbc r11+1
	sta n+1
	bra @n
@up:	lda r11                 ; up by -dy: rows top - dy .. bottom to top ..
	sec
	sbc r13
	sta sR
	lda r11+1
	sbc r13+1
	sta sR+1
	lda r11
	sta dR
	lda r11+1
	sta dR+1
	lda #80
	sta st
	stz st+1
	stz st+2
	lda r12                 ; n = bottom - (top - dy) + 1
	sec
	sbc sR
	sta n
	lda r12+1
	sbc sR+1
	sta n+1
@n:	stz scr
	stz dscr
rows:	lda n+1                 ; (n - 1 here: nothing to move if < 0)
	bpl @0
	rts
@0:	inc n
	bne @s
	inc n+1
@s:	lda sR                  ; this screen's addresses
	ldy sR+1
	ldx scr
	jsr addr
	sta sA
	stx sA+1
	sty sA+2
	lda dR
	ldy dR+1
	ldx dscr
	jsr addr
	sta dA
	stx dA+1
	sty dA+2
	lda n
	sta k
	lda n+1
	sta k+1
@row:	bit fx
	bmi @x
	jsr plain
	bra @nx
@x:	jsr cache
@nx:	ldx #0                  ; the next row: the steps added
@a:	clc
	lda sA,x
	adc st
	sta sA,x
	lda sA+1,x
	adc st+1
	sta sA+1,x
	lda sA+2,x
	adc st+2
	and #1
	sta sA+2,x
	cpx #dA - sA
	beq @k
	ldx #dA - sA
	bra @a
@k:	lda k
	bne @k1
	dec k+1
@k1:	dec k
	lda k
	ora k+1
	bne @row
	lda r14L                ; both screens: the back screen's turn
	beq @r
	stz r14L
	inc scr
	inc dscr
	jmp @s
@r:	rts

; a = low, y = high of a row, x its screen (1: the back): the row's
; address (a low, x middle, y bit 16): row * 80 + PG_C0 (+ VRAM_BG)
addr:	stx tmp+2
	sta tmp
	sty tmp+1
	asl tmp                 ; * 4
	rol tmp+1
	asl tmp
	rol tmp+1
	clc                     ; + 1: * 5
	adc tmp
	sta tmp
	tya
	adc tmp+1
	sta tmp+1
	lda tmp+2
	pha
	stz tmp+2
	ldx #4                  ; * 16
@m:	asl tmp
	rol tmp+1
	rol tmp+2
	dex
	bne @m
	pla
	beq @f
	clc
	lda tmp+1
	adc #>VRAM_BG
	sta tmp+1
	lda tmp+2
	adc #^VRAM_BG
	sta tmp+2
@f:	clc
	lda tmp
	adc #PG_C0
	pha
	lda tmp+1
	adc #0
	tax
	lda tmp+2
	adc #0
	tay
	pla
	rts

; the ports at the row: port 1 the source (step 1), port 0 the target
; (step a: VERA_INC1, or $30 for the cache's 4)
seek:	pha
	lda #1
	ora ctl
	sta VERA_CTRL
	lda sA
	sta VERA_ADDR_L
	lda sA+1
	sta VERA_ADDR_M
	lda sA+2
	ora #VERA_INC1
	sta VERA_ADDR_H
	lda ctl
	sta VERA_CTRL
	lda dA
	sta VERA_ADDR_L
	lda dA+1
	sta VERA_ADDR_M
	pla
	ora dA+2
	sta VERA_ADDR_H
	rts

; a row byte by byte: 34 times 2
plain:	stz ctl
	lda #VERA_INC1
	jsr seek
	ldx #34
@p:
.repeat 2
	lda VERA_DATA1
	sta VERA_DATA0
.endrepeat
	dex
	bne @p
	rts

; a row through the cache (DCSEL 2): the first group's 2 bytes (the
; cache from its byte 2: FX_MULT's index 2, bits 2-3; the write keeps
; bytes 0 and 1), 16 whole groups, the last group's 2 (bytes 2 and 3 kept)
cache:	lda #2 << 1
	sta ctl
	php
	sei
	lda #$30
	jsr seek
	lda #$60                ; cache writes, the cache filled by the reads
	sta VERA_FX_CTRL
	lda #2 << 2
	sta VERA_FX_MULT
	lda VERA_DATA1
	lda VERA_DATA1
	lda #$0F
	sta VERA_DATA0
	ldx #GROUPS / 2
@g:
.repeat 2
	lda VERA_DATA1
	lda VERA_DATA1
	lda VERA_DATA1
	lda VERA_DATA1
	stz VERA_DATA0
.endrepeat
	dex
	bne @g
	lda VERA_DATA1
	lda VERA_DATA1
	lda #$F0
	sta VERA_DATA0
	stz VERA_FX_CTRL
	stz VERA_FX_MULT
	stz VERA_CTRL
	plp
	rts

; VERA FX's cache writes, looked at once: DCSEL 63 reads "V", major,
; minor, patch; 0.3.1 or later (Veranda's FxCheck, kernel/s/init.s)
fxcheck:
	lda fx
	bne @r
	ldx #1
	lda #63 << 1
	sta VERA_CTRL
	lda VERA_VERSION
	cmp #'V'
	bne @no
	lda VERA_VERSION+1
	bne @yes
	lda VERA_VERSION+2
	cmp #3
	bcc @no
	bne @yes
	lda VERA_VERSION+3
	beq @no
@yes:	ldx #$80
@no:	stz VERA_CTRL
	stx fx
@r:	rts
