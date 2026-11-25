# Veranda Write: a word processor for the Commander X16 under Veranda.
#
#   make test        the document model's unit tests on the host (gcc)
#   make veranda     build/veranda/Veranda Write.cvt, against Veranda's
#                    installed SDK (SDK=..., default build/sdk: see
#                    tools/sdk.sh, which builds one from ~/Projects/geos)
#   make veranda-test  build/veranda/test/: the gate's build (the session
#                    player linked in: tools/gate.sh)
#   make dict-release  build/dict-release: the dictionaries' GitHub release
#                    (W61: tools/mkdict.py; it prints the gh commands)
#   make clean
#   VFLAGS=...       more cl65 flags (-DCW_PROF: the profiling marks, W43)
#
# Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).

CC      ?= gcc
BUILD   := build
HOSTDIR := $(BUILD)/host

# the model: portable C, the same files on the host and the X16
CORE    := src/bank.c src/cbuf.c src/cbufx.c src/swap.c src/swapwh.c src/text.c src/edit.c src/undo.c src/layout.c src/layfix.c src/format.c src/find.c src/styles.c src/gwimport.c src/page.c src/pdf.c src/pdfhf.c
# stage 4 (W49-W54): .docx and .odt opened and exported
OFFICE  := src/ozip.c src/oxml.c src/oemit.c src/odocx.c src/oodt.c src/oexp.c src/oparts.c
# spelling (W56-W59): the dictionary's lookups, the suggestions
SPELL   := src/spell.c src/sugg.c src/spcfg.c
HEADERS := src/spell.h src/cw.h src/laypriv.h src/cbpriv.h src/pdfw.h src/office.h src/oimp.h src/oxnames.h src/ohelp.c src/oparts.h

HOSTFLAGS := -std=c99 -Wall -Wextra -Wno-unused-parameter -g -O1 -DCW_HOST

.PHONY: all test host clean veranda veranda-test dict-release
all: test

$(HOSTDIR)/test_cw: $(CORE) $(OFFICE) $(SPELL) src/hf.c src/v/pages.c tests/test_main.c tests/test_office.c tests/test_spell.c tests/host.c tests/host.h $(HEADERS) dict/PL.DIC dict/EN.DIC
	@mkdir -p $(HOSTDIR)
	$(CC) $(HOSTFLAGS) -o $@ $(CORE) $(OFFICE) $(SPELL) src/hf.c src/v/pages.c tests/test_main.c tests/host.c

# the names the readers know, hashed (tools/oxnames.py); the exporter's
# XML text, its templates compressed (tools/oparts.py: a minute)
src/oxnames.h: tools/oxnames.py
	python3 -I -B tools/oxnames.py $@
src/oparts.h: tools/oparts.py
	python3 -I -B tools/oparts.py $@

host: $(HOSTDIR)/test_cw
test: $(HOSTDIR)/test_cw
	$(HOSTDIR)/test_cw
	@python3 -I -B tools/check_pdf.py $(HOSTDIR)
	@python3 -I -B tools/office_fix.py $(HOSTDIR)/office
	@python3 -I -B tools/check_office.py $(HOSTDIR)/test_cw $(HOSTDIR)/office
	@python3 -I -B tools/check_docx.py --test-cw $(HOSTDIR)/test_cw $(HOSTDIR)/rt.docx $(HOSTDIR)/rt.odt \
	    $(HOSTDIR)/empty.docx $(HOSTDIR)/empty.odt

# ---- Veranda -------------------------------------------------------
# The X16 build against Veranda's installed SDK, as the package builds the
# programs of other trees (Veranda's tools/mkdist.sh: make veranda SDK=...)
SDK     ?= $(BUILD)/sdk
CC65_HOME ?= $(HOME)/.local/share/cc65
export CC65_HOME
CL65    ?= $(shell command -v cl65 2>/dev/null || echo $(HOME)/.local/bin/cl65)
VDIR    := $(BUILD)/veranda
VNAME   := Veranda Write
VCC     = $(CL65) -t none --cpu 65C02 -O -I $(SDK)/include --asm-include-dir $(SDK)/include -I src -I src/v $(VFLAGS)
# main RAM: the model, the fonts, the view, the start, the tables
V_MAIN  := $(CORE) $(SPELL) src/v/main.c src/v/fonts.c src/v/view.c src/v/menus.c
V_HDRS  := $(HEADERS) src/v/ovl23.h src/v/ovl24.h src/v/spdraw.h src/v/cwv.h src/v/view.h src/v/ovl4.h src/v/ovl6.h src/v/ovl2s.h src/v/ovl11.h src/v/ovl12.h

# $(1): the build's folder, $(2): extra flags
define vr_objs
$(1)/%.o: src/%.c $(V_HDRS) Makefile
	@mkdir -p $$(dir $$@)
	$$(VCC) $(2) -c -o $$@ $$<
$(1)/v/%.s.o: src/v/%.s Makefile
	@mkdir -p $$(dir $$@)
	$$(VCC) -c -o $$@ $$<
$(1)/sdk/scrapio.o $(1)/sdk/scraptbeg.o: $(1)/sdk/%.o: $(SDK)/src/%.c $(SDK)/include/scrap.h Makefile
	@mkdir -p $$(dir $$@)
	$$(VCC) --code-name OVERLAY7 --rodata-name OVL7RO -DSCRAP_PATH=scrap_path -c -o $$@ $$<
$(1)/hf14.o: src/hf.c $(V_HDRS) Makefile
	@mkdir -p $$(dir $$@)
	$$(VCC) $(2) --code-name OVERLAY14 --rodata-name OVL14RO --bss-name OVL14BSS --data-name OVL14DATA -c -o $$@ $$<
$(1)/v/ohash19.o $(1)/v/ohash20.o: $(1)/v/ohash%.o: src/v/ohash.s Makefile
	@mkdir -p $$(dir $$@)
	$$(VCC) --asm-define OVL=$$* -c -o $$@ $$<
$(1)/sdk/vrcs.o: $(SDK)/src/vrcs.s Makefile
	@mkdir -p $$(dir $$@)
	$$(VCC) --asm-define VRCS_OVL=10 -c -o $$@ $$<
$(1)/sdk/vrchoose.o: $(SDK)/src/vrchoose.c $(SDK)/include/vrchoose.h Makefile
	@mkdir -p $$(dir $$@)
	$$(VCC) --code-name OVERLAY3 --rodata-name OVL3RO -c -o $$@ $$<
$(1)/sdk/vrwin.o: $(SDK)/src/vrwin.c $(SDK)/include/vrwin.h Makefile
	@mkdir -p $$(dir $$@)
	$$(VCC) --code-name OVERLAY7 -Dvr_win=o_vr_win -Dvr_win_title=o_vr_win_title \
	    -Dvr_win_status=o_vr_win_status -Dvr_win_hit=o_vr_win_hit -Dvr_win_top=o_vr_win_top \
	    -Dvr_win_bottom=o_vr_win_bottom -c -o $$@ $$<
endef
$(eval $(call vr_objs,$(VDIR),))
$(eval $(call vr_objs,$(VDIR)/test,-DCW_TEST))

V_OBJ   = $(patsubst src/%.c,$(1)/%.o,$(V_MAIN) $(OFFICE) src/v/oxnote.c src/v/ui.c src/v/files.c src/v/clip.c src/v/tools.c src/v/findui.c src/v/fcache.c src/v/view2.c src/v/pages.c src/v/swapio.c src/v/latinui.c src/v/popup.c src/v/tbar.c src/v/sbar.c src/v/ruler.c src/v/status.c src/v/pgview.c src/v/pgpanel.c src/v/pdfx.c src/v/splive.c src/v/spio.c src/v/spui.c src/v/sppop.c src/v/spopt.c src/v/spload.c src/v/ovlclr.c) $(1)/hf14.o \
          $(1)/v/owin.s.o $(1)/v/ocrc.s.o $(1)/v/oparts.s.o $(1)/v/ohash19.o $(1)/v/ohash20.o $(1)/v/wrscan.s.o $(1)/v/pdfnum.s.o $(1)/v/ltab.s.o $(1)/v/latin.s.o $(1)/v/cbread.s.o $(1)/v/ldfilt.s.o $(1)/v/glue.s.o $(1)/v/fscan.s.o $(1)/v/render.s.o $(1)/v/spell.s.o $(1)/v/sphash.s.o $(1)/v/ovlbss.s.o $(1)/sdk/scrapio.o $(1)/sdk/scraptbeg.o $(1)/sdk/vrchoose.o $(1)/sdk/vrwin.o $(1)/sdk/vrcs.o

# link $(1) into $(2)/cw.bin (and .1 .. .n: the overlays), pack
# "$(2)/Veranda Write.cvt" with the overlays $(3) as its records and
# $(2)/SPELL.OVL with spelling's, $(SP_OVLS) (tools/spovl.py: W59), show
# the sizes
define vr_link
	@mkdir -p $(2)
	$(CL65) -t none -C $(BUILD)/app-ovl.cfg -Wl -D,__STACKSIZE__=0x400 -m $(2)/cw.map -Ln $(2)/labels.txt \
	    -o $(2)/cw.bin $(1) $(SDK)/lib/veranda.lib
	python3 -I -B tools/spovl.py $(2) $(SP_OVLS)
	python3 -I -B $(SDK)/tools/vpack.py --load 0x0801 -o "$(2)/$(VNAME).cvt" cfg/app.hdr \
	    $(2)/cw.bin $(foreach n,$(3),$(2)/cw.bin.$(n))
	@python3 -I -B tools/sizes.py $(2)/cw.map $(2)/cw.bin $(SP_OVLS) $(3)
endef

# the overlays (the test build's last: TOVL, src/v/cwv.h)
V_OVLS  := 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22
SP_OVLS := 23 24 25 26
V_TOVL  := 27
# the SDK's link configuration with overlays 25 to 27 (tools/ovlcfg.py)
$(BUILD)/app-ovl.cfg: $(SDK)/cfg/app-c-ovl.cfg tools/ovlcfg.py Makefile
	@mkdir -p $(BUILD)
	python3 -I -B tools/ovlcfg.py $< $(V_TOVL) $@
# spelling's overlays (W59), for the package: Veranda's tools/mkdist.sh
# copies build/veranda/DICT/SPELL.OVL to /VERANDA/DICT. The dictionaries
# are not in the package (W61): their own release (make dict-release)
$(VDIR)/DICT/SPELL.OVL: $(VDIR)/packed
	@mkdir -p $(dir $@)
	rm -f $(VDIR)/DICT/*.DIC $(VDIR)/DICT/*.DIC.txt
	cp $(VDIR)/SPELL.OVL $@
veranda: $(VDIR)/packed $(VDIR)/DICT/SPELL.OVL

# (W61) the dictionaries' GitHub release (dict-1 of veranda-x16/veranda-write)
# made from dict/: build/dict-release (tools/mkdict.py; it prints the gh
# commands, which only you run)
dict-release:
	python3 -I -B tools/mkdict.py dict $(BUILD)/dict-release
$(VDIR)/packed: $(call V_OBJ,$(VDIR)) cfg/app.hdr cfg/icon.pbm cfg/tile.txt $(BUILD)/app-ovl.cfg
	$(call vr_link,$(call V_OBJ,$(VDIR)),$(VDIR),$(V_OVLS))
	@touch $@

V_TOBJ  = $(call V_OBJ,$(VDIR)/test) $(VDIR)/test/v/test.o $(VDIR)/test/play.o
$(VDIR)/test/play.o: tools/play.s
	@mkdir -p $(dir $@)
	$(VCC) -c -o $@ $<
veranda-test: $(VDIR)/test/packed
$(VDIR)/test/packed: $(V_TOBJ) cfg/app.hdr cfg/icon.pbm cfg/tile.txt $(BUILD)/app-ovl.cfg
	$(call vr_link,$(V_TOBJ),$(VDIR)/test,$(V_OVLS) $(V_TOVL))
	@touch $@

clean:
	rm -rf $(HOSTDIR) $(BUILD)/veranda
