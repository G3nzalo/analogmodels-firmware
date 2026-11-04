#!/bin/bash
# Builds bench.elf: the firmware's voice, mixer and FX code for the Cortex-M7,
# to count instructions per audio block in QEMU. Needs arm-none-eabi-gcc,
# python3 and qemu-system-arm.
# usage: ./build.sh [path to Core] [folder with .SND kit files]
# run:   qemu-system-arm -M mps2-an500 -nographic -semihosting -icount shift=0 -kernel bench.elf
# SPDX-License-Identifier: GPL-3.0-only
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
CORE="$(cd "${1:-$HERE/../../Core}" && pwd)"; FW="$(cd "$CORE/.." && pwd)"
KITS="${2:-$HERE/../host_render/out}"
OBJ="$HERE/obj"; rm -rf "$OBJ"; mkdir -p "$OBJ"
ARCH="-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard"
# the firmware's DSP flags (see build/compile_commands.json)
CFLAGS="-O3 -g0 -std=gnu11 -w $ARCH -ffunction-sections -fdata-sections -ffast-math -freciprocal-math
  -mfp16-format=ieee -fomit-frame-pointer -DSTM32F765xx -DUSE_HAL_DRIVER -I$CORE/Inc
  -I$FW/Drivers/CMSIS/Include -I$FW/Drivers/CMSIS/Device/ST/STM32F7xx/Include
  -I$FW/Drivers/STM32F7xx_HAL_Driver/Inc -I$FW/Middlewares/Third_Party/FatFs/src -I$FW/FATFS/App
  -I$FW/FATFS/Target -I$FW/Middlewares/JS/STM32_USB_Device_Library/Core/Inc
  -I$FW/Middlewares/JS/STM32_USB_Device_Library/Class/CDC/Inc -I$FW/USB_DEVICE_MIDI/App
  -I$FW/USB_DEVICE_MIDI/Target -I$OBJ"
python3 "$HERE/gen_kits.py" "$KITS" "$OBJ/kits_bin.h"
# the mixer reads the output jack switches from the STM32 GPIOs, which QEMU's board does not have
tr -d '\r' < "$CORE/Src/Soundengine/DSPAudio/mixer.c" |
  sed -E 's/^(\s*)(r2|l1|l2|r1)_Available = \(switch_.*$/\1\2_Available = 1;/' > "$OBJ/mixer_bench.c"
FILES="$(cat "$HERE/../host_render/filelist.txt" "$HERE/../host_render/filelist_extra.txt") Src/Preset/initKit.c"
for f in $FILES; do
  arm-none-eabi-gcc $CFLAGS -c "$CORE/$f" -o "$OBJ/$(basename "${f%.c}").o"
done
for f in "$HERE/bench.c" "$HERE/vectors.c" "$HERE/stubs.c" "$OBJ/mixer_bench.c"; do
  arm-none-eabi-gcc $CFLAGS -c "$f" -o "$OBJ/$(basename "${f%.c}").o"
done
LIBDIR="$(dirname "$(arm-none-eabi-gcc $ARCH -print-file-name=librdimon.a)")"
arm-none-eabi-gcc $ARCH --specs=rdimon.specs -T "$HERE/link.ld" -nostartfiles -Wl,--gc-sections \
  "$OBJ"/*.o "$LIBDIR/rdimon-crt0.o" -lc -lrdimon -lm -lgcc -o "$HERE/bench.elf"
echo "built $HERE/bench.elf"
