#!/bin/bash
# Veranda Write's per-key timings (DECISIONS W43): the speed session
# (tools/script.py's session_speed, SESSION=speed) in x16emu, the test
# build as it is (make veranda-test first, or tools/gate.sh), on a host
# folder under ROM R49 at 512 KB of RAM banks; tools/check.py prints each
# group of timed keys ("keys ...": from the key put in the ROM's queue to
# the program's drawing after it, in milliseconds at the emulated 8 MHz)
# and checks the states (no key lost).
#
#   tools/speed.sh [RAM=512] [BIG_SIZE=400000] [ROM=...] [OUT=build/speed]
#
# Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).
set -e
cd "$(dirname "$0")/.."
for a in "$@"; do export "$a"; done
R49=${R49:-$HOME/Downloads/Release.R49.ROM.Image/rom.bin}
OUT=${OUT:-build/speed}
export SESSION=speed RAM=${RAM:-512} TIMEOUT=${TIMEOUT:-900}
tools/run.sh "$OUT" "" "${ROM:-$R49}" > /dev/null
python3 -I -B tools/check.py "$OUT"
