#!/bin/bash
# Builds the parameter lock test (PC only) from this repository's firmware
# sources: the real menu, button handler, sequencer and sound engine code with
# the hardware stubbed out. Run the result to check the step lock handling.
# usage: ./build_plock.sh [path to Core] [output binary]
# SPDX-License-Identifier: GPL-3.0-only
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; HOST="$HERE/.."
CORE="$(cd "${1:-$HERE/../../../Core}" && pwd)"; OUT="${2:-$HERE/plock_test}"
OBJ="$HERE/obj_$(basename "$OUT")"; rm -rf "$OBJ"; mkdir -p "$OBJ"
CFLAGS="-std=gnu11 -O1 -g -w -fcommon -I$HOST/stub -I$CORE/Inc -idirafter $CORE/Src"
DSP="$(cat "$HOST/filelist.txt") $(cat "$HOST/filelist_extra.txt" 2>/dev/null || true) Src/Preset/initKit.c"
UI="Src/menu/menu.c Src/menu/modelPresets.c Src/menu/Cc2Text.c Src/menu/copyClearTools.c Src/Soundengine/Sequencer/EuklidGenerator.c
    Src/Soundengine/MIDI/CortexParser.c Src/frontPanelParser.c Src/ledHandler.c Src/Parameters.c"
for f in $DSP; do
  [ -f "$CORE/$f" ] || continue
  gcc $CFLAGS -c "$CORE/$f" -o "$OBJ/$(basename "${f%.c}").o"
done
for f in $UI; do
  [ -f "$CORE/$f" ] || { echo "missing $f"; exit 1; }
  gcc $CFLAGS -c "$CORE/$f" -o "$OBJ/$(basename "${f%.c}").o"
done
gcc $CFLAGS -c "$HERE/plock_seq_access.c" -o "$OBJ/plock_seq_access.o"
gcc $CFLAGS -c "$HERE/plock_test.c" -o "$OBJ/plock_test.o"
gcc $CFLAGS -c "$HOST/src/ui_stubs.c" -o "$OBJ/ui_stubs.o"
gcc -fcommon "$OBJ"/*.o -lm -o "$OUT"
echo "built $OUT"
