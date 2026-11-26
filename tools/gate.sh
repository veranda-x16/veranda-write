#!/bin/bash
# Veranda Write's gate: stage 4 (stage 3's, and .docx and .odt).
#
#   tools/gate.sh [SDK=<Veranda's installed SDK>] [SD=0: no SD card run]
#
# 1. Veranda's SDK (tools/sdk.sh, from ~/Projects/geos, untouched) unless
#    SDK= names one; the host unit tests (make test)
# 2. make veranda and make veranda-test against it: an application (VRND,
#    640x480, API 6, documents CWR), main RAM below the C stack, each
#    overlay under 8 KB; no call into an overlay that may not be mapped
#    (tools/check_ovl.py)
# 3. the session (tools/script.py, tools/play.s) in x16emu with Veranda
#    booted, on a host folder (512 KB of RAM banks): typing in styles, the
#    menus, undo and redo, copy and paste, the pointer, Save as, Alt+Tab
#    away and back (a test desktop, tools/desk), New, Open; paragraph
#    styles (headings, lists, a quotation, centred and justified text,
#    line spacing; undone, redone, saved); Find, Replace, Replace all (one
#    undo step), match case, whole words; Word count; a 1.0 file opened;
#    a geoWrite document listed by its class in the Open dialog, converted
#    when asked (as tools/gwimport.py converts it), saved as a .cwr (the
#    original unchanged), opened again;
#    Polish (W38): "Zażółć gęślą jaźń" and "ZAŻÓŁĆ GĘŚLĄ JAŹŃ" typed (the
#    Veranda set's codes, as its Polish keyboard gives them), saved (CWRD
#    1.3, ISO-8859-16) and opened again, a 1.2 document's Latin-1
#    converted; the fonts given the Polish letters first (tools/hifont.py:
#    Veranda's extended records, V136), the screenshot's text compared
#    with the host's drawing of it (tools/check_polish.py);
#    (W41) a styled run at the end deleted with Backspace held (the
#    keyboard's repeat), Delete and Cut, the cursor inside the text after;
#    a big document opened, edited, searched, replaced in, saved, a burst
#    of keys; (W43) the per-key timings, tools/speed.sh;
#    (stage 3, W44-W48: SESSION=page, under R49 at 512 KB, on the host
#    folder and on an SD card image) page breaks typed, the header and
#    footer, the page setup, saved as 1.4 and opened again, exported as
#    PDF (Polish in it), 1.1-1.3 documents opened, a big document's pages
#    and its PDF (timed), an export stopped by Esc, Go to page against the
#    PDF's page (tools/check.py, tools/check_pdf.py: qpdf, pdfinfo,
#    pdftotext);
#    (stage 4, W49-W54: SESSION=office, under R49 at 512 KB, on the host
#    folder and on an SD card image) a Word document opened (converted:
#    its note), exported as Word and as OpenDocument, an OpenDocument
#    text opened, a big Word document opened and
#    exported both ways (timed: 600 KB of document.xml, 100 KB of text); each state against the host's conversion
#    of the same file (build/host/test_cw ox), each exported file read back
#    on the host (the same document, its page setup) and checked as other
#    programs read it (tools/check_docx.py: the ZIP, the XML, LibreOffice);
#    (W55: SESSION=redraw, under R49 at 512 KB) after every change of
#    the layout, the text area as the edits drew it against the text
#    wrapped again and drawn whole (src/v/test.c's redraw check), the
#    spelling marks included;
#    (W56-W59: SESSION=spell, under R49 at 512 KB, on the host folder and
#    an SD card image) spelling: live marks, Check spelling, the pop-up,
#    the user's words, both languages, the lookups timed; (W60) the
#    settings changed, the program quit and started again (the session's
#    second part), the settings kept, no marks and nothing loaded;
#    (W61: SESSION=nodict, R49 at 512 KB) spelling without the
#    dictionaries: quiet while typing, F7's panel, Tools > Get dictionaries (the
#    requests for Veranda's desktop and Veranda Update, put aside and
#    back), a dictionary that arrives while Write runs found;
#    (W71: SESSION=oxspeed, R49 at 512 KB, a host folder and an SD card
#    image) the user's kind of .docx (LibreOffice's, from HTML), the
#    stress fixtures (tools/oxstress.py) opened, timed, against the host;
#    (W68: SESSION=ai, R49 at 512 KB, ASKFAKE=1) AI help with a stand-in
#    for Veranda Ask (tools/askfake: nothing sent anywhere);
#    (W73: SESSION=scroll, R49 at 512 KB) scrolling by every way, timed,
#    each step against a full drawing;
#    tools/check.py: the states (characters, styles, paragraphs)
#    and the files against the script's model; the timings
# 4. screenshots: the session stopped at points (build/gate/*.png; the
#    styled page, the find panel, Word count also in build/shots)
# 5. the session on an SD card image under ROM R49
#    (~/Downloads/Release.R49.ROM.Image/rom.bin), with 2 MB of RAM banks and
#    a big document of 200,000 bytes (the host folder's run: 512 KB, the
#    machine's own, and 140,000), the files read back
#
# Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).
set -e
cd "$(dirname "$0")/.."
for a in "$@"; do export "$a"; done
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
export PATH=$CC65_HOME/bin:$HOME/.local/bin:$PATH
PY="python3 -I -B"
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
R49=${R49:-$HOME/Downloads/Release.R49.ROM.Image/rom.bin}
OUT=build/gate

ok() { echo "ok    $*"; }
# tools/check.py's report indented; its status the gate's (W41: under
# "| sed" a failure was printed and the gate went on)
chk() { local r=0; $PY tools/check.py "$1" > "$1/check.txt" || r=$?; sed 's/^/      /' "$1/check.txt"; return $r; }
fail() { echo "FAIL  $*"; exit 1; }

if [ -z "$SDK" ]; then
	[ -f build/sdk/bin/VERANDA.SYS ] && [ -f build/fonts/Gate\ Sans.cvt ] || tools/sdk.sh >/dev/null || fail "tools/sdk.sh"
	SDK=build/sdk
fi
export SDK
mkdir -p "$OUT"
for f in "Gate Sans" "Gate Mono"; do        # (W38: with $A0-$FF; Veranda's own glyphs kept when they are there)
	$PY tools/hifont.py "build/fonts/$f.cvt" "build/fonts/$f.cvt" || fail "tools/hifont.py: $f"
done
make -s test > "$OUT/unit.log" 2>&1 || { tail "$OUT/unit.log"; fail "make test"; }
ok "unit tests: $(grep 'checks, ' "$OUT/unit.log" | tail -1); $(grep -c '^pdf ' "$OUT/unit.log") PDFs (tools/check_pdf.py); $(grep -c '^office ' "$OUT/unit.log") .docx and .odt files (tools/check_office.py, tools/check_docx.py)"

make -s veranda SDK="$SDK" > "$OUT/build.log" 2>&1 || { tail "$OUT/build.log"; fail "make veranda"; }
make -s veranda-test SDK="$SDK" > "$OUT/build-test.log" 2>&1 || { tail "$OUT/build-test.log"; fail "make veranda-test"; }
A="build/veranda/Veranda Write.cvt"
field() { $PY "$SDK/tools/vpack.py" --list "$1" | sed -n "s/^$2 *//p"; }
[ "$(field "$A" 'geos type')" = 6 ] && [ "$(field "$A" veranda)" = True ] && [ "$(field "$A" mode)" = 640x480 ] \
	|| fail "$A: not an application (VRND, 640x480)"
$PY "$SDK/tools/vpack.py" --list "$A" | grep -q "CWR" || fail "$A: no documents CWR"
grep -v "^python3\|^cl65\|^/" "$OUT/build.log" | sed 's/^/      /'
ok "make veranda: $A ($(stat -c %s "$A") bytes)"
$PY tools/check_ovl.py build/veranda || fail "build/veranda: a call into an overlay"
$PY tools/check_ovl.py build/veranda/test || fail "build/veranda/test: a call into an overlay"
ok "no call into an overlay that may not be mapped (tools/check_ovl.py, both builds)"

mkdir -p build/desk
cl65 -t none --cpu 65C02 -O -I "$SDK/include" -C "$SDK/cfg/app-c.cfg" -o build/desk/desk.bin tools/desk/desk.c \
	"$SDK/lib/veranda.lib" || fail "the test desktop"
$PY "$SDK/tools/vpack.py" --load 0x0801 -o build/desk/SHELL.cvt tools/desk/desk.hdr build/desk/desk.bin >/dev/null
# (W68) the stand-in for Veranda Ask (AI help's gate: no network, no key)
cl65 -t none --cpu 65C02 -O -I "$SDK/include" -C "$SDK/cfg/app-c.cfg" -o build/desk/askfake.bin tools/askfake/askfake.c \
	"$SDK/lib/veranda.lib" || fail "the stand-in for Veranda Ask"
$PY "$SDK/tools/vpack.py" --load 0x0801 -o "build/desk/Veranda Ask.cvt" tools/askfake/askfake.hdr build/desk/askfake.bin >/dev/null

# the session on the host folder (W56: without the dictionaries, NODICT:
# its Polish screenshot is compared with the font's drawing, which has no
# spelling marks under its words; the other sessions have them)
export NODICT=1
tools/run.sh "$OUT/run" > /dev/null || fail "the session (host folder)"
echo "== the session on the host folder"
chk "$OUT/run" || fail "the session (host folder)"
$PY "$SDK/tools/vram2png.py" --color "$OUT/run/dump.bin" "$OUT/end.png" 2>/dev/null || true
ok "the session: states, files and Alt+Tab as the model's"

# screenshots: the session stopped at points, side by side
SHOTS="start styles selbold pasted saveas popup menu paras find replaced count polish latin big bigrepl gwlist gwask gwnote gwdoc gwsave"
for s in $SHOTS; do
	tools/run.sh "$OUT/shot-$s" "$s" > "$OUT/shot-$s.log" 2>&1 &
done
wait
for s in $SHOTS; do
	[ -f "$OUT/shot-$s/dump.bin" ] || { tail -3 "$OUT/shot-$s.log"; fail "the session stopped at $s"; }
	$PY tools/check.py "$OUT/shot-$s" --partial > /dev/null || fail "the session stopped at $s: its states"
	$PY "$SDK/tools/vram2png.py" --color "$OUT/shot-$s/dump.bin" "$OUT/$s.png" 2>/dev/null || true
done
$PY tools/check_polish.py "$OUT/polish.png" "build/fonts/Gate Sans.cvt" 12 "Zażółć gęślą jaźń" "ZAŻÓŁĆ GĘŚLĄ JAŹŃ" \
	|| fail "Polish: the screenshot's text is not the font's"
mkdir -p build/shots
for s in paras find replaced count polish latin bigrepl gwlist gwask gwnote gwdoc; do cp "$OUT/$s.png" build/shots/ 2>/dev/null || true; done
ok "screenshots: $OUT/{$(echo $SHOTS | tr ' ' ,),end}.png; build/shots/"

# the session on an SD card image under R49
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	S=$OUT/sd
	BIG_SIZE=200000 tools/run.sh "$S/card" "" "$R49" --prepare > /dev/null || fail "the SD card's files"
	mkdir -p "$S/out"
	cp "$S/card/expect.json" "$S/out/"
	$PY "$SDK/tools/vcard.py" make --mb 64 "$S/sd.img" "$S/card" >/dev/null || fail "vcard make"
	cp "$SDK/bin/VERANDA.PRG" "$S/"
	(cd "$S" && SDL_VIDEODRIVER=dummy timeout -s INT 600 "$EMU" -rom "$R49" -sdcard sd.img -prg VERANDA.PRG \
		-run -warp -sound none -echo -dump V -ram 2048 > out/console.txt 2>&1) || true
	[ -f "$S/dump.bin" ] || { tail -c 300 "$S/out/console.txt"; fail "SD card: the session did not finish"; }
	for f in "Stage one test.cwr" Styles.cwr BIG2.cwr "Geo letter.cwr" "Geo letter.cvt" Polish.cwr Codes.cwr; do
		$PY "$SDK/tools/vcard.py" get "$S/sd.img" "$f" "$S/out/$f" >/dev/null || fail "SD card: no $f"
	done
	echo "== the session on an SD card image (R49)"
	chk "$S/out" || fail "the session (SD card, R49)"
	ok "the same on an SD card image under R49"
fi
unset NODICT
# stage 3 (W44-W48): pages, the header and footer, the page setup, PDF
# (tools/script.py's session_page: SESSION=page) on a host folder under
# R49 at 512 KB, its screenshots, then on an SD card image under R49
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	export SESSION=page RAM=512
	TIMEOUT=900 tools/run.sh "$OUT/page" "" "$R49" > /dev/null || fail "the page session (host folder)"
	echo "== pages and PDF (host folder, R49, 512 KB)"
	chk "$OUT/page" || fail "the page session (host folder)"
	ok "pages, the header and footer, the page setup, CWRD 1.4, PDF"
	PSHOTS="pbreak hfpanel bands setup pdfdlg old goto bigpages"
	for s in $PSHOTS; do
		TIMEOUT=900 tools/run.sh "$OUT/pshot-$s" "$s" "$R49" > "$OUT/pshot-$s.log" 2>&1 &
	done
	wait
	for s in $PSHOTS; do
		[ -f "$OUT/pshot-$s/dump.bin" ] || { tail -3 "$OUT/pshot-$s.log"; fail "the page session stopped at $s"; }
		$PY tools/check.py "$OUT/pshot-$s" --partial > /dev/null || fail "the page session stopped at $s: its states"
		$PY "$SDK/tools/vram2png.py" --color "$OUT/pshot-$s/dump.bin" "$OUT/$s.png" 2>/dev/null || true
		cp "$OUT/$s.png" build/shots/ 2>/dev/null || true
	done
	ok "screenshots: $OUT/{$(echo $PSHOTS | tr ' ' ,)}.png; build/shots/"
	S=$OUT/sdpage
	tools/run.sh "$S/card" "" "$R49" --prepare > /dev/null || fail "the SD card's files (page)"
	mkdir -p "$S/out"
	cp "$S/card/expect.json" "$S/card/XBIG.cwr" "$S/out/"
	$PY "$SDK/tools/vcard.py" make --mb 64 "$S/sd.img" "$S/card" >/dev/null || fail "vcard make"
	cp "$SDK/bin/VERANDA.PRG" "$S/"
	(cd "$S" && SDL_VIDEODRIVER=dummy timeout -s INT 900 "$EMU" -rom "$R49" -sdcard sd.img -prg VERANDA.PRG \
		-run -warp -sound none -echo -dump V -ram 512 > out/console.txt 2>&1) || true
	[ -f "$S/dump.bin" ] || { tail -c 300 "$S/out/console.txt"; fail "SD card, page: the session did not finish"; }
	mkdir -p "$S/out/DOCUMENTS"
	for f in Pages.cwr DOCUMENTS/Pages.pdf DOCUMENTS/XBIG.pdf; do
		$PY "$SDK/tools/vcard.py" get "$S/sd.img" "$f" "$S/out/$f" >/dev/null || fail "SD card, page: no $f"
	done
	$PY "$SDK/tools/vcard.py" get "$S/sd.img" DOCUMENTS/Stop.pdf "$S/out/Stop.pdf" >/dev/null 2>&1 \
		&& fail "SD card, page: the stopped export's Stop.pdf left on the card"
	echo "== pages and PDF on an SD card image (R49, 512 KB)"
	chk "$S/out" || fail "the page session (SD card, R49)"
	ok "the same on an SD card image under R49"
	unset SESSION RAM
fi
# (W61) spelling without the dictionaries, as Veranda's package has it now
# (tools/script.py's session_nodict: SESSION=nodict, a host folder under
# R49 at 512 KB, only SPELL.OVL in /VERANDA/DICT): live checking stays
# quiet, F7's panel says where they come from, nothing said while typing;
# Tools > Get dictionaries: START.REQ and UPDATE.REQ written,
# Write put aside and back; English arrives while Write runs and F7 finds
# a word; its screenshots
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	export SESSION=nodict RAM=512 NODICT=1 DICTNEW=1 UPDSTUB=1
	tools/run.sh "$OUT/nodict" "" "$R49" > /dev/null || fail "the nodict session"
	echo "== spelling without the dictionaries (host folder, R49, 512 KB)"
	chk "$OUT/nodict" || fail "the nodict session"
	ok "no dictionary: quiet while typing, F7 says where they come from, Get dictionaries hands over to Veranda Update, a dictionary that arrives is found"
	NSHOTS="ndpanel ndask ndfound"
	for s in $NSHOTS; do
		tools/run.sh "$OUT/sshot-$s" "$s" "$R49" > "$OUT/sshot-$s.log" 2>&1 &
	done
	wait
	for s in $NSHOTS; do
		[ -f "$OUT/sshot-$s/dump.bin" ] || { tail -3 "$OUT/sshot-$s.log"; fail "the nodict session stopped at $s"; }
		$PY tools/check.py "$OUT/sshot-$s" --partial > /dev/null || fail "the nodict session stopped at $s: its checks"
		$PY "$SDK/tools/vram2png.py" --color "$OUT/sshot-$s/dump.bin" "$OUT/$s.png" 2>/dev/null || true
		cp "$OUT/$s.png" build/shots/ 2>/dev/null || true
	done
	ok "screenshots: $OUT/{$(echo $NSHOTS | tr ' ' ,)}.png"
	unset SESSION RAM NODICT DICTNEW UPDSTUB
fi
# stage 4 (W49-W54): .docx and .odt opened and exported (tools/script.py's
# session_office: SESSION=office) on a host folder under R49 at 512 KB, its
# screenshots, then on an SD card image under R49
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	export SESSION=office RAM=512
	TIMEOUT=900 tools/run.sh "$OUT/office" "" "$R49" > /dev/null || fail "the office session (host folder)"
	echo "== .docx and .odt (host folder, R49, 512 KB)"
	chk "$OUT/office" || fail "the office session (host folder)"
	ok "Word and OpenDocument documents opened (converted) and exported (read back, LibreOffice)"
	OSHOTS="oxnote oxdoc oxsave oxodt"
	for s in $OSHOTS; do
		TIMEOUT=900 tools/run.sh "$OUT/oshot-$s" "$s" "$R49" > "$OUT/oshot-$s.log" 2>&1 &
	done
	wait
	for s in $OSHOTS; do
		[ -f "$OUT/oshot-$s/dump.bin" ] || { tail -3 "$OUT/oshot-$s.log"; fail "the office session stopped at $s"; }
		$PY tools/check.py "$OUT/oshot-$s" --partial > /dev/null || fail "the office session stopped at $s: its states"
		$PY "$SDK/tools/vram2png.py" --color "$OUT/oshot-$s/dump.bin" "$OUT/$s.png" 2>/dev/null || true
		cp "$OUT/$s.png" build/shots/ 2>/dev/null || true
	done
	ok "screenshots: $OUT/{$(echo $OSHOTS | tr ' ' ,)}.png; build/shots/"
	S=$OUT/sdoffice
	tools/run.sh "$S/card" "" "$R49" --prepare > /dev/null || fail "the SD card's files (office)"
	mkdir -p "$S/out/DOCUMENTS"
	cp "$S/card/expect.json" "$S/out/"
	$PY "$SDK/tools/vcard.py" make --mb 64 "$S/sd.img" "$S/card" >/dev/null || fail "vcard make"
	cp "$SDK/bin/VERANDA.PRG" "$S/"
	(cd "$S" && SDL_VIDEODRIVER=dummy timeout -s INT 900 "$EMU" -rom "$R49" -sdcard sd.img -prg VERANDA.PRG \
		-run -warp -sound none -echo -dump V -ram 512 > out/console.txt 2>&1) || true
	[ -f "$S/dump.bin" ] || { tail -c 300 "$S/out/console.txt"; fail "SD card, office: the session did not finish"; }
	for f in "DOCUMENTS/Report.docx" "DOCUMENTS/Report.odt" "DOCUMENTS/Big report.docx" "DOCUMENTS/Big report.odt"; do
		$PY "$SDK/tools/vcard.py" get "$S/sd.img" "$f" "$S/out/$f" >/dev/null || fail "SD card, office: no $f"
	done
	echo "== .docx and .odt on an SD card image (R49, 512 KB)"
	chk "$S/out" || fail "the office session (SD card, R49)"
	ok "the same on an SD card image under R49"
	unset SESSION RAM
fi
# (W71) opening timed: LibreOffice's .docx from HTML (tests/office: the
# Veranda Desktop Guide, Writing and Asking on Veranda, the Guide as
# .odt), Report.docx, Notes.odt, the big .docx, tools/oxstress.py's
# Stress.docx and the Guide deflated with fixed codes and in stored
# blocks, the Guide exported both ways (tools/script.py's session_oxspeed:
# SESSION=oxspeed) on a host folder and on an SD card image under R49 at
# 512 KB: each state and note against the host's conversion, the times
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	export SESSION=oxspeed RAM=512
	TIMEOUT=900 tools/run.sh "$OUT/oxspeed" "" "$R49" > /dev/null || fail "the oxspeed session (host folder)"
	echo "== opening timed (host folder, R49, 512 KB)"
	chk "$OUT/oxspeed" || fail "the oxspeed session (host folder)"
	ok "the user's kind of .docx and the stress fixtures opened as the host converts them, timed"
	S=$OUT/sdoxspeed
	tools/run.sh "$S/card" "" "$R49" --prepare > /dev/null || fail "the SD card's files (oxspeed)"
	mkdir -p "$S/out/DOCUMENTS"
	cp "$S/card/expect.json" "$S/out/"
	$PY "$SDK/tools/vcard.py" make --mb 64 "$S/sd.img" "$S/card" >/dev/null || fail "vcard make"
	cp "$SDK/bin/VERANDA.PRG" "$S/"
	(cd "$S" && SDL_VIDEODRIVER=dummy timeout -s INT 900 "$EMU" -rom "$R49" -sdcard sd.img -prg VERANDA.PRG \
		-run -warp -sound none -echo -dump V -ram 512 > out/console.txt 2>&1) || true
	[ -f "$S/dump.bin" ] || { tail -c 300 "$S/out/console.txt"; fail "SD card, oxspeed: the session did not finish"; }
	for f in "DOCUMENTS/Veranda Desktop Guide.docx" "DOCUMENTS/Veranda Desktop Guide.odt"; do
		$PY "$SDK/tools/vcard.py" get "$S/sd.img" "$f" "$S/out/$f" >/dev/null || fail "SD card, oxspeed: no $f"
	done
	echo "== opening timed on an SD card image (R49, 512 KB)"
	chk "$S/out" || fail "the oxspeed session (SD card, R49)"
	ok "the same on an SD card image under R49"
	unset SESSION RAM
fi
# the partial redraw against a full one (W55; tools/script.py's
# session_redraw: SESSION=redraw, a host folder under R49 at 512 KB): after
# every change of the layout (paragraph styles, sizes, fonts, B I U,
# alignment, indents, spacing, lists, the page setup, undo and redo,
# with and without a selection, at the end, in the middle, scrolled off
# the top, a footer with {pages}) the text area as the edits drew it
# against the text wrapped again and drawn whole (src/v/test.c)
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	export SESSION=redraw RAM=512
	TIMEOUT=900 tools/run.sh "$OUT/redraw" "" "$R49" > /dev/null || fail "the redraw session (host folder)"
	echo "== the partial redraw against a full one (host folder, R49, 512 KB)"
	chk "$OUT/redraw" || fail "the redraw session"
	ok "every change drawn as a full drawing draws it"
	RSHOTS="rdhead rdbug rdtop1 rdend"
	for s in $RSHOTS; do
		TIMEOUT=900 tools/run.sh "$OUT/rshot-$s" "$s" "$R49" > "$OUT/rshot-$s.log" 2>&1 &
	done
	wait
	for s in $RSHOTS; do
		[ -f "$OUT/rshot-$s/dump.bin" ] || { tail -3 "$OUT/rshot-$s.log"; fail "the redraw session stopped at $s"; }
		$PY tools/check.py "$OUT/rshot-$s" --partial > /dev/null || fail "the redraw session stopped at $s: its checks"
		$PY "$SDK/tools/vram2png.py" --color "$OUT/rshot-$s/dump.bin" "$OUT/$s.png" 2>/dev/null || true
		cp "$OUT/$s.png" build/shots/ 2>/dev/null || true
	done
	ok "screenshots: $OUT/{$(echo $RSHOTS | tr ' ' ,)}.png; build/shots/"
	unset SESSION RAM
fi
# spelling (W56-W59; tools/script.py's session_spell: SESSION=spell, under
# R49 at 512 KB, on a host folder and on an SD card image): live marks in
# a Polish and an English document, Check spelling (Change, Change all,
# Ignore, Ignore all, Add: the user's words' file), the pop-up (a right
# click, F8), the Tools menu, Alt+Tab; the lookups and Check spelling over
# 2,000 words timed (as Writer did it, from cold, again); its screenshots
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	export SESSION=spell RAM=512
	TIMEOUT=900 tools/run.sh "$OUT/spell" "" "$R49" > /dev/null || fail "the spell session (host folder)"
	echo "== spelling (host folder, R49, 512 KB)"
	chk "$OUT/spell" || fail "the spell session"
	ok "live marks, Check spelling, the pop-up, the user's words, both languages, the settings kept after a restart"
	SSHOTS="splivepl spdlg spdone spliveen sppop splang spopts spnomark"
	for s in $SSHOTS; do
		TIMEOUT=900 tools/run.sh "$OUT/sshot-$s" "$s" "$R49" > "$OUT/sshot-$s.log" 2>&1 &
	done
	wait
	for s in $SSHOTS; do
		[ -f "$OUT/sshot-$s/dump.bin" ] || { tail -3 "$OUT/sshot-$s.log"; fail "the spell session stopped at $s"; }
		$PY tools/check.py "$OUT/sshot-$s" --partial > /dev/null || fail "the spell session stopped at $s: its checks"
		$PY "$SDK/tools/vram2png.py" --color "$OUT/sshot-$s/dump.bin" "$OUT/$s.png" 2>/dev/null || true
		cp "$OUT/$s.png" build/shots/ 2>/dev/null || true
	done
	ok "screenshots: $OUT/{$(echo $SSHOTS | tr ' ' ,)}.png; build/shots/"
	S=$OUT/sdspell
	CARD=1 tools/run.sh "$S/card" "" "$R49" --prepare > /dev/null || fail "the SD card's files (spell)"
	mkdir -p "$S/out/VERANDA/.SYSTEM"
	cp "$S/card/expect.json" "$S/out/"
	$PY "$SDK/tools/vcard.py" make --mb 64 "$S/sd.img" "$S/card" >/dev/null || fail "vcard make"
	cp "$SDK/bin/VERANDA.PRG" "$S/"
	(cd "$S" && SDL_VIDEODRIVER=dummy timeout -s INT 1200 "$EMU" -rom "$R49" -sdcard sd.img -prg VERANDA.PRG \
		-run -warp -sound none -echo -dump V -ram 512 > out/console.txt 2>&1) || true
	[ -f "$S/dump.bin" ] || { tail -c 300 "$S/out/console.txt"; fail "SD card, spell: the session did not finish"; }
	for f in Polski.cwr English.cwr VERANDA/.SYSTEM/WRITE.UDIC VERANDA/.SYSTEM/WRITE.CFG; do
		$PY "$SDK/tools/vcard.py" get "$S/sd.img" "$f" "$S/out/$f" >/dev/null || fail "SD card, spell: no $f"
	done
	echo "== spelling on an SD card image (R49, 512 KB)"
	chk "$S/out" || fail "the spell session (SD card, R49)"
	ok "the same on an SD card image under R49"
	unset SESSION RAM
fi
# (1.6, W62-W66) tables (tools/script.py's session_table: SESSION=table,
# R49 at 512 KB, on a host folder and on an SD card image): Insert >
# Table, typing, Tab, Return in a cell, rows and columns added and
# deleted, widths, borders, undo and redo, the edges (refused), saved and
# opened again, exported as PDF, .docx and .odt (read back: the tables);
# each step's tables' codes and the redraw check; its screenshots. Then
# typing in a table timed against typing in a paragraph (SESSION=tspeed)
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	export SESSION=table RAM=512
	TIMEOUT=900 tools/run.sh "$OUT/table" "" "$R49" > /dev/null || fail "the table session (host folder)"
	echo "== tables (host folder, R49, 512 KB)"
	chk "$OUT/table" || fail "the table session (host folder)"
	ok "tables: made, typed in, edited, undone, saved, opened, exported (PDF, .docx, .odt read back)"
	TSHOTS="tbdlg tbtyped tbrow tbmenu tbcols tbwdlg tbwidths tbnogrid tbrefused tbopened"
	for s in $TSHOTS; do
		TIMEOUT=900 tools/run.sh "$OUT/tshot-$s" "$s" "$R49" > "$OUT/tshot-$s.log" 2>&1 &
	done
	wait
	for s in $TSHOTS; do
		[ -f "$OUT/tshot-$s/dump.bin" ] || { tail -3 "$OUT/tshot-$s.log"; fail "the table session stopped at $s"; }
		$PY tools/check.py "$OUT/tshot-$s" --partial > /dev/null || fail "the table session stopped at $s: its checks"
		$PY "$SDK/tools/vram2png.py" --color "$OUT/tshot-$s/dump.bin" "$OUT/$s.png" 2>/dev/null || true
		cp "$OUT/$s.png" build/shots/ 2>/dev/null || true
	done
	ok "screenshots: $OUT/{$(echo $TSHOTS | tr ' ' ,)}.png; build/shots/"
	S=$OUT/sdtable
	tools/run.sh "$S/card" "" "$R49" --prepare > /dev/null || fail "the SD card's files (table)"
	mkdir -p "$S/out/DOCUMENTS"
	cp "$S/card/expect.json" "$S/out/"
	$PY "$SDK/tools/vcard.py" make --mb 64 "$S/sd.img" "$S/card" >/dev/null || fail "vcard make"
	cp "$SDK/bin/VERANDA.PRG" "$S/"
	(cd "$S" && SDL_VIDEODRIVER=dummy timeout -s INT 900 "$EMU" -rom "$R49" -sdcard sd.img -prg VERANDA.PRG \
		-run -warp -sound none -echo -dump V -ram 512 > out/console.txt 2>&1) || true
	[ -f "$S/dump.bin" ] || { tail -c 300 "$S/out/console.txt"; fail "SD card, table: the session did not finish"; }
	for f in Table.cwr DOCUMENTS/Table.pdf DOCUMENTS/Table.docx DOCUMENTS/Table.odt; do
		$PY "$SDK/tools/vcard.py" get "$S/sd.img" "$f" "$S/out/$f" >/dev/null || fail "SD card, table: no $f"
	done
	echo "== tables on an SD card image (R49, 512 KB)"
	chk "$S/out" || fail "the table session (SD card, R49)"
	ok "the same on an SD card image under R49"
	export SESSION=tspeed
	TIMEOUT=900 tools/run.sh "$OUT/tspeed" "" "$R49" > /dev/null || fail "the tspeed session"
	echo "== typing in a table against typing in a paragraph (host folder, R49, 512 KB)"
	chk "$OUT/tspeed" || fail "the tspeed session"
	ok "typing in tables timed"
	unset SESSION RAM
fi
# (1.6, W67) templates (tools/script.py's session_templ: SESSION=templ,
# TPLCFG=1, the clock the host's (-rtc), R49 at 512 KB, on a host folder
# and on an SD card image): the chooser, each English template, the
# Polish ones (the keyboard Polish), {date} today's in the template's
# language, Save as template into /DOCUMENTS/Templates, listed as the
# user's and opened, a blank document; Veranda's files unchanged; the
# screenshots
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	export SESSION=templ RAM=512 TPLCFG=1
	TIMEOUT=900 tools/run.sh "$OUT/templ" "" "$R49" > /dev/null || fail "the templ session (host folder)"
	echo "== templates (host folder, R49, 512 KB)"
	chk "$OUT/templ" || fail "the templ session (host folder)"
	ok "templates: chosen, opened as Untitled with today's date, one saved as the user's and opened"
	PLSHOTS="tpchoose tpletter tpcv tpreport tpmeet tpchoosepl tpraport tpsave tpmine tpcvpl"
	for s in $PLSHOTS; do
		TIMEOUT=900 tools/run.sh "$OUT/tpshot-$s" "$s" "$R49" > "$OUT/tpshot-$s.log" 2>&1 &
	done
	wait
	for s in $PLSHOTS; do
		[ -f "$OUT/tpshot-$s/dump.bin" ] || { tail -3 "$OUT/tpshot-$s.log"; fail "the templ session stopped at $s"; }
		$PY tools/check.py "$OUT/tpshot-$s" --partial > /dev/null || fail "the templ session stopped at $s: its checks"
		$PY "$SDK/tools/vram2png.py" --color "$OUT/tpshot-$s/dump.bin" "$OUT/$s.png" 2>/dev/null || true
		cp "$OUT/$s.png" build/shots/ 2>/dev/null || true
	done
	ok "screenshots: $OUT/{$(echo $PLSHOTS | tr ' ' ,)}.png; build/shots/"
	S=$OUT/sdtempl
	tools/run.sh "$S/card" "" "$R49" --prepare > /dev/null || fail "the SD card's files (templ)"
	mkdir -p "$S/out/DOCUMENTS/Templates" "$S/out/VERANDA/TEMPLATES/EN" "$S/out/VERANDA/TEMPLATES/PL"
	cp "$S/card/expect.json" "$S/out/"
	$PY "$SDK/tools/vcard.py" make --mb 64 "$S/sd.img" "$S/card" >/dev/null || fail "vcard make"
	cp "$SDK/bin/VERANDA.PRG" "$S/"
	(cd "$S" && SDL_VIDEODRIVER=dummy timeout -s INT 900 "$EMU" -rom "$R49" -sdcard sd.img -prg VERANDA.PRG \
		-run -warp -sound none -echo -dump V -ram 512 -rtc > out/console.txt 2>&1) || true
	[ -f "$S/dump.bin" ] || { tail -c 300 "$S/out/console.txt"; fail "SD card, templ: the session did not finish"; }
	for f in "DOCUMENTS/Templates/Moj raport.cwr" VERANDA/TEMPLATES/EN/Letter.cwr VERANDA/TEMPLATES/PL/Raport.cwr; do
		$PY "$SDK/tools/vcard.py" get "$S/sd.img" "$f" "$S/out/$f" >/dev/null || fail "SD card, templ: no $f"
	done
	echo "== templates on an SD card image (R49, 512 KB)"
	chk "$S/out" || fail "the templ session (SD card, R49)"
	ok "the same on an SD card image under R49"
	unset SESSION RAM TPLCFG
fi
# (W68) AI help (tools/script.py's session_ai: SESSION=ai, R49 at 512 KB,
# a host folder, ASKFAKE=1: tools/askfake stands in for Veranda Ask, no
# network, no key): every action, undo, Polish, cancelled, not signed in,
# an error, an answer that comes later or after the text changed, a cell,
# refused selections, Tools > AI help off and on, a text too long; each
# request's AI.IN and AI.REQ byte for byte, the redraw check; screenshots
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	export SESSION=ai RAM=512 NODICT=1 ASKFAKE=1
	TIMEOUT=900 tools/run.sh "$OUT/ai" "" "$R49" > /dev/null || fail "the ai session"
	echo "== AI help with a stand-in for Veranda Ask (host folder, R49, 512 KB)"
	chk "$OUT/ai" || fail "the ai session"
	ok "AI help: the requests as Veranda's docs/AIREQ.md, the answers put in as one undo step, the refusals, the errors"
	ASHOTS="aimenu aiask aibefore aiafter aisummary ainosign aierror ainotyet aichanged aicell aicells aioff aitools aiabout ailong ainosignlist aibig aiaside ainostart"
	for s in $ASHOTS; do
		TIMEOUT=900 tools/run.sh "$OUT/ashot-$s" "$s" "$R49" > "$OUT/ashot-$s.log" 2>&1 &
	done
	wait
	for s in $ASHOTS; do
		[ -f "$OUT/ashot-$s/dump.bin" ] || { tail -3 "$OUT/ashot-$s.log"; fail "the ai session stopped at $s"; }
		$PY tools/check.py "$OUT/ashot-$s" --partial > /dev/null || fail "the ai session stopped at $s: its checks"
		$PY "$SDK/tools/vram2png.py" --color "$OUT/ashot-$s/dump.bin" "$OUT/$s.png" 2>/dev/null || true
		cp "$OUT/$s.png" build/shots/ 2>/dev/null || true
	done
	ok "screenshots: $OUT/{$(echo $ASHOTS | tr ' ' ,)}.png; build/shots/"
	# (W69) the not-signed-in box over a long list as over blank paper: opaque, framed
	$PY tools/check_box.py "$SDK" "$OUT/ashot-ainosignlist/dump.bin" "$OUT/ashot-ainosign/dump.bin" 96 176 543 271 \
		|| fail "the AI box over the page's text: not opaque"
	ok "the AI help box opaque over a long list (its rectangle as over blank paper, its frame drawn)"
	unset SESSION RAM NODICT ASKFAKE
fi
# (W73) scrolling timed and checked (tools/script.py's session_scroll:
# SESSION=scroll, a host folder under R49 at 512 KB): a plain long
# document, the converted "Veranda Desktop Guide.docx" (headings, lists,
# tables) and a document with page bands (a header and a footer): a line
# down and up at the screen's edge, a held arrow key, the wheel a notch
# and five at once, Page Down and Up, the scroll bar's arrows and track,
# its thumb dragged, a selection made by Shift+Down and scrolled by the
# wheel, the spelling marks on; after each, the screen against a full
# drawing (src/v/test.c's redraw check, W55); the times and the main
# loop's longest pause (tools/check.py: "keys ...", "time ...")
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	export SESSION=scroll RAM=512
	TIMEOUT=900 tools/run.sh "$OUT/scroll" "" "$R49" > /dev/null || fail "the scroll session"
	echo "== scrolling (host folder, R49, 512 KB)"
	chk "$OUT/scroll" || fail "the scroll session"
	ok "scrolling: every step drawn as a full drawing draws it, timed"
	unset SESSION RAM
fi
# the per-key timings (W43; tools/speed.sh: SESSION=speed, a host folder
# under R49 at 512 KB): typing at a document's end, in a long paragraph,
# in styles, Backspace, Return, one-line paragraphs, a 400 KB document;
# 60 keys at 30 a second, 8 at once: none lost (60 a second and the big
# document's 30: counted, not checked)
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	echo "== the per-key timings (host folder, R49, 512 KB)"
	r=0
	OUT=$OUT/speed tools/speed.sh > "$OUT/speed.txt" 2>&1 || r=$?
	sed 's/^/      /' "$OUT/speed.txt" | grep -v "^      state\|^      swap\|^      time\|^      mark"
	[ $r = 0 ] || fail "the per-key timings: a key lost, or a state wrong"
	ok "the per-key timings: no key lost at 30 a second"
fi
# a document larger than RAM (SESSION=big: tools/script.py's session_big,
# BIGDOC bytes, 1,000,000 by default) on an SD card image under R49, at
# 512 KB and at 2 MB of RAM banks: opened, the end, the start, Go to page,
# found in the middle, typed in, undone, redone, Alt+Tab, saved, opened
# again; its pages read back from the card
if [ "${SD:-1}" != 0 ] && [ -f "$R49" ]; then
	for R in ${BIGRAM:-512 2048}; do
		S=$OUT/sdbig$R
		SESSION=big BIG_STALE=1 BIG_SIZE=${BIGDOC:-1000000} tools/run.sh "$S/card" "" "$R49" --prepare > /dev/null \
			|| fail "the SD card's files (big, $R KB)"
		# a stale swap (a Veranda Write that did not end): removed at the start
		mkdir -p "$S/card/VERANDA/.SYSTEM/CWSWAP.0"
		head -c 8192 /dev/urandom > "$S/card/VERANDA/.SYSTEM/CWSWAP.0/S000"
		mkdir -p "$S/out"
		cp "$S/card/expect.json" "$S/out/"
		$PY "$SDK/tools/vcard.py" make --mb 64 "$S/sd.img" "$S/card" >/dev/null || fail "vcard make"
		cp "$SDK/bin/VERANDA.PRG" "$S/"
		(cd "$S" && SDL_VIDEODRIVER=dummy timeout -s INT ${BIGTIME:-1500} "$EMU" -rom "$R49" -sdcard sd.img \
			-prg VERANDA.PRG -run -warp -sound none -echo -dump V -ram $R > out/console.txt 2>&1) || true
		[ -f "$S/dump.bin" ] || { tail -c 300 "$S/out/console.txt"; fail "SD card, big, $R KB: the session did not finish"; }
		$PY "$SDK/tools/vcard.py" get "$S/sd.img" BIG2.cwr "$S/out/BIG2.cwr" >/dev/null || fail "SD card, big: no BIG2.cwr"
		$PY "$SDK/tools/vcard.py" get "$S/sd.img" BIG.cwr "$S/out/BIG.cwr" >/dev/null || fail "SD card, big: no BIG.cwr"
		cmp -s "$S/out/BIG.cwr" "$S/card/BIG.cwr" || fail "SD card, big: the original BIG.cwr changed"
		grep -aq "CW STALE SWAP REMOVED" "$S/out/console.txt" || fail "SD card, big: the stale swap not removed"
		if $PY "$SDK/tools/vcard.py" get "$S/sd.img" VERANDA/.SYSTEM/CWSWAP.0/S000 "$S/out/S000" >/dev/null 2>&1; then
			cmp -s "$S/out/S000" "$S/card/VERANDA/.SYSTEM/CWSWAP.0/S000" && fail "SD card, big: the stale swap file still there"
		fi
		$PY "$SDK/tools/vram2png.py" --color "$S/dump.bin" "$OUT/sdbig$R.png" 2>/dev/null || true
		echo "== a document larger than RAM ($((${BIGDOC:-1000000} / 1000)) KB) on an SD card image (R49), $R KB of RAM banks"
		chk "$S/out" || fail "the big document (SD card, $R KB)"
		ok "the big document at $R KB (the original untouched, a stale swap removed)"
	done
fi
echo "GATE stage 4 (.docx and .odt) + stage 3 (pages, PDF) + stage 2 + geoWrite + big documents + Polish + redraw + spelling + tables + templates + AI help: PASS"
