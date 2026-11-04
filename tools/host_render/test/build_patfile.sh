#!/bin/bash
# Builds the pattern file test (PC only) from a firmware Core/ directory: the
# real preset manager, sequencer and menu code, with the SD card on a folder of
# the PC (ff_stdio.c). Also builds against the original firmware's Core/.
# usage: ./build_patfile.sh [path to Core] [output binary]
# SPDX-License-Identifier: GPL-3.0-only
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; HOST="$HERE/.."
CORE="$(cd "${1:-$HERE/../../../Core}" && pwd)"; OUT="${2:-$HERE/patfile_test}"
OBJ="$HERE/obj_$(basename "$OUT")"; rm -rf "$OBJ"; mkdir -p "$OBJ"
CFLAGS="-std=gnu11 -O1 -g -w -fcommon -I$HOST/stub -I$CORE/Inc -idirafter $CORE/Src"
DSP="$(cat "$HOST/filelist.txt") $(cat "$HOST/filelist_extra.txt" 2>/dev/null || true) Src/Preset/initKit.c"
UI="Src/menu/menu.c Src/menu/modelPresets.c Src/menu/Cc2Text.c Src/menu/copyClearTools.c Src/Soundengine/Sequencer/EuklidGenerator.c
    Src/Soundengine/MIDI/CortexParser.c Src/frontPanelParser.c Src/ledHandler.c Src/Parameters.c Src/buttonHandler.c
    Src/Preset/presetManager.c"
for f in $DSP $UI; do
  [ -f "$CORE/$f" ] || continue
  gcc $CFLAGS -c "$CORE/$f" -o "$OBJ/$(basename "${f%.c}").o"
done
gcc $CFLAGS -c "$HERE/plock_seq_access.c" -o "$OBJ/plock_seq_access.o"
gcc $CFLAGS -c "$HERE/patfile_test.c" -o "$OBJ/patfile_test.o"
gcc $CFLAGS -c "$HERE/ff_stdio.c" -o "$OBJ/ff_stdio.o"
gcc $CFLAGS -c "$HERE/patfile_stubs.c" -o "$OBJ/patfile_stubs.o"
gcc -fcommon "$OBJ"/*.o -lm -o "$OUT"
echo "built $OUT"
