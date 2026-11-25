; Veranda Write on Veranda: spelling's part in main RAM (W56-W59): what
; the overlays share (the word looked up, the flags), the block scan and
; the key comparison (they map a cache bank at $A000, where the overlays
; run), the wrappers of the spelling overlays' entry points (glue.s's far).
;
; sp_bscan1 and sp_kcmp1 are Writer's fast_scan and fast_keycmp (the
; user's own earlier project, ~/Projects/Writer src/core/fast_x16.s),
; taken over: the word at sp_w (main RAM, so read with the bank mapped),
; the block or key at the address in A/X of bank sp_bk.
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.include "veranda.inc"
.importzp ptr1, ptr3, tmp1, tmp2, tmp3
.import far, _bank23, _bank24

.export _sp_lw, _sp_wl, _sp_word, _sp_live, _sp_pend, _sp_kj, _sp_ws, _sp_we, _sp_buf, _sp_bk
.export _sp_from, _sp_stop, _sp_wrap, _spp_on, _sp_xb, _sp_tflags, _sp_ulen, _sp_ilen, _sp_ncb, _sp_busy, _sp_nofile, _sp_cfg
.export _sp_bscan1, _sp_kcmp1, _sp_getin

.macro FAR name, target, bank
.export name
.import target
name:	jsr far
	.addr target
	.addr bank
.endmacro

.segment "BSS"
_sp_lw:     .res 32              ; the word looked up (lower case)
_sp_wl:    .res 1
_sp_word:  .res 32              ; as written
_sp_live:  .res 1               ; marking as you type: on
_sp_pend:  .res 1               ; rows on the screen to check
_sp_kj:    .res 1               ; cw_jiffies at the last key
_sp_ws:    .res 4               ; the word found: its place
_sp_we:    .res 4
_sp_buf:   .res 24              ; (bk_rd's and bk_wr's: main RAM)
_sp_bk:    .res 1               ; sp_bscan1's and sp_kcmp1's bank
_sp_from:  .res 4               ; Check spelling: from, to (the cursor at its start), wrapped
_sp_stop:  .res 4
_sp_wrap:  .res 1
_spp_on:   .res 1               ; the spelling panel is open
_sp_xb:    .res 1               ; the lists' bank (spell.c: the suggestions there)
_sp_tflags: .res 1              ; (the gate's: spelling as Writer did it)
_sp_ulen:  .res 2               ; the user's and the ignored words' bytes (in the lists' bank)
_sp_ilen:  .res 2
_sp_ncb:   .res 1               ; the cache's banks (spell.c's)
_sp_nofile: .res 1              ; SPELL.OVL not there (v/spload.c)
_sp_cfg:   .res 1               ; (W60) the settings, WRITE.CFG's (src/spcfg.c)
_sp_busy:  .res 1               ; a lookup is reading the cache (fonts.c: not given back now)

.segment "CODE"

; unsigned char __fastcall__ sp_bscan1(unsigned int a): 1 found, 0 not
; there (it sorts inside the block), 2 it sorts after it. Entry: shared
; prefix length p, suffix length n, n suffix bytes; m is how much of the
; word the entry before matched: p > m sorts before the word (passed), p
; < m after it (stop), p == m compare the suffix. ptr3 = entry + 2 - m,
; so suffix byte k and word byte m + k share one index Y.
_sp_bscan1:
	sta ptr1
	stx ptr1+1
	lda $00
	pha
	lda _sp_bk
	sta $00
	jsr scan
	tax
	pla
	sta $00
	txa
	ldx #0
	rts

scan:	lda _sp_wl
	sta tmp2                ; len
	stz tmp1                ; m = 0
entry:	lda (ptr1)              ; p
	cmp #$FF
	beq after               ; the block's end
	cmp tmp1
	bcc no                  ; p < m: passed it
	bne next                ; p > m: before it
	ldy #1
	lda (ptr1),y            ; n
	clc
	adc tmp1
	sta tmp3                ; Y at the suffix's end: m + n
	lda ptr1
	clc
	adc #2
	sta ptr3
	lda ptr1+1
	adc #0
	sta ptr3+1
	lda ptr3
	sec
	sbc tmp1
	sta ptr3
	bcs :+
	dec ptr3+1
:	ldy tmp1
cmpl:	cpy tmp3
	beq sufend              ; the whole suffix matched
	cpy tmp2
	beq no                  ; the word ended first: entry > word
	lda (ptr3),y
	cmp _sp_lw,y
	bne diff
	iny
	bra cmpl
sufend:	cpy tmp2
	beq found
	sty tmp1                ; the entry is a prefix of the word
	bra next
diff:	bcs no                  ; entry > word
	sty tmp1
next:	ldy #1
	lda (ptr1),y
	sec                     ; + 2: n <= 31, no carry out
	adc #1
	adc ptr1
	sta ptr1
	bcc entry
	inc ptr1+1
	bra entry
found:	lda #1
	rts
no:	lda #0
	rts
after:	lda #2
	rts

; signed char __fastcall__ sp_kcmp1(unsigned int a): the 16-byte key
; (NUL-padded) against sp_w cut to 16: $FF key < word, 0 equal, 1 key > word
_sp_kcmp1:
	sta ptr1
	stx ptr1+1
	lda $00
	pha
	lda _sp_bk
	sta $00
	ldy #0
@l:	lda #0
	cpy _sp_wl
	bcs :+                  ; past the word: compared with NUL
	lda _sp_lw,y
:	cmp (ptr1),y
	bne @d
	tax
	beq @eq                 ; both NUL
	iny
	cpy #16
	bne @l
@eq:	lda #0
	bra @r
@d:	lda #1                  ; word byte < key byte: key > word
	bcc @r
	lda #$FF
@r:	tax
	pla
	sta $00
	txa
	ldx #0
	cmp #$80                ; (sign-extended for signed char)
	bcc :+
	dex
:	rts

.segment "OVERLAY26"
; a key from the ROM's queue (GETIN: 0 none): the pop-up's loop, which
; the main loop's KeyPoll does not feed (v/sppop.c, overlay 26: the ROM
; puts the bank back)
_sp_getin:
	jsr $FFE4
	ldx #0
	rts

.segment "CODE"

; overlay 23: the dictionary, the hash, the lists, the marks, live
; checking, Check spelling's search (spell.c, v/splive.c, v/spio.c)
FAR _sp_ready, _e_sp_ready, _bank23
FAR _sp_check, _e_sp_check, _bank23
FAR _sp_look, _e_sp_look, _bank23
FAR _sp_ignore, _e_sp_ignore, _bank23
FAR _sp_adduser, _e_sp_adduser, _bank23
FAR _sp_drop, _e_sp_drop, _bank23
FAR _sp_end, _e_sp_end, _bank23
FAR _sp_aside, _e_sp_aside, _bank23
FAR _sp_tokat, _e_sp_tokat, _bank23
FAR _sp_tok, _e_sp_tok, _bank23
FAR _mk_pre, _e_mk_pre, _bank23
FAR _mk_dots, _e_mk_dots, _bank23
FAR _sp_idle, _e_sp_idle, _bank23
FAR _sp_next, _e_sp_next, _bank23
; overlay 24: the suggestions (sugg.c)
FAR _sp_suggest, _e_sp_suggest, _bank24
; overlay 24 too: the files and banks (v/spio.c)
FAR _sp_fopen, _e_sp_fopen, _bank24
FAR _sp_fclose, _e_sp_fclose, _bank24
FAR _sp_fseek, _e_sp_fseek, _bank24
FAR _sp_fread, _e_sp_fread, _bank24
FAR _sp_uload, _e_sp_uload, _bank24
FAR _sp_usave, _e_sp_usave, _bank24
FAR _sp_bank, _e_sp_bank, _bank24
FAR _sp_bfree, _e_sp_bfree, _bank24
FAR _sp_nbanks, _e_sp_nbanks, _bank24
FAR _sp_deflang, _e_sp_deflang, _bank24
