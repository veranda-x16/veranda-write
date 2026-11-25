#!/bin/bash
# Veranda Write's gate: one run of the session in a fresh host folder.
#
#   tools/run.sh <folder> [stop name] [ROM] [--prepare]
#
# (--prepare: the folder only, for an SD card image: tools/gate.sh)
#
# The folder gets the test build as AUTOSTART.cvt, Veranda's system, its
# fonts in VERANDA/FONTS, the test desktop as SHELL.cvt, the session
# (tools/script.py); x16emu runs it headless at full speed (-warp; the
# timings are the emulated 8 MHz clock's) until the player stops it
# (dump.bin: VRAM); console.txt is its log.
#
# Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).
set -e
cd "$(dirname "$0")/.."
D=$1
STOP=$2
ROM=${3:-${ROM:-$HOME/Projects/x16-rom/build/x16/rom.bin}}
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
SDK=${SDK:-build/sdk}
[ -n "$D" ] && [ "$D" != / ] || exit 2
mkdir -p "$D"
find "$D" -mindepth 1 -delete
mkdir -p "$D/VERANDA/FONTS"
cp "${APP:-build/veranda/test/Veranda Write.cvt}" "$D/AUTOSTART.cvt"
cp "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" "$D/"
cp build/desk/SHELL.cvt "$D/"
cp build/desk/SHELL.cvt "$D/VERANDA/"
cp build/fonts/Gate*.cvt "$D/VERANDA/FONTS/"
if [ -z "$NODICT" ]; then                   # (W56: the dictionaries, as the package puts them)
	mkdir -p "$D/VERANDA/DICT"
	cp dict/PL.DIC dict/EN.DIC "$D/VERANDA/DICT/"
fi
if [ -n "$DICTNEW" ]; then                 # (W61: English, to arrive while Write runs: src/v/test.c's 'L' $79)
	mkdir -p "$D/VERANDA/DICT"
	cp dict/EN.DIC "$D/VERANDA/DICT/EN.NEW"
fi
if [ -n "$UPDSTUB" ]; then                  # (W61: Tools > Get dictionaries looks for Veranda Update)
	mkdir -p "$D/VERANDA/APPS"
	printf 'not Veranda Update' > "$D/VERANDA/APPS/Veranda Update.cvt"
fi
# (W59) spelling's overlays: the program's own (its build folder's), always
mkdir -p "$D/VERANDA/DICT"
cp "$(dirname "${APP:-build/veranda/test/Veranda Write.cvt}")/SPELL.OVL" "$D/VERANDA/DICT/"
python3 -I -B tools/script.py "$D" ${STOP:+--stop $STOP}
[ "$4" = --prepare ] && exit 0
(cd "$D" && SDL_VIDEODRIVER=dummy timeout -s INT ${TIMEOUT:-400} "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
	-warp -sound none -echo -dump V ${RAM:+-ram $RAM} > console.txt 2>&1) || true
[ -f "$D/dump.bin" ] || { tail -c 400 "$D/console.txt"; echo "run.sh: the session did not finish ($D)"; exit 1; }
