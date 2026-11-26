#!/bin/bash
# (W71) x16emu with a flat profile, for tools/profrun.sh: a copy of the
# emulator's tree (EMUSRC, default ~/Projects/x16-emulator, untouched) in
# build/x16prof with tools/x16prof.patch (X16PROF=file: the cycles at
# each address, banked RAM by its bank, the C library's and the kernel's
# cycles their callers' (X16PROF_LIB); X16PROF_FROM, X16PROF_TO: from and
# to a "CW MARK name" (or "@word": any word) of the console)
set -e
cd "$(dirname "$0")/.."
EMUSRC=${EMUSRC:-$HOME/Projects/x16-emulator}
rm -rf build/x16prof
mkdir -p build/x16prof
rsync -a --exclude build --exclude .git "$EMUSRC/" build/x16prof/
(cd build/x16prof && patch -p1 -s < ../../tools/x16prof.patch && make -s -j8 > /dev/null)
echo "x16prof.sh: build/x16prof/build/x16emu"
