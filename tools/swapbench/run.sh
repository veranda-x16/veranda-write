#!/bin/bash
# The SD card's speed for Veranda Write's swap (DECISIONS W29): bench.c
# as AUTOSTART on an SD card image under ROM R49, x16emu headless.
#   tools/swapbench/run.sh [SDK=build/sdk]
# Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).
set -e
cd "$(dirname "$0")/../.."
for a in "$@"; do export "$a"; done
SDK=${SDK:-build/sdk}
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
R49=${R49:-$HOME/Downloads/Release.R49.ROM.Image/rom.bin}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
export PATH=$CC65_HOME/bin:$HOME/.local/bin:$PATH
D=build/swapbench
rm -rf $D && mkdir -p $D/card
cl65 -t none --cpu 65C02 -O -I $SDK/include -C $SDK/cfg/app-c.cfg -o $D/bench.bin tools/swapbench/bench.c $SDK/lib/veranda.lib
python3 -I -B $SDK/tools/vpack.py --load 0x0801 -o $D/card/AUTOSTART.cvt tools/swapbench/bench.hdr $D/bench.bin >/dev/null
cp $SDK/bin/VERANDA.PRG $SDK/bin/VERANDA.SYS $D/card/
head -c 4194304 /dev/urandom > $D/card/BIG.BIN
python3 -I -B $SDK/tools/vcard.py make --mb 64 $D/sd.img $D/card >/dev/null
cp $SDK/bin/VERANDA.PRG $D/
(cd $D && SDL_VIDEODRIVER=dummy timeout -s INT 600 "$EMU" -rom "$R49" -sdcard sd.img -prg VERANDA.PRG -run -warp \
	-sound none -echo -ram 2048 > console.txt 2>&1) || true
tr '\r' '\n' < $D/console.txt | grep BENCH | while read -r a b c; do
	[ -n "$c" ] && printf '%-16s %8.3f s\n' "$b" "$(echo "ibase=16; $c" | bc | awk '{print $1/8e6}')" || echo "$a $b"
done
