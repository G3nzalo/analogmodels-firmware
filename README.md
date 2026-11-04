# AnalogModels: custom firmware for the Erica Synths LXR-02

This is a **modified version** of the LXR-02 desktop firmware 1.7 published by Erica Synths. It is
**not an official Erica Synths release** and Erica Synths provides no support for it. The boot
screen shows `AnalogModels 0.3` / `custom FW V 1.7`, and the update file is
`LXR_AnalogModels_V0.3.img`.

## What it adds

### The instruments of the TR-808 and TR-909

25 instruments, modelled after the circuits of the two drum machines, appear in every **Waveform**
list after `Sin Tri Saw Rec Noi PWM`. Any of the six voices can play any of them.

| Waveform | Instrument | | Waveform | Instrument |
|---|---|---|---|---|
| `BD8` | 808 bass drum | | `RS8` | 808 rim shot |
| `BD9` | 909 bass drum | | `RS9` | 909 rim shot |
| `SD8` | 808 snare | | `CL8` | 808 claves |
| `SD9` | 909 snare | | `CP8` | 808 hand clap |
| `LT8` `MT8` `HT8` | 808 low, mid, high tom | | `CP9` | 909 hand clap |
| `LT9` `MT9` `HT9` | 909 low, mid, high tom | | `MA8` | 808 maracas |
| `LC8` `MC8` `HC8` | 808 low, mid, high conga | | `CB8` | 808 cowbell |
| `HH8` | 808 hi-hat | | `CY8` | 808 cymbal |
| `HH9` | 909 hi-hat | | `CR9` `RD9` | 909 crash, ride |

Each instrument makes its own pitch sweeps, clicks, noise and envelopes: the bridged-T resonators
of the 808 kick, snare, toms, congas, rim shot and claves; the 909 kick's triangle VCO with its fast
pitch sweep and attack click; the six square oscillators of the 808 metal through the band passes
of the cowbell, hats and cymbal (low, mid and high band with their own decays); the clap's noise
bursts and reverb tail; the 909 hats and cymbals (samples in the original) as dense ring-modulated
metal and noise, the hats at 6 bit like the original sample ROM.

**Choosing an instrument sets up its voice**, so it plays like the original right away: coarse and
fine at 0 (the original tuning), the amp envelope as a gate (attack 0, slope at the end) with the
instrument's own decay, filter, pitch EG, FM, transient, drive and saturator off. On the snare voice
the voice's own noise is turned off (only the instrument plays; turn `Mix` up to add it), on the hat
voice the open hat gets its decay too. This does not happen while a step is held (the waveform
becomes a lock of that step) or while automation is recorded.

On an instrument:

- **Decay** sets its length: the instrument has faded out when the amp gate closes. **Coarse** and
  **Fine** tune it (0 = original tuning).
- **PWM** is a **tube saturator** (the menu shows `sat`, long name `TubeSat`): an asymmetric
  triode-style curve with a low shelf in front of it for body, so it adds even and odd harmonics
  and makes the tail last longer. 0 = off.
- **Velocity** acts like the accent: harder hits get more click, snap and brightness. With the
  velocity setting `vol` on (on the page of the pitch EG), it also sets the level.
- Filter, drive, LFOs, the FX and the other voice settings work as with the original waveforms.
- A step can play another instrument by locking its **Waveform** (and its decay), see below.

Every hit gets a tiny random pitch drift (about 2 cent) like analog circuits, a retrigger fades the
previous hit out over 1.5 ms instead of clicking, and a soft limiter keeps overlapping hits and
extreme settings from clipping hard. The ring mod FX keeps the six original waveforms.

Nothing changes for kits that only use the original waveforms: rendering the voices with the
original and the modified code on a PC gives bit-identical audio. A kit that uses an instrument
plays that oscillator silent on the official firmware.

**Kits saved with version 0.2** that use its models keep the waveform numbers, but the list
changed: `808`, `909` and `Snr` are now `BD8`, `BD9` and `SD8`; `Clp`, `Rim`, `Mtl`, `Bel`, `Cym` and
`Rid` became `SD9`, `LT8`, `MT8`, `HT8`, `LT9` and `MT9`. Choose the instrument again for those voices.

The instruments run instead of the oscillator they replace. Counted on a Cortex-M7 in QEMU
(`tools/m7_bench`), a drum voice takes 11k to 15k instructions per block with most instruments
(13k to 15k with the original waveforms), 20k with `RD9`, 25k with `HH8` and 30k with `CY8`; on
the cymbal and hat voices about 5k less, as their FM oscillators rest. A whole audio block with the
example kits takes 87k to 104k instructions (88k with the init kit), the worst case (`CY8` on all
six voices with the compressor FX) 194k. The budget is about 313k cycles per block. The code is in
`Core/Src/Soundengine/DSPAudio/AnalogModels.c`, the voice settings in `Core/Src/menu/modelPresets.c`.

### Up to 8 parameter locks per step

Select the voice and a page (for example the filter page), hold a step button, turn knobs and
release the step: that step then plays with those values and the next step without locks plays the
kit's sound again. A step now keeps up to **eight** locked parameters (firmware 1.7: two, a third
replaced one of them). Turning a ninth knob replaces the step's oldest lock; turning a locked knob
again changes its lock. Locking the **Waveform** (and the **Decay**) plays another instrument on
that step, for example `CY8` on the last step of a clap track.

- While holding a step, **click the encoder** to clear all locks of that step.
- Lock 1 and 2 are the ones the step page shows (`p1d`/`p1v`, `p2d`/`p2v`); the others are not shown
  there.
- Each pattern has room for 192 locks beyond the first two of each step, shared by its tracks.
  When they are all used, a new parameter replaces one of the step's locks.
- Recording automation while playing also fills free locks, so several knobs can be recorded on the
  same steps. Only when a step has all eight it replaces the lock of the selected automation track,
  as before.
- `clear [autom.1]` clears lock 1 of the track, `clear [autom.2]` lock 2 and all further locks.
  Clearing a track or pattern, copying a track or pattern and filling the next bar with the copy
  button held take all locks along.
- Pattern files keep the locks beyond the first two after the data of the original format, so the
  official firmware still loads them (with two locks per step) and patterns saved with the official
  firmware load unchanged.

Version 0.2 fixed the problems that made the locks unreliable on firmware 1.7: a third parameter on
a held step erased lock 1 or 2 on every step of track 1 or 2, holding a step showed and restored
the locks of track 1 instead of the voice being edited, locks of parameters above number 127 were
restored on the wrong parameter, the locks of the previously held step were mixed into the next
one, holding a step changed the track that realtime recording writes into, a step with only lock 2
got a second lock for the same parameter, and a knob without a parameter used up a lock.

The lock handling is checked by PC tests that run the real menu, button, sequencer, sound engine
and preset manager code (`tools/host_render/test`: 76 checks for holding steps, playing and
recording locks, 36 for saving and loading patterns, including files of the original firmware).

## Example kits and patterns

The SD-card project `proj60` ("AnalogMd") has these kits in kit slots 0 to 4, each with a demo
pattern in the same pattern slot:

| Slot | Kit | Contents |
|---|---|---|
| 0 | `TR808` | `BD8`, `MT8`, `RS8`, `SD8`, `CP8` and `HH8`; drum 3 plays a cowbell and the clap voice the cymbal on one step each (waveform locks) |
| 1 | `TR909` | `BD9`, `CP9`, `RS9`, `SD9`, `RD9` and `HH9`; a tom fill from drum 3 (`LT9`, `MT9`, `HT9` by locks) and a rising snare roll |
| 2 | `Perc808` | the three congas, claves, cowbell and maracas |
| 3 | `Bateria` | `BD9`, floor tom `LT9`, ride `RD9`, `SD9`, crash `CR9` and `HH9`; the 2-bar pattern has ghost snares and a tom fill from the floor tom voice |
| 4 | `LockDemo` | a snare on drum 2: normal, filter opened a bit, longer amp decay, filter closed and short; the kick changes its sound on every beat, with 8 locks on beat 4 |

They are generated with the PC render tool in `tools/host_render`, which runs the real firmware DSP
code on a computer.

Known limitations:

- The instruments ignore FM input, and the transient page's `Ofs` mode has no effect on them (`Snp`
  and the sample clicks work).
- The step page shows only lock 1 and 2 of a step.
- Tested on an LXR-02 (installing the update, playing the instruments, locks and saturator, saving
  and loading projects) and on a PC: rendering the firmware's voice code (including a check that
  kits with the original waveforms render bit-identically), the tests above and a stress test of
  all instruments on all voice types at extreme settings.

Developed by Gonzalo with the help of Claude, Anthropic's AI assistant. Bug reports and ideas are
welcome in the issues of this repository.

## Original README

Firmware for the LXR V2 desktop drum machine: an STM32F765 (Cortex-M7, 216 MHz) running the
drum voices, FX bus, step sequencer, MIDI (DIN + USB), SD-card preset storage and the LCD/button UI.

> **Licence and trademarks:** the firmware source code is free software under the GNU GPL version 3
> ([LICENSE](LICENSE)). You're welcome to study, change and share it, for example to write your own
> updates for your LXR. The names **"Erica Synths" and "LXR"** and the **LXR hardware** are the property
> of Erica Synths and are **not** covered by this licence. Building and installing your own firmware is
> at your own risk: Erica Synths provides no support for self-built or modified firmware. See
> [Licence, trademarks and hardware](#licence-trademarks-and-hardware) below and [NOTICE.md](NOTICE.md).

## Updating your LXR

Every LXR has a built-in updater (the bootloader) that installs new firmware from the SD card. You
don't need any extra hardware:

1. [Build the firmware](#building-the-firmware). This creates the update file `LXR_AnalogModels_V<version>.img`
   (the official firmware builds `LXR_V2_V<version>.img`).
2. [Copy it to the SD card and install it](#installing-the-update) on the LXR.

### Building the firmware

You need [CMake](https://cmake.org/download/) 3.21 or newer, [Ninja](https://ninja-build.org/),
the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
(`arm-none-eabi-gcc`, tested with 14.3.rel1 and 11.3.rel1) and [Python 3](https://www.python.org/downloads/)
(to create the `.img` update file). The build finds them on `PATH`.

**1. Install the tools (once)**

*macOS* (Apple Silicon and Intel), with [Homebrew](https://brew.sh):

```sh
brew install cmake ninja
brew install --cask gcc-arm-embedded
```

Use exactly this `--cask gcc-arm-embedded` package, the official Arm toolchain. The similarly named
`brew install arm-none-eabi-gcc` does **not** include the C library the firmware needs, and the build
fails with it. Python 3 comes with the Xcode Command Line Tools, which Homebrew installs.
If you installed the toolchain with Arm's own `.pkg` installer instead, it isn't on `PATH`: add
`-DARM_TOOLCHAIN_DIR=/Applications/ArmGNUToolchain/<version>/arm-none-eabi/bin` to the first `cmake`
command below.

*Windows:* install CMake, Ninja and Python 3 (e.g. `winget install Kitware.CMake Ninja-build.Ninja
Python.Python.3.12`) and the Arm GNU Toolchain with its Windows installer. On the installer's last page,
tick "Add path to environment variable".

*Linux:* install CMake, Ninja and Python 3 with your package manager (e.g.
`sudo apt install cmake ninja-build python3`) and the Arm GNU Toolchain from Arm's download page, then
add its `bin` folder to `PATH`.

**2. Build**

Open a terminal (Terminal on macOS, PowerShell on Windows) in this folder and run:

```sh
cmake --preset release
cmake --build --preset release
```

When the build has finished it shows where the update file is and how to install it:

```
======================================================================
  Firmware update file:
    .../lxr_desktop_app/build/release/LXR_AnalogModels_V0.3.img
  ...
```

The version in the file name and on the first line of the LXR's start-up screen is set by
`LXR_MOD_VERSION` in [CMakeLists.txt](CMakeLists.txt); `project(VERSION ...)` stays the version of the
Erica Synths firmware it is based on (second line of the start-up screen).

### Installing the update

1. Copy `LXR_AnalogModels_V<version>.img` from `build/release/` to the top folder of the SD card. Delete any
   older `LXR*.img` files from the card: the updater installs the first one it finds.
   On a Mac, eject the card in Finder before you take it out. Finder also leaves hidden files starting
   with `._` on the card; the updater ignores them.
2. Switch the LXR off and put the card in.
3. Hold down the encoder button and switch the LXR on. Keep holding the button until
   "LXR Updater" appears on the display.
4. The display shows "Checking Firmware...", then "Loading Firmware" with the progress in percent.
5. When it shows "Update OK!" the LXR restarts by itself with the new firmware.

The updater itself is never changed by an update. If an update fails or is interrupted (for example by
a power cut), the LXR can always be updated again the same way.

### Troubleshooting

If the updater stops with one of these messages, switch the LXR off, fix the cause and try again:

| Display | Cause |
|---|---|
| `can't mount` / `SD Card` | No SD card, or the card can't be read. The card must be formatted as FAT32. Cards larger than 32 GB usually come formatted as exFAT, which the updater can't read. On a Mac, reformat the card in Disk Utility with the format "MS-DOS (FAT)". |
| `LXRE*.img` / `Not Found!` | No update file in the top folder of the card. The file name must start with `LXR` and end with `.img`. |
| `Can't open` / `LXRE*.img!` | The update file couldn't be read. Copy it to the card again. |
| `LXR*.img` / `Not Valid!` | The file isn't an update for the desktop LXR (for example a Eurorack module update, or a damaged file). |
| `LXR*.img` / `Wrong Size!`, `Wrong` / `Checksum!` | The file is damaged or incomplete. Build it again or copy it to the card again. |
| `Can't erase` / `Flash!`, `Flash write` / `Error!` | Writing the firmware to the LXR's memory failed. Switch the LXR off and try the update again. |

## Build options

| Option | Effect |
|---|---|
| `cmake --preset debug` | Debug build: `-O3 -g3` with `DEBUG` defined, output in `build/debug/`. The release build uses `-O3 -g0`. Both need `-O3` for the audio engine to keep up in real time. |
| `-DARM_TOOLCHAIN_DIR=<bin folder>` | Use a toolchain that isn't on `PATH`, e.g. `/Applications/ArmGNUToolchain/14.3.rel1/arm-none-eabi/bin` (macOS) or `C:/toolchain_arm-none-eabi/11.3rel1/bin` (Windows) |
| `-DLXR_ITCM_OPT=<flag>` | Optimisation for the code in the fast ITCM memory, see [Memory map](#memory-map) (default `-O2`) |
| `-DLXR_GENERATE_LISTING=ON` | Also write a disassembly listing (`.list`) |

Options are passed when configuring, e.g. `cmake --preset release -DLXR_GENERATE_LISTING=ON`.
The toolchain file and the CMake modules and tools are in the `cmake/` folder.

All files the build creates (in `build/<preset>/`):

| File                     | Purpose                                                        |
|--------------------------|----------------------------------------------------------------|
| `LXR_AnalogModels_V<version>.img` | **Update file for the SD card** (see [Installing the update](#installing-the-update)) |
| `LXR_V2_STM32F765.elf`   | Firmware with debug information, for a debugger               |
| `LXR_V2_STM32F765.bin`   | Raw firmware image (from 0x08008000)                          |
| `LXR_V2_STM32F765.hex`   | Intel HEX of the same                                         |
| `LXR_V2_STM32F765.map`   | Linker map                                                    |

## Cleaning the build

**Remove the compiled files** (including the `.img`) but keep the settings. The next build then
compiles everything again:

```sh
cmake --build --preset release --target clean
```

Use `--preset debug` to clean the debug build.

**Start completely from scratch:** delete the build folder, which also forgets all settings such as
`-DARM_TOOLCHAIN_DIR` or `-DLXR_BOOTLOADER_HEX`, then configure and build again:

```sh
rm -rf build                        # Linux, macOS, Git Bash
Remove-Item -Recurse -Force build   # Windows PowerShell
rmdir /s /q build                   # Windows Command Prompt

cmake --preset release
cmake --build --preset release
```

Start from scratch when you switch to a different compiler (another `ARM_TOOLCHAIN_DIR`, or a
different `arm-none-eabi-gcc` on `PATH`), because an existing build folder keeps using the compiler it
was first set up with. Do the same if the build behaves strangely after updating the sources.
With CMake 3.24 or newer, `cmake --preset release --fresh` redoes the setup without deleting the folder.

The `build/` folder is never committed to git.

## Memory map

| Region   | Address                   | Size   | Contents                                              |
|----------|---------------------------|--------|-------------------------------------------------------|
| FLASH    | 0x08000000 – 0x08007FFF   | 32 KB  | Bootloader (`LXR_V2_STM32F765_Bootloader`)            |
| FLASH    | 0x08008000 –              | 992 KB | This firmware (`VECT_TAB_OFFSET` 0x8000)              |
| ITCM     | 0x00000000                | 16 KB  | `IN_ITCM_RAM` hot DSP code, copied there by `main()`  |
| DTCM     | 0x20000000                | 128 KB | `INDTCM`/`INDTCMZ` data, DMA buffers, the stack       |
| RAM      | 0x20020000                | 384 KB | `.data`, `.bss`, heap                                 |

The ITCM is tight. Current GCC releases generate too much code at `-O3` for it, so the translation
units that contain ITCM code are compiled with `LXR_ITCM_OPT` (default `-O2`). The linker prints
region usage after every build.

## Source layout and CMake targets

The project was originally generated with STM32CubeMX/STM32CubeIDE, which explains the directory
layout and the `USER CODE BEGIN/END` comments. It no longer depends on either tool: the CubeMX project
(`.ioc`) was removed because regenerating code from it overwrites application code in `main.c`.
Pins, clocks and peripherals are configured in `Core/Inc/main.h`, `Core/Src/main.c`
(`SystemClock_Config`, `MX_*_Init`) and `Core/Src/stm32f7xx_hal_msp.c`; edit them there.

Every component is an OBJECT library linked into `LXR_V2_STM32F765`:

| Target          | Sources                                         | Contents                                              |
|-----------------|-------------------------------------------------|-------------------------------------------------------|
| `stm32f7_hal`   | `Drivers/`                                      | ST HAL + CMSIS                                         |
| `fatfs`         | `Middlewares/Third_Party/FatFs`, `FATFS/`       | FatFs and the SD card glue                            |
| `usb_midi`      | `Middlewares/JS`, `USB_DEVICE_MIDI/`            | USB device stack with the custom USB-MIDI class       |
| `lxr_platform`  | `Core/Startup`, `Core/Src/main.c`, ...          | Startup, clocks, peripheral init, interrupts          |
| `lxr_io`        | `Core/Src/IO`                                   | Button/LED shift registers, pots and faders           |
| `lxr_ui`        | `Core/Src/menu`, `lcd.c`, `buttonHandler.c`, ...| LCD, encoder, buttons, LEDs, menu pages               |
| `lxr_preset`    | `Core/Src/Preset`                               | Kit/pattern/project load & save on SD                 |
| `lxr_dsp`       | `Core/Src/Soundengine/DSPAudio`                 | Oscillators, filters, envelopes, voices, master FX    |
| `lxr_engine`    | `Core/Src/Soundengine/*.c`                      | Codec/I2S DMA, sample block rendering, FX bus, clock in/out |
| `lxr_midi`      | `Core/Src/Soundengine/MIDI`                     | MIDI UART, parsers, parameter mapping                 |
| `lxr_sequencer` | `Core/Src/Soundengine/Sequencer`                | Step sequencer, Euclidean generator, clock sync       |
| `lxr_usb_app`   | `Core/Src/Soundengine/USB`                      | USB-MIDI message queue                                |

Source files are listed explicitly in the `CMakeLists.txt` files. Add new files there.

## Advanced: programming with a debug probe

Not needed for updates: use the [SD card](#installing-the-update) for that. This section is for
development, factory programming and repairs, with an ST-Link debug probe connected to the LXR's
debug header and [OpenOCD](https://openocd.org/) installed (the build finds it on `PATH`; on macOS:
`brew install open-ocd`).

**Firmware only.** Writes just the firmware; the bootloader is left alone:

```sh
cmake --build --preset release --target flash
```

**Blank devices (bootloader + firmware).** A blank device, or replacing the bootloader on purpose,
needs the bootloader as well. The bootloader is a separate project, so by default the build doesn't
include it. Tell the build where to find it in one of two ways:

```sh
# A: a prebuilt bootloader .hex (e.g. from a bootloader release)
cmake --preset release -DLXR_BOOTLOADER_HEX=path/to/LXR_V2_STM32F765_Bootloader.hex

# B: the bootloader source code, which is then built together with the firmware
cmake --preset release -DLXR_BOOTLOADER_DIR=path/to/LXR_V2_STM32F765_Bootloader
```

The build checks that the bootloader is valid for this device: it must fit in the 32 KB bootloader
sector and accept desktop (`LXRV2IMG`) update images, so the firmware, a full image or the Eurorack
bootloader are rejected. It then also writes `LXR_V2_V<version>_full.hex`/`.bin` (bootloader +
firmware, the whole flash from 0x08000000) and enables:

```sh
cmake --build --preset release --target flash_full
```

The same `.hex`/`.bin` can also be loaded with STM32CubeProgrammer (the `.bin` goes to 0x08000000).
To go back to a firmware-only build, clear the setting: `-DLXR_BOOTLOADER_HEX=` or `-DLXR_BOOTLOADER_DIR=`.

> **Warning:** `flash_full` overwrites the bootloader. If that is interrupted, the device can no longer
> update itself from the SD card and has to be recovered with a debug probe. The target therefore
> prints a warning and only continues after you type `yes`; without an interactive terminal (e.g. a
> build started from an IDE) it refuses. For scripts and factory programming, set the environment
> variable `LXR_CONFIRM_BOOTLOADER_OVERWRITE=yes` to skip the question.

The bootloader write-protects its own flash sector (sector 0) every time it boots, so on a unit that
has run before, sector 0 cannot simply be erased. `flash_full` therefore clears that protection first;
the new bootloader sets it again on its first boot. Other programming tools need the same step
(in STM32CubeProgrammer: Option bytes → Write protection → uncheck nWRP0).

The OpenOCD settings can be changed with `LXR_OPENOCD_INTERFACE`, `LXR_OPENOCD_TRANSPORT`,
`LXR_OPENOCD_SPEED` (default 1000 kHz) and `LXR_OPENOCD_WORKAREA` (default 0x8000). The defaults are
deliberately conservative: with a 64 KB work area at the faster default clock, an ST-Link V2 (firmware
V2J29) failed half-way through programming.

## Licence, trademarks and hardware

Copyright (C) 2012-2026 Erica Synths.

**Firmware licence.** The firmware, the bootloader and the build system are licensed under the
[GNU General Public License version 3](LICENSE) (GPL-3.0-only), with an additional permission for
the STMicroelectronics libraries and the additional terms in [NOTICE.md](NOTICE.md). In short:

- You may use, study and change the firmware, for example to build your own updates for your device.
- If you share or distribute the firmware, or a modified version of it, including pre-installed on a
  device, you must provide the complete source code under the same licence, and the people you give it
  to may install their own modified versions.
- Modified firmware must be marked as modified (its start-up screen must show this, e.g. with a
  different name or version text) and must not be presented as official Erica Synths firmware.
- The firmware comes **without any warranty**. Building and installing your own or modified firmware is
  entirely at your own risk, and Erica Synths provides no support for it.

**Trademarks.** "Erica Synths", "LXR" and "LXR-02" and the associated logos are trademarks of
Erica Synths. The GPL licence does **not** grant any right to use them. They may not be used for any
product, firmware release, service or commercial activity without prior written permission from
Erica Synths. Truthful references such as "custom firmware for the Erica Synths LXR-02" are fine.

**Hardware.** The LXR hardware (schematics, circuit board layouts, front panel, enclosure and industrial
design) is the intellectual property of Erica Synths and is protected by copyright and design rights.
It is **not** part of this repository and is **not** licensed under the GPL or any other licence. It may
not be copied, manufactured or sold, in whole or in part, without prior written permission from
Erica Synths.

**Third-party code.** The repository includes components from STMicroelectronics, Arm and ChaN (FatFs)
under their own licences. See [NOTICE.md](NOTICE.md#third-party-components).


<sup>
Use code OPENLXR15 on the Erica Synths website for a discount on the LXR (active for 1 week since open-source announcment) :)
</sup>