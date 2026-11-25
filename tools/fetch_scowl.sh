#!/bin/sh
# fetch_scowl.sh -- rebuild dict/EN.DIC (W56): SCOWL 2020.12.07 (Kevin
# Atkinson's Spell Checker Oriented Word Lists, a permissive licence:
# dict/EN.DIC.txt) downloaded into build/dsrc/scowl (not committed), its
# english-, american- and british- words, upper, contractions and
# proper-names at sizes 10-70 merged by tools/build_dictionary.py.
#
#   tools/fetch_scowl.sh
#
# Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE).
set -e
cd "$(dirname "$0")/.."
D=build/dsrc/scowl
V=2020.12.07
mkdir -p "$D"
[ -f "$D/scowl.tar.gz" ] || curl -sfL -o "$D/scowl.tar.gz" \
    "https://downloads.sourceforge.net/project/wordlist/SCOWL/$V/scowl-$V.tar.gz"
[ -d "$D/scowl-$V" ] || tar xzf "$D/scowl.tar.gz" -C "$D"
F=$D/scowl-$V/final
L=""
for c in english american british; do
    for k in words upper contractions proper-names; do
        for n in 10 20 35 40 50 55 60 70; do
            [ -f "$F/$c-$k.$n" ] && L="$L $F/$c-$k.$n"
        done
    done
done
python3 -I tools/build_dictionary.py EN dict/EN.DIC $L --latin1 --apostrophe
