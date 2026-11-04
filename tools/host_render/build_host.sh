#!/bin/bash
# Builds the PC render tool from this repository's firmware sources.
# usage: ./build_host.sh [path to Core] [output binary]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
CORE="${1:-$HERE/../../Core}"; OUT="${2:-$HERE/render}"
OBJ="$HERE/obj"; rm -rf "$OBJ"; mkdir -p "$OBJ"
FILES="$(cat "$HERE/filelist.txt") $(cat "$HERE/filelist_extra.txt") Src/Preset/initKit.c"
for f in $FILES; do
  [ -f "$CORE/$f" ] || continue
  gcc -std=gnu11 -O2 -g -w -fcommon -I"$HERE/stub" -I"$CORE/Inc" -c "$CORE/$f" -o "$OBJ/$(basename "${f%.c}").o"
done
for f in host_stubs.c host_render.c; do
  gcc -std=gnu11 -O2 -g -w -fcommon -I"$HERE/stub" -I"$HERE/src" -I"$CORE/Inc" -c "$HERE/src/$f" -o "$OBJ/${f%.c}.o"
done
gcc -fcommon "$OBJ"/*.o -lm -o "$OUT"
echo "built $OUT"
