#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Plot waveform, envelope, spectrogram and pitch track of rendered WAVs.

usage: analyze.py out.png file1.wav [file2.wav ...]
Prints per-file stats: peak, RMS, -20/-40/-60 dB decay times, pitch at 10/50/200 ms.
"""
import sys
import numpy as np
from scipy.io import wavfile
from scipy import signal
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load(path):
    fs, x = wavfile.read(path)
    if x.ndim > 1:
        x = x.mean(axis=1)
    return fs, x.astype(np.float64) / 32768.0


def envelope_db(x, fs, win_ms=2.0):
    n = max(1, int(fs * win_ms / 1000))
    e = np.sqrt(np.convolve(x * x, np.ones(n) / n, mode="same"))
    return 20 * np.log10(e + 1e-9)


def decay_times(env_db, fs):
    pk = np.argmax(env_db)
    out = {}
    for d in (20, 40, 60):
        idx = np.where(env_db[pk:] < env_db[pk] - d)[0]
        out[d] = (idx[0] / fs * 1000) if len(idx) else float("nan")
    return out


def pitch_at(x, fs, t_ms, win_ms=25.0, fmin=25, fmax=2000):
    a = int(fs * t_ms / 1000)
    n = int(fs * win_ms / 1000)
    seg = x[a:a + n]
    if len(seg) < n or np.max(np.abs(seg)) < 1e-4:
        return float("nan")
    seg = seg * np.hanning(len(seg))
    nfft = 1 << 16
    sp = np.abs(np.fft.rfft(seg, nfft))
    f = np.fft.rfftfreq(nfft, 1 / fs)
    band = (f >= fmin) & (f <= fmax)
    return float(f[band][np.argmax(sp[band])])


def main():
    out = sys.argv[1]
    files = sys.argv[2:]
    fig, axes = plt.subplots(len(files), 3, figsize=(15, 3.2 * len(files)), squeeze=False)
    for row, path in enumerate(files):
        fs, x = load(path)
        t = np.arange(len(x)) / fs
        env = envelope_db(x, fs)
        dt = decay_times(env, fs)
        p = [pitch_at(x, fs, ms) for ms in (5, 10, 30, 60, 120, 250)]
        rms = 20 * np.log10(np.sqrt(np.mean(x[: int(fs * 0.3)] ** 2)) + 1e-9)
        print(f"{path}: peak {20*np.log10(np.max(np.abs(x))+1e-9):.1f} dBFS, rms300ms {rms:.1f} dB, "
              f"decay -20/-40/-60 dB: {dt[20]:.0f}/{dt[40]:.0f}/{dt[60]:.0f} ms, "
              f"pitch@5/10/30/60/120/250ms: " + "/".join(f"{v:.0f}" for v in p))
        name = path.split("/")[-1]
        lim = min(len(x), int(fs * 0.6))
        axes[row, 0].plot(t[:lim] * 1000, x[:lim], lw=0.5)
        axes[row, 0].set_title(f"{name} waveform (ms)")
        axes[row, 1].plot(t * 1000, env, lw=0.8)
        axes[row, 1].set_ylim(-90, 3)
        axes[row, 1].set_title("envelope dB (ms)")
        f, tt, S = signal.spectrogram(x, fs, nperseg=1024, noverlap=896)
        axes[row, 2].pcolormesh(tt * 1000, f, 10 * np.log10(S + 1e-12), shading="auto", vmin=-120, vmax=-20)
        axes[row, 2].set_yscale("symlog", linthresh=200)
        axes[row, 2].set_ylim(20, fs / 2)
        axes[row, 2].set_title("spectrogram (ms / Hz)")
    fig.tight_layout()
    fig.savefig(out, dpi=70)


if __name__ == "__main__":
    main()
