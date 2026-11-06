# Cortex-M7 cost estimate of the voices

Compiles the firmware's voice, mixer and FX code with the firmware's compiler flags for the
Cortex-M7 and runs it in QEMU's `mps2-an500` board (a Cortex-M7) with `-icount shift=0`, where
every instruction takes the same virtual time. It prints the instructions per 64-sample audio
block for single voices with the original waveforms, for each instrument (with its voice
settings) on a drum, snare, cymbal and hat voice, for all six voices with the example kits, and
for the whole mixer block with the most expensive instrument on all six voices and each FX type,
also with each voice FX on all six voices and their sends to the delay. QEMU has no caches, flash wait states or dual
issue, so these are instruction counts, not cycles; compare them with each other and with the
budget of 216 MHz x 64 / 44140 Hz = 313,185 cycles per block. Not part of the firmware.

```sh
(cd ../host_render && ./build_host.sh && mkdir -p out && python3 kits.py out &&
 for k in 808 909 PERC BAT LOCK; do ./render out/$k.txt; done)   # the example .SND kits
./build.sh
qemu-system-arm -M mps2-an500 -nographic -semihosting -icount shift=0 -kernel bench.elf
```

Needs `arm-none-eabi-gcc` (with newlib's semihosting library `librdimon`), Python 3 and
`qemu-system-arm`.
