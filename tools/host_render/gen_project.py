#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build an LXR-02 SD-card project folder with the analog model kits and demo patterns.

usage: gen_project.py <kit .SND dir> <out dir> <project number>
Creates <out dir>/projNN/ with prj.nfo, 64 kits (NN-name.snd) and 64 patterns
(NN-name.pat, format "PatV03" with the extra parameter locks behind it), using
the formats of presetManager.c.
"""
import importlib.util
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("kits", os.path.join(HERE, "kits.py"))
kits = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kits)

END_OF_SOUND_PARAMETERS = 247
NUM_TRACKS, NUM_STEPS, NUM_BARS, SLOTS = 7, 64, 4, 64
NO_AUTOMATION = 0xFF
STEP_ACTIVE = 0x80
NUM_STEP_LOCKS, LOCK_POOL_SIZE = 8, 192   # sequencer.h
LOCK_POOL_TAG = b"XLK1"                   # presetManager.c
TIMING_DEFAULT = 0x70  # flam 0 (low nibble), shift 7 = no shift (high nibble)


def name8(s):
    b = s.encode("ascii")[:8]
    return b + b"\0" * (8 - len(b))


def file_name(slot, name, ext):
    # presetManager.c: "NN-" + first 5 characters of the name, spaces as '_'
    short = re.sub(r"[^A-Za-z0-9_]", "_", name[:5].replace(" ", "_"))
    return f"{slot:02d}-{short}.{ext}"


def core_dir():
    # tools/host_render inside the firmware repository, or next to a fw/ checkout
    for c in (os.path.join(HERE, "..", "..", "Core"), os.path.join(HERE, "..", "fw", "Core")):
        if os.path.isdir(c):
            return c
    sys.exit("firmware Core/ directory not found")


def param_numbers():
    """numbers of the ParamEnums entries in Parameters.h (the front panel numbering)"""
    src = open(os.path.join(core_dir(), "Inc", "Parameters.h"), encoding="utf-8", errors="replace").read()
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    src = re.sub(r"//[^\n]*", "", src)
    body = re.search(r"enum\s+ParamEnums\s*\{(.*?)\}", src, re.S).group(1)
    nums, cur = {}, -1
    for item in body.split(","):
        item = item.strip()
        if not item:
            continue
        if "=" in item:
            name, val = [x.strip() for x in item.split("=", 1)]
            cur = int(val, 0) if re.match(r"^(0x)?[0-9a-fA-F]+$", val) else nums[val]
        else:
            name = item
            cur += 1
        nums[name] = cur
    return nums


def lock_code(nums, par):
    # how a step lock stores a parameter (seq_recordAutomation): number+1 below 128
    n = nums[par]
    assert n < END_OF_SOUND_PARAMETERS and n != 127, f"{par} can not be locked"
    return n + 1 if n < 128 else n


def init_kit_params():
    src = open(os.path.join(core_dir(), "Src", "Preset", "initKit.c")).read()
    vals = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{2})", src)]
    p = vals[8:]
    return bytes(p + [0] * (END_OF_SOUND_PARAMETERS - len(p)))[:END_OF_SOUND_PARAMETERS]


def pattern_bytes(name, hits, kit_nr, length=16, nums=None):
    steps = [[[127, 100, 60, NO_AUTOMATION, 0, NO_AUTOMATION, 0] for _ in range(NUM_STEPS)] for _ in range(NUM_TRACKS)]
    main = [[0] * NUM_BARS for _ in range(NUM_TRACKS)]
    pool = []   # the extra locks: (track, step, parameter, value)
    for hit in hits:
        trk, step, vel = hit[:3]
        st = steps[trk][step]
        st[0] = (vel & 0x7F) | STEP_ACTIVE
        if len(hit) > 4:
            st[2] = hit[4]                    # the step's note
        main[trk][step // 16] |= 1 << (step % 16)
        # up to 8 parameter locks: lock 1 and 2 in the step, the others in the pool
        locks = hit[3] if len(hit) > 3 else ()
        assert len(locks) <= NUM_STEP_LOCKS, f"track {trk} step {step}: more than {NUM_STEP_LOCKS} locks"
        for slot, (par, val) in enumerate(locks):
            if slot < 2:
                st[3 + 2 * slot] = lock_code(nums, par)
                st[4 + 2 * slot] = val
            else:
                pool.append((trk, step, lock_code(nums, par), val))
    assert len(pool) <= LOCK_POOL_SIZE, f"{name}: more than {LOCK_POOL_SIZE} extra locks"
    out = bytearray(name8(name) + b"PatV03")
    for trk in range(NUM_TRACKS):
        for st in steps[trk]:
            out += bytes(st)
        out += bytes([TIMING_DEFAULT] * NUM_STEPS)
    for trk in range(NUM_TRACKS):
        for bar in range(NUM_BARS):
            out += struct.pack("<H", main[trk][bar])
    out += bytes([0])                         # shuffle
    out += bytes([length] * NUM_TRACKS)       # track lengths
    out += bytes([kit_nr])                    # kit used by the pattern
    # the extra locks (custom firmware), ignored by the original firmware
    out += LOCK_POOL_TAG + bytes([len(pool)])
    for lock in pool:
        out += bytes(lock)
    return bytes(out)


def main():
    snd_dir, out_dir, proj = sys.argv[1], sys.argv[2], int(sys.argv[3])
    folder = os.path.join(out_dir, f"proj{proj:02d}")
    os.makedirs(folder, exist_ok=True)
    for f in os.listdir(folder):
        os.remove(os.path.join(folder, f))
    open(os.path.join(folder, "prj.nfo"), "wb").write(name8("AnalogMd"))

    order = ["808", "909", "PERC", "BAT", "LOCK", "GAM", "DJE", "TAI"]
    pattern_names = {"808": "808Groov", "909": "909Groov", "PERC": "PercGroo", "BAT": "BatRock", "LOCK": "LockDemo",
                     "GAM": "GamelanG", "DJE": "DjembeGr", "TAI": "TaikoGro"}
    init = init_kit_params()
    nums = param_numbers()
    for slot in range(SLOTS):
        if slot < len(order):
            k = kits.KITS[order[slot]]
            path = os.path.join(snd_dir, k["name"] + ".SND")
            if not os.path.exists(path):
                sys.exit(f"{path} is missing: render all kit scripts made by kits.py first")
            data = open(path, "rb").read()
            # name, parameters and the kit extension (XKT1 block) of the custom firmware
            assert len(data) >= 8 + END_OF_SOUND_PARAMETERS, (k["name"], len(data))
            name = k["name"]
        else:
            name = "Init"
            data = name8(name) + init
        open(os.path.join(folder, file_name(slot, name, "snd")), "wb").write(data)

        if slot < len(order):
            k = kits.KITS[order[slot]]
            pname = pattern_names[order[slot]]
            pdata = pattern_bytes(pname, k["groove"], slot, k.get("steps", 16), nums)
        else:
            pname = "Empty"
            pdata = pattern_bytes(pname, [], 0)
        open(os.path.join(folder, file_name(slot, pname, "pat")), "wb").write(pdata)
    print(f"{folder}: {len(os.listdir(folder))} files")


if __name__ == "__main__":
    main()
