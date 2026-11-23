#!/usr/bin/env python3
"""Noise Box measurement tool. We can't listen, so we measure.

Run with the repo venv:  .tools/bin/python tools/measure.py <command> ...

Commands
  file A.wav [B.wav ...]          loudness, peak, clipping and band balance per file
  compare A.wav REF.wav           1/3-octave tonal difference (REF minus A), loudness, width
  spectrogram A.wav OUT.png       spectrogram + average spectrum image (view it with your image reader)
  box <Box> [--input X.wav]       build <Box>Render, render every factory preset, check:
                                    - no clipping
                                    - presets loudness-matched to preset 0 (warn > 1 LU, fail > 2 LU)
           [--save-baseline]      store results in boxes/<Box>/baseline.json (uses the synthetic signal)
           [--check]              compare against baseline.json (catches accidental sound changes,
                                  e.g. after editing boxkit). Exit code 1 on any failure.
  signal [OUT.wav]                write the deterministic synthetic test signal

Test input for `box`: --input, else $NOISEBOX_TEST_WAV, else the synthetic plucked-guitar phrase.
Baselines always use the synthetic phrase so they are reproducible on any machine.
"""
import json
import os
import subprocess
import sys
from pathlib import Path

import numpy as np
import pyloudnorm as pyln
import scipy.signal as sig
import soundfile as sf

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
SR = 44100
BANDS = [(60, 150), (150, 300), (300, 1000), (1000, 3000), (3000, 6000), (6000, 12000)]
LOUDNESS_WARN_LU = 1.0   # presets should match preset 0 within this on the test signal
LOUDNESS_FAIL_LU = 2.0   # beyond this it is a real mismatch on any material
BASELINE_TOLERANCE_DB = 0.3


# ---------------------------------------------------------------- analysis
def db(x):
    return 20 * np.log10(np.maximum(x, 1e-12))


def load(path):
    x, sr = sf.read(str(path), always_2d=True)
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    return x, sr


def analyse(path):
    x, sr = load(path)
    m = x.mean(axis=1)
    f, p = sig.welch(m, sr, nperseg=8192)
    total = p.sum()
    side = (x[:, 0] - x[:, 1]) / 2
    return {
        "lufs": float(pyln.Meter(sr).integrated_loudness(x)),
        "peak_db": float(db(np.abs(x).max())),
        "clipped": int((np.abs(x) >= 0.999).sum()),
        "bands_db": [float(10 * np.log10(p[(f >= a) & (f < b)].sum() / total + 1e-20)) for a, b in BANDS],
        "centroid_hz": float((f * p).sum() / total),
        "width_db": float(db(np.sqrt((side ** 2).mean())) - db(np.sqrt((m ** 2).mean()))),
    }


def band_header():
    return " ".join(f"{a}-{b}".rjust(10) for a, b in BANDS)


def row(name, r):
    bands = " ".join(f"{v:10.1f}" for v in r["bands_db"])
    return f"{name[:22]:22} {r['lufs']:6.1f} {r['peak_db']:6.1f} {r['clipped']:6d} | {bands}"


def table_header():
    return f"{'name':22} {'LUFS':>6} {'peak':>6} {'clip':>6} | {band_header()}"


# ---------------------------------------------------------------- synthetic test signal
def synth_signal(path):
    """Deterministic plucked-guitar phrase (Karplus-Strong): single notes, octaves and a strum."""
    rng = np.random.default_rng(1955)
    out = np.zeros(int(SR * 14))

    def pluck(freq, start, dur, amp):
        n = int(SR * dur)
        period = int(SR / freq)
        burst = np.zeros(n)
        burst[:period] = rng.uniform(-1, 1, period) * amp
        burst[:period] = sig.lfilter([0.5, 0.5], [1], burst[:period])  # softer, thumb-like attack
        a = np.zeros(period + 2)
        a[0], a[period], a[period + 1] = 1.0, -0.4985, -0.4985
        y = sig.lfilter([1.0], a, burst)
        s = int(SR * start)
        out[s:s + n] += y[: len(out) - s]

    beat = 60 / 80
    melody = [196.0, 246.9, 293.7, 370.0, 329.6, 293.7, 246.9, 220.0]  # G major line
    for i, f in enumerate(melody):
        pluck(f, i * beat * 0.5, 1.6, 0.6)
    for i, f in enumerate([110.0, 146.8, 164.8, 130.8]):  # octaves
        pluck(f, 4.5 + i * beat, 2.0, 0.5)
        pluck(f * 2, 4.5 + i * beat + 0.005, 2.0, 0.4)
    for k, chord in enumerate([[98.0, 146.8, 196.0, 246.9, 293.7, 392.0], [110.0, 164.8, 220.0, 261.6, 329.6]]):
        for j, f in enumerate(chord):  # strums
            pluck(f, 8.5 + k * 2.4 + j * 0.018, 2.4, 0.35)
    out = out / np.abs(out).max() * 10 ** (-1 / 20)  # peak -1 dBFS, like a hot DI take
    sf.write(str(path), out.astype(np.float32), SR, subtype="PCM_24")
    return path


def default_input(explicit=None):
    if explicit:
        return Path(explicit)
    env = os.environ.get("NOISEBOX_TEST_WAV")
    if env and Path(env).exists():
        return Path(env)
    return synthetic_path()


def synthetic_path():
    p = BUILD / "test-signals" / "pluck.wav"
    p.parent.mkdir(parents=True, exist_ok=True)
    if not p.exists():
        synth_signal(p)
    return p


# ---------------------------------------------------------------- box rendering
def render_tool(box):
    target = f"{box}Render"
    cmake = ROOT / ".tools" / "bin" / "cmake"
    subprocess.run([str(cmake), "--build", str(BUILD), "--target", target], check=True,
                   stdout=subprocess.DEVNULL)
    exe = BUILD / "boxes" / box / f"{target}_artefacts" / "Release" / target
    if not exe.exists():
        sys.exit(f"render tool not found: {exe}")
    return exe


def render_presets(box, input_wav):
    exe = render_tool(box)
    names = subprocess.run([str(exe), "--presets"], check=True, capture_output=True, text=True).stdout.splitlines()
    outdir = BUILD / "renders" / box
    outdir.mkdir(parents=True, exist_ok=True)
    results = []
    for i, name in enumerate(names):
        out = outdir / f"{i:02d}.wav"
        subprocess.run([str(exe), str(input_wav), str(out), f"preset:{i}"], check=True, stdout=subprocess.DEVNULL)
        results.append((name, out, analyse(out)))
    return results


def cmd_box(args):
    box = args[0]
    save = "--save-baseline" in args
    check = "--check" in args
    explicit = args[args.index("--input") + 1] if "--input" in args else None
    input_wav = synthetic_path() if (save or check) else default_input(explicit)

    print(f"{box}: rendering factory presets from {input_wav}")
    src = analyse(input_wav)
    print(table_header())
    print(row("(input)", src))
    results = render_presets(box, input_wav)
    for name, _, r in results:
        print(row(name, r))
    print(f"renders in {BUILD / 'renders' / box}")

    failures, warnings = [], []
    ref = results[0][2]["lufs"]
    for name, _, r in results:
        if r["clipped"]:
            failures.append(f"{name}: {r['clipped']} clipped samples")
        delta = r["lufs"] - ref
        if abs(delta) > LOUDNESS_FAIL_LU:
            failures.append(f"{name}: {delta:+.1f} LU vs preset 0 (adjust its output dB)")
        elif abs(delta) > LOUDNESS_WARN_LU:
            warnings.append(f"{name}: {delta:+.1f} LU vs preset 0 on this input (matching depends on material)")

    baseline_file = ROOT / "boxes" / box / "baseline.json"
    if save:
        data = {name: {"lufs": r["lufs"], "bands_db": r["bands_db"]} for name, _, r in results}
        baseline_file.write_text(json.dumps(data, indent=2) + "\n")
        print(f"saved {baseline_file}")
    if check:
        if not baseline_file.exists():
            failures.append("no baseline.json (run with --save-baseline first)")
        else:
            base = json.loads(baseline_file.read_text())
            for name, _, r in results:
                if name not in base:
                    failures.append(f"{name}: not in baseline (new preset? re-save the baseline)")
                    continue
                diffs = [r["lufs"] - base[name]["lufs"]] + [a - b for a, b in zip(r["bands_db"], base[name]["bands_db"])]
                worst = max(diffs, key=abs)
                if abs(worst) > BASELINE_TOLERANCE_DB:
                    failures.append(f"{name}: differs from baseline by {worst:+.2f} dB")
            if not failures:
                print("baseline: identical within tolerance")

    for w in warnings:
        print("  warning:", w)
    if failures:
        print("\nFAIL")
        for f in failures:
            print("  -", f)
        sys.exit(1)
    print("\nOK: no clipping, presets loudness-matched")


def cmd_file(paths):
    print(table_header())
    for p in paths:
        print(row(Path(p).name, analyse(p)))


def cmd_compare(a, ref):
    xa, sra = load(a)
    xr, srr = load(ref)
    ra, rr = analyse(a), analyse(ref)
    print(f"loudness  {ra['lufs']:.1f} vs ref {rr['lufs']:.1f} LUFS")
    print(f"width     {ra['width_db']:.1f} vs ref {rr['width_db']:.1f} dB (side/mid)")
    print(f"centroid  {ra['centroid_hz']:.0f} vs ref {rr['centroid_hz']:.0f} Hz")

    def third_octaves(x, sr):
        f, p = sig.welch(x.mean(axis=1), sr, nperseg=16384)
        tot = p.sum()
        centres = [25 * 2 ** (i / 3) for i in range(30) if 25 * 2 ** (i / 3) < 18000]
        return centres, [10 * np.log10(p[(f >= c / 2 ** (1 / 6)) & (f < c * 2 ** (1 / 6))].sum() / tot + 1e-20) for c in centres]

    c, la = third_octaves(xa, sra)
    _, lr = third_octaves(xr, srr)
    print("1/3-octave: ref minus file (positive = file needs more there)")
    for f, a_, r_ in zip(c, la, lr):
        bar = "+" * int(max(0, r_ - a_)) + "-" * int(max(0, a_ - r_))
        print(f"{f:8.0f} Hz {r_ - a_:+6.1f}  {bar}")


def cmd_spectrogram(path, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    x, sr = load(path)
    m = x.mean(axis=1)
    fig, ax = plt.subplots(2, 1, figsize=(13, 8))
    f, t, s = sig.spectrogram(m, sr, nperseg=4096, noverlap=3072)
    ax[0].pcolormesh(t, f, 10 * np.log10(s + 1e-12), shading="auto", vmin=-120, vmax=-20)
    ax[0].set_yscale("symlog", linthresh=100)
    ax[0].set_ylim(30, sr / 2)
    ax[0].set_title(f"{Path(path).name} spectrogram")
    fw, p = sig.welch(m, sr, nperseg=8192)
    ax[1].semilogx(fw, 10 * np.log10(p / p.sum() + 1e-20))
    ax[1].set_xlim(30, 20000)
    ax[1].grid(True, which="both")
    ax[1].set_title("average spectrum (normalised)")
    plt.tight_layout()
    plt.savefig(out, dpi=80)
    print(f"wrote {out}")


def main():
    a = sys.argv[1:]
    if not a or a[0] in ("-h", "--help"):
        print(__doc__)
        return
    cmd, rest = a[0], a[1:]
    if cmd == "file" and rest:
        cmd_file(rest)
    elif cmd == "compare" and len(rest) == 2:
        cmd_compare(*rest)
    elif cmd == "spectrogram" and len(rest) == 2:
        cmd_spectrogram(*rest)
    elif cmd == "box" and rest:
        cmd_box(rest)
    elif cmd == "signal":
        print(synth_signal(Path(rest[0])) if rest else synthetic_path())
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == "__main__":
    main()
