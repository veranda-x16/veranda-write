#!/bin/bash
# (W71) opening .docx and .odt timed: tools/script.py's session_oxspeed
# (SESSION=oxspeed) under R49 at 512 KB on a host folder, or with CARD=1
# on an SD card image (as the gate's office session), the test build as
# it is (APP=...); tools/check.py prints the times and checks the states
#   tools/oxspeed.sh [OUT=build/oxspeed] [CARD=1] [APP=...]
set -e
cd "$(dirname "$0")/.."
for a in "$@"; do export "$a"; done
R49=${R49:-$HOME/Downloads/Release.R49.ROM.Image/rom.bin}
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
SDK=${SDK:-build/sdk}
OUT=${OUT:-build/oxspeed}
export SESSION=oxspeed RAM=512
if [ -z "$CARD" ]; then
	TIMEOUT=${TIMEOUT:-900} tools/run.sh "$OUT" "" "$R49" > /dev/null
	python3 -I -B tools/check.py "$OUT"
	exit
fi
rm -rf "$OUT"; mkdir -p "$OUT"
tools/run.sh "$OUT/card" "" "$R49" --prepare > /dev/null
mkdir -p "$OUT/out/DOCUMENTS"
cp "$OUT/card/expect.json" "$OUT/out/"
python3 -I -B "$SDK/tools/vcard.py" make --mb 64 "$OUT/sd.img" "$OUT/card" >/dev/null
cp "$SDK/bin/VERANDA.PRG" "$OUT/"
(cd "$OUT" && SDL_VIDEODRIVER=dummy timeout -s INT ${TIMEOUT:-900} "$EMU" -rom "$R49" -sdcard sd.img -prg VERANDA.PRG \
	-run -warp -sound none -echo -dump V -ram 512 > out/console.txt 2>&1) || true
[ -f "$OUT/dump.bin" ] || { tail -c 300 "$OUT/out/console.txt"; echo "oxspeed.sh: the session did not finish"; exit 1; }
for f in "DOCUMENTS/Veranda Desktop Guide.docx" "DOCUMENTS/Veranda Desktop Guide.odt"; do
	python3 -I -B "$SDK/tools/vcard.py" get "$OUT/sd.img" "$f" "$OUT/out/$f" >/dev/null
done
python3 -I -B tools/check.py "$OUT/out"
