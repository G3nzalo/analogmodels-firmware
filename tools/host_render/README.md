# PC render tool for the analog models

Renders the LXR-02 drum voices on a PC with the real firmware DSP and MIDI-parameter code
(`Core/Src/Soundengine/...`), so sounds and kits can be checked before flashing. Needs gcc and,
for the kit scripts, Python 3 (numpy, scipy and matplotlib for `analyze.py`). Not part of the
firmware build.

```sh
./build_host.sh                      # builds ./render from ../../Core
mkdir -p out && python3 kits.py out  # render scripts for the 808, 909, PERC, BAT and LOCK kits
for k in 808 909 PERC BAT LOCK; do ./render out/$k.txt; done   # one-shots, grooves, .SND kit files
python3 gen_project.py out sd 60     # SD-card project folder sd/proj60 with the kits and patterns
python3 analyze.py plot.png out/808_d1.wav   # waveform, envelope, spectrogram, decay and pitch
```

Render script commands are listed at the top of `src/host_render.c`. `preset <voice> <wave>`
chooses a waveform like the menu does, so an 808/909 instrument brings its voice settings. A `hit`
can carry up to eight parameter locks (`hit 1.5 3 60 100 PAR_FILTER_FREQ_4 95 PAR_VELOD4 40`),
played the way the sequencer plays a step with locks; `kits.py` grooves use the same locks and
`gen_project.py` writes them into the pattern files (lock 1 and 2 in the step, the others in the
pattern's lock list behind the original data). `stub/` replaces the
STM32 HAL with just enough for the DSP code to compile on a PC; the PC runs with flush-to-zero
like the STM32. `src/param_names.h` is generated from `Core/Inc/Parameters.h` by `gen_param_names.py`.

`test/` has two tests. `plock_test` checks the step parameter locks (hold a step, turn knobs,
release, play; up to eight locks per step, a full lock pool, recording while playing), choosing
an instrument, and the scale and voice FX knobs (no locks). It builds the real menu, button handler, CortexParser, sequencer and sound
engine code with the hardware stubbed out (`src/ui_stubs.c`). `patfile_test` saves and loads
patterns with the real `presetManager.c` on a folder of the PC (`test/ff_stdio.c` stands in for
FatFs): pattern files and `tmp.pat` with up to eight locks per step, files of the original
firmware, a broken lock list, copy and clear; kit files and `tmp.snd` with the kit extension
(per-voice scale and voice FX), old and damaged ones. Built against the original firmware's `Core/` it
writes the same pattern, so the files can be compared (the new ones only add the lock list):

```sh
test/build_plock.sh && test/plock_test           # prints each check, exit code 0 when all pass
test/build_patfile.sh && mkdir -p sd && test/patfile_test sd
```
