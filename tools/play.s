; Veranda Write's gate: the player of its scripted sessions (the test
; build: -DCW_TEST; src/v/test.c reads the script, CW.PLY, into a RAM bank
; of its own). Veranda's tests/edit_play.s (Veranda Edit's gate, MIT,
; Copyright Veranda's authors), taken over with two records more for the
; menus and Veranda Write's names.
;
; Veranda calls mouseSource from its interrupt every frame: this gives
; it the script's pointer (absolute, carry set) and puts the script's
; keys into the ROM's key queue, as a mouse and a keyboard would. Paced
; by the program, not the clock: a frame counts only when the program is
; idle (its main loop found nothing to do: cw_idle, which its appMain and
; the file chooser's hook set; or appMain is 0, a kernel dialog box's
; loop), and each record waits for both key queues to be empty, then GAP
; idle frames.
;
;   'P' xl xh yl yh   the pointer to (x, y)
;   'C' n             the pointer to the menu bar's item n (barx[n] + 6, 7)
;   'J' m             the pointer to item m of the list open under it (24 + 16 m)
;   'D' / 'U'         the button down / up ('G': the right one down)
;   'K' k             a key (the ROM's ISO code) into its queue
;   'T' n k1 .. kn    n keys (up to 8) at once
;   'M' m             the ROM's modifier keys held from now (shflag: 1
;                     Shift, 2 Alt, 4 Ctrl), as the keyboard driver keeps them
;   'W' n             n idle frames more
;   'H' n             the wheel: n notches (signed)
;   'L' n             test_log = n: the program logs its state
;   'Z'               the clock: start (8 MHz cycles)
;   'Y' c             "TIME c <cycles, hex>": from 'Z' to the last frame
;                     the program was busy since
;   'R' k n f         key k n times, one every f frames whatever the
;                     program does (a key held: the keyboard's repeat)
;   'A' k1 k2 f       key k1 now, then k2 f frames on whatever the program
;                     does (a long command started and stopped: Esc while
;                     a PDF is written, Veranda Write's W47)
;   'B' m n           the blink probe (W42): n changes of the cursor
;                     (cur_on, as play_slow is given it), each logged
;                     "BLINK m s frames" (s the new state, frames since the
;                     probe began, hex) as this routine sees them, a frame
;                     at a time; m: the main loop slowed meanwhile, about
;                     m * 1,300 cycles a pass (play_slow(cur_on), which the
;                     program calls in its idle pass after view_blink; 0:
;                     as fast as it runs); no record meanwhile
;   'Q' k             key k into the ROM's queue, timed: the clock kept
;                     (play_t0, play_st set) unless a key put before is
;                     still waiting to be drawn; src/v/test.c logs "CW LAT
;                     keys cycles" when the program has drawn it (keys: the
;                     ones it took since its last drawing). 'R''s keys are
;                     timed the same way
;   'V' dl dh n f     the pointer moved by d (signed) every f frames, n
;                     times, whatever the program does (a drag: the
;                     scroll bar's thumb, W73); no record meanwhile
;   'h' n             the wheel, timed as 'Q' times a key (the scroll
;                     drawn: "CW LAT")
;   'S' / 'E'         the end: VRAM dumped, the emulator stops
;
; The main loop's longest pause (the busy pointer's measure: Veranda shows
; it after 20 frames without a pass of the main loop): _play_lf counts the
; frames since the program's appMain last ran (it clears it), _play_lfm
; the most since 'Z', 'Q', 'R' or 'h'; 'Y' and "CW LAT" log it
;
; A program busy for HUNG frames on end is hung: "HUNG", the dump, the end.
;
; MIT licence (see LICENSE).

.include "veranda.inc"
.include "x16hw.inc"

.export _play_bank, _play_go, _play_slow, _play_t0, _play_st, _play_lf, _play_lfm
.import _cw_idle, _test_log, _barx, _bankt

GAP     = 6
HUNG    = 30000                 ; frames (500 s: a 4 MB document opens in about 5 minutes)
SHFLAG  = $A80C                 ; the ROM's modifiers, RAM bank 0 (tools/gate.sh checks it)
KBDBUF_PEEK = $FEBD

.segment "DATA"

_play_bank: .byte 0
cur:	.word $A000             ; (the session: in a bank of its own, 1.6)
wait:	.byte 60
px:	.word 320
py:	.word 240
pb:	.byte 0
busy:	.word 0
t0:	.res 4
tl:	.res 4
rec:	.res 10
ended:	.byte 0
moved:	.byte 1
rk:	.byte 0
gd:	.word 0                 ; 'V': the pointer's step,
gn:	.byte 0                 ;   steps left,
gf:	.byte 0                 ;   frames between,
gc:	.byte 0                 ;   frames to the next
rn:	.byte 0
rf:	.byte 0
rc:	.byte 0
bn:	.byte 0                 ; the probe: changes still to log (0: off)
bm:	.byte 0                 ; its slowing
bslow:	.byte 0                 ; (play_slow's: bm while the probe runs)
bcur:	.byte 0                 ; cur_on as play_slow was given it
bs:	.byte 0                 ; cur_on as last seen
bf:	.word 0                 ; frames since it began
_play_st: .byte 0               ; 'Q': a timed key waits to be drawn (test.c clears it)
_play_t0: .res 4                ; the clock when it was put
_play_lf: .byte 0               ; frames since appMain last ran (it clears it)
_play_lfm: .byte 0              ; the most since the clock was last started

.segment "CODE"

; (W56) the player's code is in the test overlay's bank (1.6; overlay
; 16's until then: the test build's main RAM had no room for it, and
; overlay 16 wanted its room for the on-demand overlays' loader): the
; interrupt calls this, which maps it, and
; maps back the bank that was (the kernel's, an overlay's); the record's
; bytes (in the session's bank) and the ROM's modifiers (RAM bank 0) are
; read and written here, in main RAM
_play_go:
	lda #<Tramp
	sta mouseSource
	lda #>Tramp
	sta mouseSource+1
	rts

Tramp:	lda $00
	pha
	lda _bankt
	sta $00
	jsr Player              ; (a: the buttons; c set)
	tay
	pla
	sta $00
	tya
	sec
	rts

; the record at cur into rec, from the session's bank
rdrec:	lda $00
	pha
	lda _play_bank
	sta $00
	lda cur
	sta @a+1
	lda cur+1
	sta @a+2
	ldx #0
@a:	lda $FFFF,x
	sta rec,x
	inx
	cpx #10
	bne @a
	pla
	sta $00
	rts

; 'M': the ROM's modifiers (RAM bank 0)
setmods:
	ldx $00
	stz $00
	lda rec+1
	sta SHFLAG
	stx $00
	rts


; the program's idle pass, a = cur_on: the main loop slowed while the
; probe runs, about bslow * 1,300 cycles (256 x 5 a step)
_play_slow:
	sta bcur
	ldy bslow
	beq @r
@o:	ldx #0
@i:	dex
	bne @i
	dey
	bne @o
@r:	rts

.segment "OVERLAY30"

Player:	lda _play_lf            ; (the main loop's pauses)
	inc a
	beq @lf
	sta _play_lf
	cmp _play_lfm
	bcc @lf
	sta _play_lfm
@lf:	jsr rep
	jsr glide
	lda bn
	beq @np
	jsr probe
	bra @to0
@np:	jsr step
@to0:
	lda moved               ; a new place: there; else where the pointer is,
	bne @to                 ; which Veranda may have moved itself (a menu
	lda mouseXPos           ; opened puts it on its first item)
	sta px
	lda mouseXPos+1
	sta px+1
	lda mouseYPosW
	sta py
	lda mouseYPosW+1
	sta py+1
@to:	stz moved
	lda px
	sta r0L
	lda px+1
	sta r0H
	lda py
	sta r1L
	lda py+1
	sta r1H
	lda pb
	sec
	rts

; the blink probe, every frame while bn > 0
probe:	inc bf
	bne @1
	inc bf+1
@1:	lda bcur
	cmp bs
	beq @r
	sta bs
	ldx #0
@t:	lda blink,x
	beq @m
	sta EMU_STDOUT
	inx
	bra @t
@m:	lda bm
	jsr hex2
	lda #' '
	sta EMU_STDOUT
	lda bs
	jsr hex2
	lda #' '
	sta EMU_STDOUT
	lda bf+1
	jsr hex2
	lda bf
	jsr hex2
	lda #10
	sta EMU_STDOUT
	dec bn
	bne @r
	stz bslow
	lda #GAP
	sta wait
@r:	rts
opB:	lda rec+1
	sta bm
	sta bslow
	lda bcur
	sta bs
	stz bf
	stz bf+1
	lda rec+2
	sta bn
	rts


; the clock now into tl
mark:	lda EMU_CLOCK
	sta tl
	lda EMU_CLOCK+1
	sta tl+1
	lda EMU_CLOCK+2
	sta tl+2
	lda EMU_CLOCK+3
	sta tl+3
	rts

; 'R': the held key's next repeat, every rf frames
rep:	lda rn
	beq @r
	dec rc
	bne @r
	lda rf
	sta rc
	dec rn
	jsr stamp
	lda rk
	jmp k_kbdbuf_put
@r:	rts

; 'V': the pointer on by gd every gf frames
glide:	lda gn
	beq @r
	dec gc
	bne @r
	lda gf
	sta gc
	dec gn
	clc
	lda py
	adc gd
	sta py
	lda py+1
	adc gd+1
	sta py+1
	lda #1
	sta moved
@r:	rts
opV:	lda rec+1
	sta gd
	lda rec+2
	sta gd+1
	lda rec+4
	sta gf
	lda #1
	sta gc
	lda rec+3
	sta gn
	rts

; a timed key: the clock, unless one put before is not drawn yet
stamp:	lda _play_st
	bne @r
	stz _play_lfm
	lda EMU_CLOCK
	sta _play_t0
	lda EMU_CLOCK+1
	sta _play_t0+1
	lda EMU_CLOCK+2
	sta _play_t0+2
	lda EMU_CLOCK+3
	sta _play_t0+3
	lda #1
	sta _play_st
@r:	rts
opQ:	jsr stamp
	lda rec+1
	jmp k_kbdbuf_put
oph:	jsr stamp
	jmp opH

step:	lda ended
	bne @r
	lda rn                  ; (a held key: the next record after it)
	ora gn                  ; (a glide too)
	bne @r
	lda appMain
	ora appMain+1
	beq @idle
	lda _cw_idle
	beq @busy
	stz _cw_idle
@idle:	stz busy
	stz busy+1
	lda wait
	beq @q
	dec wait
@r:	rts
@busy:	jsr mark
	inc busy
	bne @r
	inc busy+1
	lda busy+1
	cmp #>HUNG
	bcc @r
	ldx #0
@h:	lda hung,x
	beq @hd
	sta EMU_STDOUT
	inx
	bra @h
@hd:	lda appMain+1           ; (and where appMain is)
	jsr hex2
	lda appMain
	jsr hex2
	jmp stop
@q:	lda pressFlag           ; a key in Veranda's queue
	bmi @r
	jsr KBDBUF_PEEK         ; x: keys in the ROM's
	cpx #0
	bne @r
	jsr rdrec               ; the record at cur, from the script's bank
	lda #GAP
	sta wait
	lda rec
	ldx #0
@d:	cmp ops,x
	beq @go
	inx
	inx
	inx
	inx
	ldy ops,x
	bne @d
	jmp stop                ; not an op: the end
@go:	lda ops+1,x             ; its length
	clc
	adc cur
	sta cur
	bcc @j
	inc cur+1
@j:	lda ops+3,x
	pha
	lda ops+2,x
	pha
	rts                     ; (the address - 1)

ops:	.byte 'P', 5
	.addr opP - 1
	.byte 'C', 2
	.addr opC - 1
	.byte 'J', 2
	.addr opJ - 1
	.byte 'D', 1
	.addr opD - 1
	.byte 'U', 1
	.addr opU - 1
	.byte 'G', 1
	.addr opG - 1
	.byte 'K', 2
	.addr opK - 1
	.byte 'T', 0
	.addr opT - 1
	.byte 'M', 2
	.addr opM - 1
	.byte 'W', 2
	.addr opW - 1
	.byte 'H', 2
	.addr opH - 1
	.byte 'L', 2
	.addr opL - 1
	.byte 'Z', 1
	.addr opZ - 1
	.byte 'Y', 2
	.addr opY - 1
	.byte 'R', 4
	.addr opR - 1
	.byte 'A', 4
	.addr opA - 1
	.byte 'B', 3
	.addr opB - 1
	.byte 'Q', 2
	.addr opQ - 1
	.byte 'h', 2
	.addr oph - 1
	.byte 'V', 5
	.addr opV - 1
	.byte 'S', 1
	.addr stop - 1
	.byte 'E', 1
	.addr stop - 1
	.byte 0

opP:	lda #1
	sta moved
	lda rec+1
	sta px
	lda rec+2
	sta px+1
	lda rec+3
	sta py
	lda rec+4
	sta py+1
	rts
opC:	lda rec+1               ; barx[n] + 6, 7
	asl
	tax
	clc
	lda _barx,x
	adc #6
	sta px
	lda _barx+1,x
	adc #0
	sta px+1
	lda #7
	sta py
	stz py+1
	lda #1
	sta moved
	rts
opJ:	lda rec+1               ; 24 + 16 m: the list's items are 16 lines
	asl
	asl
	asl
	asl
	clc
	adc #24
	sta py
	lda #0
	adc #0
	sta py+1
	clc
	lda px
	adc #4
	sta px
	bcc @j
	inc px+1
@j:	lda #1
	sta moved
	rts
opR:	lda rec+1
	sta rk
	lda rec+3
	sta rf
	lda #1
	sta rc
	lda rec+2
	sta rn
	rts
opA:	lda rec+2              ; k2 in f frames ('R''s, once)
	sta rk
	lda rec+3
	sta rc
	lda #1
	sta rn
	lda rec+1
	jmp k_kbdbuf_put
opD:	lda #1
	sta pb
	rts
opU:	stz pb
	rts
opG:	lda #2                  ; (W58) the right button down ('U' up)
	sta pb
	rts
opK:	lda rec+1
	jmp k_kbdbuf_put
opT:	lda rec+1               ; n keys: the record is 2 + n bytes
	clc
	adc #2
	clc
	adc cur
	sta cur
	bcc @1
	inc cur+1
@1:	ldx #0
@k:	cpx rec+1
	beq @r
	phx
	lda rec+2,x
	jsr k_kbdbuf_put
	plx
	inx
	bra @k
@r:	rts
opM:	jmp setmods
opW:	lda rec+1
	sta wait
	rts
opH:	lda rec+1
	clc
	adc mouseWheel
	sta mouseWheel
	rts
opL:	lda rec+1
	sta _test_log
	rts
opZ:	stz _play_lfm
	jsr mark
	ldx #3
@z:	lda tl,x
	sta t0,x
	dex
	bpl @z
	rts
opY:	ldx #0
@t:	lda time,x
	beq @c
	sta EMU_STDOUT
	inx
	bra @t
@c:	lda rec+1
	sta EMU_STDOUT
	lda #' '
	sta EMU_STDOUT
	sec
	lda tl
	sbc t0
	sta t0
	lda tl+1
	sbc t0+1
	sta t0+1
	lda tl+2
	sbc t0+2
	sta t0+2
	lda tl+3
	sbc t0+3
	sta t0+3
	ldx #3
@h:	lda t0,x
	lsr
	lsr
	lsr
	lsr
	jsr hex
	lda t0,x
	and #15
	jsr hex
	dex
	bpl @h
	lda #' '
	sta EMU_STDOUT
	lda _play_lfm
	jsr hex2
	lda #10
	sta EMU_STDOUT
	rts
hex2:	pha
	lsr
	lsr
	lsr
	lsr
	jsr hex
	pla
	and #15
hex:	cmp #10
	bcc @1
	adc #6
@1:	adc #'0'
	sta EMU_STDOUT
	rts

stop:	lda #1
	sta ended
	sta EMU_SAVE_ON_EXIT
	jmp $FFFF

.segment "RODATA"
hung:	.byte 10, "HUNG", 10, 0
time:	.byte 10, "TIME ", 0
blink:	.byte 10, "BLINK ", 0
