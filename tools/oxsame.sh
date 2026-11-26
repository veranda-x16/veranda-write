#!/bin/bash
# (W71) the host's conversion of every file in a folder by two test_cw
# builds: the documents (.cwr) and the counts said byte for byte the same
#   tools/oxsame.sh <old test_cw> <new test_cw> <folder>
O=$1; N=$2; D=$3; T=$(mktemp -d); r=0
for f in "$D"/*.docx "$D"/*.odt; do
	[ -f "$f" ] || continue
	a=$("$O" ox "$f" "$T/a.cwr" 2>&1 | sed "s|$T/a.cwr||"); b=$("$N" ox "$f" "$T/b.cwr" 2>&1 | sed "s|$T/b.cwr||")
	if [ "$a" != "$b" ] || ! cmp -s "$T/a.cwr" "$T/b.cwr"; then echo "DIFFERENT: $f"; r=1; else echo "same: $(basename "$f")"; fi
done
rm -rf "$T"; exit $r
