; Veranda Write on Veranda: each record overlay's variables (its BSS),
; for v/ovlclr.c's ovl_clear (W60): the loader reads a record into its
; bank and leaves the BSS after it as the bank held it (Veranda's V37),
; which is the program's own earlier values when it is started again in
; the same session (and anything on a machine just switched on). A row
; an overlay (the records: 1-16, then 27-29, the tables', and the test build's 30;
; spelling's and the office files' are cleared when v/spload.c loads them): the address of its bank's
; variable, its BSS's address and size (0: none). tools/spovl.py fills
; it in after the link, from labels.txt (as it does sp_want).
;
; Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

.export _ovl_bss

.segment "OVL9RO"
_ovl_bss:
	.res 20 * 6, 0
