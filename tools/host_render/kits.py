#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Kit definitions for the instruments, plus render-script generation.

usage: kits.py <outdir> [kit ...]
Writes <outdir>/<kit>.txt render scripts (one-shots, velocity steps and a demo
groove) that also save the kit file with 'savekit'. Each voice gets its
instrument like choosing it in the menu ('preset': the instrument brings its
voice settings), then the kit's own parameter values are set on top. Values
are the 0..127 values the LXR stores in its kit files.
Groove hits are (voice, step, velocity) or (voice, step, velocity, locks) with
up to 8 (parameter, value) step locks, like holding a step and turning knobs,
or (voice, step, velocity, locks, note) with the step's note (note()).
"""
import os
import sys

# the waveform numbers of the instruments (AnalogModels.h)
INSTRUMENTS = ["BD8", "BD9", "SD8", "SD9", "LT8", "MT8", "HT8", "LT9", "MT9", "HT9",
               "LC8", "MC8", "HC8", "RS8", "RS9", "CL8", "CP8", "CP9", "MA8", "CB8",
               "HH8", "HH9", "CY8", "CR9", "RD9",
               "SRN", "GND", "BNG", "GNG", "DJB", "DJT", "DJS", "CGO", "CGS",
               "ODK", "NGD", "SHM", "TKR"]
W = {name: 6 + i for i, name in enumerate(INSTRUMENTS)}

# the decay each instrument brings (menu/modelPresets.c), for waveform locks
DECAY = {"BD8": 65, "BD9": 47, "SD8": 26, "SD9": 32, "LT8": 44, "MT8": 37, "HT8": 32,
         "LT9": 51, "MT9": 46, "HT9": 41, "LC8": 32, "MC8": 28, "HC8": 24, "RS8": 6,
         "RS9": 8, "CL8": 8, "CP8": 29, "CP9": 35, "MA8": 12, "CB8": 31, "HH8": 7,
         "HH9": 8, "CY8": 79, "CR9": 87, "RD9": 89,
         "SRN": 84, "GND": 93, "BNG": 74, "GNG": 120, "DJB": 37, "DJT": 41, "DJS": 25,
         "CGO": 49, "CGS": 22, "ODK": 80, "NGD": 58, "SHM": 25, "TKR": 10}

# per voice 0..5: waveform, decay, coarse, saturator (PWM), volume, pan parameters
WAVE = ["PAR_OSC_WAVE_DRUM1", "PAR_OSC_WAVE_DRUM2", "PAR_OSC_WAVE_DRUM3", "PAR_OSC_WAVE_SNARE",
        "PAR_WAVE1_CYM", "PAR_WAVE1_HH"]
DEC = ["PAR_VELOD1", "PAR_VELOD2", "PAR_VELOD3", "PAR_VELOD4", "PAR_VELOD5", "PAR_VELOD6_CLOSED"]
COA = ["PAR_COARSE1", "PAR_COARSE2", "PAR_COARSE3", "PAR_COARSE4", "PAR_COARSE5", "PAR_COARSE6"]
SAT = ["PAR_PWM_1", "PAR_PWM_2", "PAR_PWM_3", "PAR_PWM_4", "PAR_PWM_5", "PAR_PWM_6"]
VOL = ["PAR_VOL1", "PAR_VOL2", "PAR_VOL3", "PAR_VOL4", "PAR_VOL5", "PAR_VOL6"]
PAN = ["PAR_PAN1", "PAR_PAN2", "PAR_PAN3", "PAR_PAN4", "PAR_PAN5", "PAR_PAN6"]
VELO = ["PAR_VOLUME_MOD_ON_OFF%d" % (v + 1) for v in range(6)]


def other(voice, inst, coarse=None):
    """locks that play another instrument on one step: its waveform and decay"""
    locks = [(WAVE[voice], W[inst]), (DEC[voice], DECAY[inst])]
    if coarse is not None:
        locks.append((COA[voice], coarse))
    return tuple(locks)


def kit(name, bpm, instruments, mix, extra=None, steps=16, groove=(), gain=1.0, fxbus=False):
    """instruments: the instrument of each of the 6 voices. mix: (volume, pan) per voice,
    gain scales all volumes (headroom for the groove). fxbus: render the kit's FX too
    (for the voice FX sends)."""
    params = {}
    for v in range(6):
        vol, params[PAN[v]] = mix[v]
        params[VOL[v]] = int(round(vol * gain))
        params[VELO[v]] = 1          # the step velocity is the accent
    params.update(extra or {})
    return {"name": name, "bpm": bpm, "instruments": instruments, "params": params, "steps": steps,
            "groove": list(groove), "fxbus": fxbus}


def note(voice, step, vel, n, locks=()):
    """a groove hit with a step note (60 is the original tuning; with a gamelan scale each
    note up is the next tone of the scale)"""
    return (voice, step, vel, tuple(locks), n)


# voice index: 0-2 drum 1-3, 3 snare, 4 cymbal, 5 closed hat, 6 open hat
KITS = {}

# --- TR-808: kick, tom, rim shot, snare, clap, hats. The clap voice plays the
# cymbal on the last step and drum 3 a cowbell, by locking waveform and decay.
KITS["808"] = kit(
    "TR808", 98, ["BD8", "MT8", "RS8", "SD8", "CP8", "HH8"],
    mix=[(127, 63), (112, 45), (100, 85), (118, 63), (112, 72), (104, 40)],
    extra={"PAR_PWM_1": 12, "PAR_VELOD6_OPEN": 44},
    groove=[(0, 0, 127), (0, 6, 90), (0, 10, 127),
            (3, 4, 120), (3, 12, 120), (4, 12, 100),
            (2, 3, 90), (2, 9, 70), (2, 7, 100, other(2, "CB8")),
            (1, 15, 110), (4, 15, 110, other(4, "CY8"))]
           + [(5, s, 110 if s % 4 == 0 else 70) for s in range(0, 16, 2) if s != 14] + [(6, 14, 100)])

# --- TR-909: kick, clap, rim shot, snare, ride, hats. Drum 3 plays the tom
# fill (low, mid and high tom by waveform locks), the snare a roll that rises.
KITS["909"] = kit(
    "TR909", 126, ["BD9", "CP9", "RS9", "SD9", "RD9", "HH9"],
    mix=[(127, 63), (110, 70), (92, 85), (112, 55), (88, 30), (100, 45)], gain=0.8,
    extra={"PAR_PWM_1": 20, "PAR_VELOD6_OPEN": 55},
    groove=[(0, s, 127) for s in (0, 4, 8, 12)]
           + [(1, s, 115) for s in (4, 12)]
           + [(6, s, 95) for s in (2, 6, 10, 14)]
           + [(5, s, 70) for s in (1, 3, 5, 7, 9, 11, 13, 15)]
           + [(4, s, 80 if s % 4 else 105) for s in range(0, 16, 2)]
           + [(2, 7, 80), (3, 10, 45)]
           + [(3, 13, 80, (("PAR_COARSE4", 62), ("PAR_VELOD4", 26))),
              (3, 14, 95, (("PAR_COARSE4", 64), ("PAR_VELOD4", 22))),
              (3, 15, 115, (("PAR_COARSE4", 66), ("PAR_VELOD4", 18), ("PAR_PWM_4", 50)))]
           + [(2, 11, 110, other(2, "LT9")), (2, 13, 110, other(2, "MT9")), (2, 15, 115, other(2, "HT9"))])

# --- 808 percussion: congas, claves, cowbell, maracas
KITS["PERC"] = kit(
    "Perc808", 110, ["LC8", "MC8", "HC8", "CL8", "CB8", "MA8"],
    mix=[(120, 40), (116, 63), (112, 86), (96, 95), (100, 70), (98, 50)],
    extra={"PAR_VELOD6_OPEN": 20},
    groove=[(5, s, 100 if s % 4 == 2 else 60) for s in range(16)]
           + [(3, s, 100) for s in (0, 3, 6, 10, 12)]
           + [(2, s, 110) for s in (6, 7, 14)] + [(1, s, 100) for s in (2, 10)] + [(0, s, 115) for s in (4, 12, 15)]
           + [(4, s, 90) for s in (0, 4, 8, 12)] + [(4, 10, 70)])

# --- Bateria: a drum kit. Kick, floor tom, ride, snare, crash, hats; the
# higher toms of the fill come from the floor tom voice (waveform locks), the
# snare has ghost notes (darker and shorter by locks).
GHOST = (("PAR_VELOD4", 18), ("PAR_COARSE4", 58))
KITS["BAT"] = kit(
    "Bateria", 100, ["BD9", "LT9", "RD9", "SD9", "CR9", "HH9"],
    mix=[(127, 63), (116, 45), (90, 85), (118, 60), (84, 35), (104, 70)], gain=0.8,
    extra={"PAR_PWM_1": 25, "PAR_PWM_4": 20, "PAR_VELOD6_OPEN": 50},
    steps=32,
    groove=[(4, 0, 120)]
           + [(2, s, 105 if s % 4 == 0 else 75) for s in range(0, 26, 2)]
           + [(0, s, 127) for s in (0, 8, 10, 16, 22, 24)] + [(0, 30, 120)]
           + [(3, s, 120) for s in (4, 12, 20)] + [(3, s, 40, GHOST) for s in (7, 15, 23)]
           + [(5, s, 70) for s in (4, 12, 20)]
           + [(1, 26, 110, other(1, "HT9")), (1, 27, 95, other(1, "HT9")),
              (1, 28, 115, other(1, "MT9")), (1, 29, 95, other(1, "MT9")), (1, 30, 120)])

# --- LockDemo: more than two locks per step. The forum example on a snare
# (normal, filter a bit open, longer amp decay, filter closed and short), and
# a kick that changes its sound on every beat, with up to 8 locks on one step.
KITS["LOCK"] = kit(
    "LockDemo", 100, ["BD8", "SD8", "RS8", "SD9", "CP8", "HH8"],
    mix=[(127, 63), (118, 63), (100, 85), (110, 55), (110, 72), (100, 40)], gain=0.8,
    extra={"PAR_FILTER_TYPE_2": 0, "PAR_FILTER_FREQ_2": 75, "PAR_RESO_2": 10, "PAR_VELOD6_OPEN": 44},
    groove=[(1, 2, 110), (1, 6, 110, (("PAR_FILTER_FREQ_2", 100), ("PAR_RESO_2", 40), ("PAR_PWM_2", 40))),
            (1, 10, 110, (("PAR_VELOD2", 60), ("PAR_PWM_2", 60), ("PAR_COARSE2", 57))),
            (1, 14, 110, (("PAR_FILTER_FREQ_2", 50), ("PAR_VELOD2", 14), ("PAR_COARSE2", 66), ("PAR_PWM_2", 90)))]
           + [(0, 0, 127),
              (0, 4, 127, (("PAR_COARSE1", 66), ("PAR_VELOD1", 40), ("PAR_PWM_1", 70))),
              (0, 8, 127, other(0, "BD9", 60) + (("PAR_PWM_1", 30),)),
              (0, 12, 127, other(0, "LT8", 64) + (("PAR_FILTER_TYPE_1", 0), ("PAR_FILTER_FREQ_1", 75),
                                                  ("PAR_RESO_1", 70), ("PAR_PWM_1", 100), ("PAR_DRIVE1", 40)))]
           + [(5, s, 70 if s % 4 else 90) for s in range(0, 16, 2)])


# --- Gamelan in slendro: gong ageng, bonang, saron, gender, a kenong (the
# bonang an octave lower on the cymbal voice) and the kendhang (djembe tones
# on the hat voice: short "tak" closed, ringing "dhung" open). With the scale
# each note up is the next tone of slendro (60 = 1, 61 = 2, 62 = 3, 63 = 5,
# 64 = 6, 59 = the low 6). The saron plays the balungan 2 1 2 6 3 5 3 2, the
# bonang doubles it in pairs (mipil), the gender runs around it, and the gong
# ends the cycle on the last 2. A delay on the shared FX, sent from the
# bonang, saron and gender.
BALUNGAN = [61, 60, 61, 59, 62, 63, 62, 61]
KITS["GAM"] = kit(
    "Gamelan", 72, ["GNG", "BNG", "SRN", "GND", "BNG", "DJT"],
    mix=[(127, 63), (84, 40), (100, 82), (92, 52), (100, 75), (108, 63)], gain=0.68, steps=32, fxbus=True,
    extra={"PAR_SCALE1": 1, "PAR_SCALE2": 1, "PAR_SCALE3": 1, "PAR_SCALE4": 1, "PAR_SCALE5": 1,
           "PAR_COARSE5": 55, "PAR_VELOD5": 90, "PAR_VELOD6_CLOSED": 22, "PAR_VELOD6_OPEN": 50,
           "PAR_FX_TYPE": 4, "PAR_FX_ROUTING": 0, "PAR_FX_DRY_WET": 127,
           "PAR_FX_P1": 71, "PAR_FX_P2": 127, "PAR_FX_P3": 90, "PAR_FX_P4": 45,   # an eighth: 0.42 s
           "PAR_VFX_SEND2": 50, "PAR_VFX_SEND3": 35, "PAR_VFX_SEND4": 55},
    groove=[note(2, 4 * b, 112 if b % 4 == 3 else 96, n) for b, n in enumerate(BALUNGAN)]
           + [note(1, 2 * e, 84 if e % 2 else 100,
                   BALUNGAN[4 * (e // 8) + 2 * ((e % 8) // 4) + (e % 2)] + 5) for e in range(16)]
           + [note(3, 4 * b + i, 88 if i == 0 else 66, BALUNGAN[b] + d) for b in range(8) for i, d in enumerate((0, 2, 1, 3))]
           + [note(4, 12, 100, 59), note(4, 28, 108, 61), note(0, 28, 122, 61)]
           + [note(6, st, 92, 58) for st in (6, 14, 22, 30)]
           + [note(5, st, 74, 63) for st in (3, 11, 19, 27, 29)])

# --- Djembe and congas: djembe bass, tone and slap, conga open tones (two
# drums by the note: 60 and the tumba at 55) and slaps, and a bell (the 808
# cowbell on the hat voice). The bass has a compressor, the open congas a
# little tube.
KITS["DJE"] = kit(
    "Djembe", 112, ["DJB", "DJT", "DJS", "CGO", "CGS", "CB8"],
    mix=[(127, 63), (112, 52), (106, 52), (110, 80), (104, 80), (84, 30)], gain=0.68,
    extra={"PAR_VFX_TYPE1": 1, "PAR_VFX_AMT1": 60, "PAR_VFX_PAR1": 80,
           "PAR_VFX_TYPE4": 2, "PAR_VFX_AMT4": 30, "PAR_VFX_PAR4": 110, "PAR_VELOD6_OPEN": 31},
    groove=[(0, st, v) for st, v in ((0, 120), (7, 90), (8, 120), (14, 96))]
           + [(1, st, v) for st, v in ((2, 100), (3, 84), (10, 100), (11, 84))]
           + [(2, st, v) for st, v in ((4, 122), (12, 122), (6, 78), (15, 82))]
           + [note(3, st, v, n) for st, v, n in ((6, 104, 60), (7, 96, 60), (14, 104, 55), (15, 100, 55))]
           + [(4, st, v) for st, v in ((2, 110), (10, 110), (13, 62))]
           + [(5, st, 100 if st in (0, 7, 12) else 76) for st in (0, 2, 4, 5, 7, 9, 11, 12, 14)])

# --- Taiko: odaiko, two nagado-daiko (the second a little higher), shime,
# the rim ("ka") and an atarigane-like bell (the bonang high and short on the
# hat voice). The odaiko has a slow compressor (punch) and the nagados a
# little tube; a long delay on the shared FX, sent from the odaiko and the
# nagados, for the hall.
KITS["TAI"] = kit(
    "Taiko", 96, ["ODK", "NGD", "NGD", "SHM", "TKR", "BNG"],
    mix=[(127, 63), (112, 45), (106, 82), (92, 55), (100, 70), (78, 35)], gain=0.6, fxbus=True,
    extra={"PAR_COARSE3": 63, "PAR_COARSE6": 72, "PAR_VELOD6_CLOSED": 14, "PAR_VELOD6_OPEN": 30,
           "PAR_VFX_TYPE1": 1, "PAR_VFX_AMT1": 70, "PAR_VFX_PAR1": 90,
           "PAR_VFX_TYPE2": 2, "PAR_VFX_AMT2": 35, "PAR_VFX_PAR2": 100,
           "PAR_VFX_TYPE3": 2, "PAR_VFX_AMT3": 35, "PAR_VFX_PAR3": 100,
           "PAR_FX_TYPE": 4, "PAR_FX_ROUTING": 0, "PAR_FX_DRY_WET": 127,
           "PAR_FX_P1": 107, "PAR_FX_P2": 127, "PAR_FX_P3": 60, "PAR_FX_P4": 40,  # a quarter: 0.63 s
           "PAR_VFX_SEND1": 70, "PAR_VFX_SEND2": 45, "PAR_VFX_SEND3": 45},
    groove=[(0, st, v) for st, v in ((0, 127), (8, 112), (14, 100))]
           + [(1, st, v) for st, v in ((0, 120), (3, 90), (4, 118), (6, 92), (8, 120), (11, 90), (12, 118))]
           + [(2, st, v) for st, v in ((2, 100), (7, 96), (10, 100), (15, 104))]
           + [(3, st, 100 if st % 4 == 0 else 70) for st in range(0, 16, 2)] + [(3, 13, 60)]
           + [(4, st, v) for st, v in ((5, 110), (6, 80), (13, 110), (14, 80))]
           + [(5, st, 100 if st % 4 == 0 else 60) for st in range(0, 16, 2)])

def script(kit_key, outdir):
    k = KITS[kit_key]
    lines = ["kit init"]
    for v, inst in enumerate(k["instruments"]):
        lines.append(f"preset {v} {W[inst]}")
    lines += [f"set {p} {val}" for p, val in k["params"].items()]
    lines.append(f"savekit {outdir}/{k['name']}.SND {k['name']}")
    names = ["d1", "d2", "d3", "snare", "cym", "hhc", "hho"]
    lines.append("stereo 0")
    for v, n in enumerate(names):
        lines.append("len 1.5")
        lines.append(f"hit 0 {v} 60 127")
        lines.append(f"out {outdir}/{kit_key}_{n}.wav")
    # velocity steps on the kick and snare (accent behaviour)
    for v, n in ((0, "d1"), (3, "snare")):
        lines.append("len 2.0")
        for i, vel in enumerate((40, 80, 127)):
            lines.append(f"hit {i * 0.66:.3f} {v} 60 {vel}")
        lines.append(f"out {outdir}/{kit_key}_{n}_velsteps.wav")
    # four bars of the groove
    step = 60.0 / k["bpm"] / 4
    steps = k["steps"]
    lines.append("stereo 1")
    if k.get("fxbus"):
        lines.append("fxbus 1")
    lines.append(f"len {step * 64 + 1.5:.3f}")
    for rep in range(64 // steps):
        for hit in k["groove"]:
            v, s, vel = hit[:3]
            locks = " ".join(f"{p} {val}" for p, val in (hit[3] if len(hit) > 3 else ()))
            n = hit[4] if len(hit) > 4 else 60
            lines.append(f"hit {(rep * steps + s) * step:.4f} {v} {n} {vel} {locks}".rstrip())
    lines.append(f"out {outdir}/{kit_key}_groove.wav")
    path = f"{outdir}/{kit_key}.txt"
    open(path, "w").write("\n".join(lines) + "\n")
    return path


if __name__ == "__main__":
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for key in (sys.argv[2:] or KITS.keys()):
        print(script(key, out))
