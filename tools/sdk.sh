#!/bin/bash
# Veranda's SDK for building and testing Veranda Write, made from a
# Veranda tree without touching it: its HEAD exported (git archive) into
# build/geos, the kernel and the SDK built there (as Veranda's
# tools/mkdist.sh does into its work folder), the SDK installed in
# build/sdk, Veranda's fonts (Gate Sans, Gate Mono) in build/fonts.
#
#   tools/sdk.sh [GEOS=~/Projects/geos] [REF=HEAD]
#
# The package's own build passes its SDK instead: make veranda SDK=...
#
# Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).
set -e
cd "$(dirname "$0")/.."
for a in "$@"; do export "$a"; done
GEOS=${GEOS:-$HOME/Projects/geos}
REF=${REF:-HEAD}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
export PATH=$CC65_HOME/bin:$HOME/.local/bin:$PATH
PY="python3 -I -B"
rm -rf build/geos build/sdk build/fonts
mkdir -p build/geos
git -C "$GEOS" archive "$REF" | tar -x -C build/geos
(cd build/geos && $PY tools/apigen.py api/veranda.api build/gen >/dev/null && $PY tools/kbuild.py >/dev/null \
	&& tools/mksdk.sh ../sdk >/dev/null && $PY tools/fontcvt.py testfonts ../fonts >/dev/null)
echo "sdk.sh: Veranda $(git -C "$GEOS" rev-parse --short "$REF"): build/sdk, build/fonts"
