#!/bin/bash
# Veranda Write's gate: stage 2.
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
#    of keys; tools/check.py: the states (characters, styles, paragraphs)
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
ok "unit tests: $(tail -1 "$OUT/unit.log")"

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

# the session on the host folder
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
echo "GATE stage 2 + geoWrite + big documents + Polish: PASS"
