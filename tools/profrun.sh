#!/bin/bash
# a flat profile of opening one document (W71): tools/script.py's
# session_oxspeed with OXONLY=<letter> under build/x16prof's emulator
# (x16emu with X16PROF: the cycles at each address), R49 at 512 KB.
#   tools/profrun.sh <letter> <out folder> [card]
set -e
cd "$(dirname "$0")/.."
L=$1; D=$2
R49=${R49:-$HOME/Downloads/Release.R49.ROM.Image/rom.bin}
EMU=$PWD/build/x16prof/build/x16emu
SDK=${SDK:-build/sdk}
export SESSION=oxspeed RAM=512 OXONLY=$L
export X16PROF_FROM X16PROF_TO
export X16PROF_LIB=$(python3 -I -B tools/prof.py --libranges "${APPDIR:-build/veranda/test}")
tools/run.sh "$D" "" "$R49" --prepare > /dev/null
if [ "$3" = card ]; then
	mkdir -p "$D.sd"; rm -f "$D.sd/sd.img"
	python3 -I -B "$SDK/tools/vcard.py" make --mb 64 "$D.sd/sd.img" "$D" >/dev/null
	cp "$SDK/bin/VERANDA.PRG" "$D.sd/"
	(cd "$D.sd" && X16PROF=$PWD/../$(basename "$D").prof SDL_VIDEODRIVER=dummy timeout -s INT 900 "$EMU" -rom "$R49" -sdcard sd.img -prg VERANDA.PRG \
		-run -warp -sound none -echo -dump V -ram 512 > console.txt 2>&1) || true
	cp "$D.sd/console.txt" "$D/console.txt"
else
	(cd "$D" && X16PROF=$PWD/../$(basename "$D").prof SDL_VIDEODRIVER=dummy timeout -s INT 900 "$EMU" -rom "$R49" -prg VERANDA.PRG -run \
		-warp -sound none -echo -dump V -ram 512 > console.txt 2>&1) || true
fi
python3 -I -B tools/check.py "$D" | grep "^mark\|^time\|^state\|FAIL" | paste -sd' ' | sed 's/mark //g'
