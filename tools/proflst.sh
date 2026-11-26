#!/bin/bash
# (W71) the test build's modules' listings for tools/prof.py (its
# functions' places): <build>/lst/*.lst
cd "$(dirname "$0")/.."
B=${1:-build/veranda/test}; SDK=${SDK:-build/sdk}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
export PATH=$CC65_HOME/bin:$HOME/.local/bin:$PATH
mkdir -p "$B/lst"
T=$(mktemp -d)
for f in src/*.c src/v/*.c; do
	b=$(basename "$f" .c)
	case $f in src/hf.c) continue;; esac
	cl65 -t none --cpu 65C02 -O -I $SDK/include --asm-include-dir $SDK/include -I src -I src/v -DCW_TEST \
		-c -l "$B/lst/$b.lst" -o "$T/x.o" "$f" 2>/dev/null &
done
wait
for f in src/v/*.s; do
	b=$(basename "$f" .s)
	case $b in ohash) a="--asm-define OVL=19"; n=ohash19;; *) a=; n=$b.s;; esac
	cl65 -t none --cpu 65C02 -I $SDK/include --asm-include-dir $SDK/include $a -c -l "$B/lst/$n.lst" -o "$T/$b.o" "$f" 2>/dev/null
done
rm -rf "$T"
