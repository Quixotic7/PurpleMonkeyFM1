#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PurpleMonkey FM-1 contributors
"""PurpleMonkey's voice: spoken words -> LPC-10 frames -> a C header (firmware/src/pm_speech_data.h).

  tools/gen_pm_speech.py [OUT.h] [--src DIR] [--voice NAME] [--rate WPM] [--word-rate WPM] [--pitch HZ] [--bend B] [--preemph A] [--level N]

The voice is made the way the talking toys of 1978 made theirs: a word is spoken once, analysed here into a few
numbers every 12.5 ms (how loud, the pitch or "no pitch: a hiss", and ten reflection coefficients that are the shape
of the mouth), and the instrument plays those numbers back through a ten-stage lattice filter at 8 kHz
(firmware/src/pm_speech.c). What is in the firmware is about 500 bytes a second of speech, no recording.
The toys had a frame every 25 ms; at that a short word's consonants run into its vowels (a letter's name, one
long vowel, does not mind), so the frames here are half as long.

Where the spoken words come from:
  --src DIR     your own recordings: DIR/a.wav .. DIR/z.wav, yay.wav, monkey.wav, cat.wav, dog.wav, llama.wav,
                hello.wav, bye.wav (16-bit PCM WAV, any rate, mono or stereo, one word each; silence around the
                word is trimmed). One voice, close to the microphone, a quiet room.
  otherwise     macOS's `say` speaks them (--voice; --rate the letters, --word-rate the words, slower: said
                one at a time, clearly), kept in build/speech_src/<voice>/ so a second run
                does not speak again. This is how the header in the tree was made; see docs/PURPLEMONKEY.md for
                what that means for a release.

The format (all of it written to the header, so pm_speech.c and this file cannot disagree):
  a frame, 12.5 ms energy 4 bits: 0 a silent frame (nothing follows), 15 the end of the word (nothing follows)
                   repeat 1 bit:  1 = the coefficients of the frame before
                   pitch  5 bits: 0 unvoiced (noise), else an index into PM_LPC_PITCH (the period in samples)
                   k1 .. k10      5 5 4 4 4 4 4 3 3 3 bits, indexes into PM_LPC_K1 .. (unless repeated)
  the tables       a coefficient's levels are evenly spaced in log area ratio over the range the words use; the
                   energies are 3 dB apart; the periods a constant ratio apart
  the excitation   a voiced frame is driven by PM_LPC_CHIRP once a period: an impulse with its frequencies spread
                   out in time (the same energy at every frequency, a far lower peak), designed below; an
                   unvoiced frame by +-PM_LPC_NOISE at random
No table here is taken from a chip or a ROM: the layout of a frame follows the TMS5100's (but an unvoiced frame
has all ten coefficients here, not four: a k or a t is clearer for it, at 340 bytes), the numbers are this file's
and fit these words.
"""
import argparse
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
FS = 8000
FRAME = 100                     # 12.5 ms
STEPS = 4                       # pm_speech.c: the steps a frame's values are approached in
WIN = 200                       # the analysis window, 25 ms, centred on the frame
ORDER = 10
KBITS = [5, 5, 4, 4, 4, 4, 4, 3, 3, 3]
LAGWIN = np.exp(-0.5 * (2 * np.pi * 50.0 / FS * np.arange(ORDER + 1)) ** 2)   # every resonance ~50 Hz wider
PITCH_MIN, PITCH_MAX = 32, 150  # periods in samples: 250 Hz .. 53 Hz
CHIRP_N = 32
NOISE = 32
OUT_SHIFT = 6                   # pm_speech.c: the lattice's output >> this is the sample

# (the enum's name, what the screen may show, what is said). The letters first, A .. Z: pm_speech_letter() counts
# on it; the pets after YAY in the order of the pets (pm_ui.c: PM_W_MONKEY + pet)
WORDS = [(c, c, c) for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ"] + [
    ("YAY", "YAY", "yay"), ("MONKEY", "MONKEY", "monkey"), ("CAT", "CAT", "cat"), ("DOG", "DOG", "dog"),
    ("LLAMA", "LLAMA", "llama"), ("HELLO", "HELLO", "hello"), ("BYE", "BYE", "bye")]
SAID = {"Z": "zee"}             # (a British voice would say zed)


# ---------------------------------------------------------------- the words, as 8 kHz mono floats ----
def read_wav(path):
    with wave.open(str(path)) as w:
        if w.getsampwidth() != 2:
            sys.exit(f"{path}: 16-bit PCM wanted")
        x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64) / 32768.0
        if w.getnchannels() > 1:
            x = x.reshape(-1, w.getnchannels()).mean(axis=1)
        return x, w.getframerate()


def lowpass(x, fs, fc, taps=127):
    n = np.arange(taps) - (taps - 1) / 2
    h = np.sinc(2 * fc / fs * n) * np.hamming(taps)
    return np.convolve(x, h / h.sum(), "same")


def to_8k(x, fs):
    if fs != FS:
        x = lowpass(x, fs, 3700.0) if fs > FS else x
        t = np.arange(int(len(x) * FS / fs)) * (fs / FS)
        x = np.interp(t, np.arange(len(x)), x)
    return x - x.mean()


def trim(x):
    """the word without the silence around it (25 ms kept in front for a stop's burst, 50 ms behind)"""
    env = np.sqrt(np.convolve(x * x, np.ones(80) / 80, "same"))
    on = np.nonzero(env > env.max() * 0.02)[0]
    return x[max(0, on[0] - 200):min(len(x), on[-1] + 400)]


def speak(voice, rate, text, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["say", "-v", voice, "-r", str(rate), "-o", str(path), "--data-format=LEI16@16000", text],
                   check=True)


def load_words(args):
    out = []
    for name, _, text in WORDS:
        if args.src:
            p = Path(args.src) / f"{name.lower()}.wav"
            if not p.exists():
                sys.exit(f"{p}: missing")
        else:
            rate = args.rate if len(name) == 1 else args.word_rate
            p = ROOT / "build" / "speech_src" / f"{args.voice.split(' ')[0].lower()}-{rate}" / f"{name.lower()}.wav"
            if not p.exists():
                speak(args.voice, rate, SAID.get(name, text), p)
        x = trim(to_8k(*read_wav(p)))
        out.append(x / np.abs(x).max())
    return out


# ---------------------------------------------------------------- analysis ----
def pitch_track(x):
    """per frame: the period in samples, 0 where there is no voice. Under 900 Hz, 20 ms of the signal against the
    20 ms a lag later (short: the pitch of a word said alone falls fast), normalised; the shortest lag near the best
    (a period, not two of them). A stretch the correlation gives up on between two voiced ones, loud and with its
    energy down low, is the voice still: its pitch is drawn across"""
    pad, w = 400, 160
    xl = np.concatenate([np.zeros(pad), lowpass(x, FS, 900.0, 63), np.zeros(pad + FRAME)])
    xp = np.concatenate([np.zeros(pad), x, np.zeros(pad + FRAME)])
    nfr = len(x) // FRAME
    per, strength, lowish, rms = np.zeros(nfr), np.zeros(nfr), np.zeros(nfr), np.zeros(nfr)
    lags = np.arange(PITCH_MIN - 2, PITCH_MAX + 10)
    for n in range(nfr):
        c = n * FRAME + FRAME // 2 + pad
        nc = np.zeros(len(lags))
        for i, L in enumerate(lags):
            a, b = xl[c - (w + L) // 2:c - (w + L) // 2 + w], xl[c - (w + L) // 2 + L:c - (w + L) // 2 + L + w]
            nc[i] = np.dot(a, b) / (np.sqrt(np.dot(a, a) * np.dot(b, b)) + 1e-12)
        best = nc.max()
        peaks = [i for i in range(1, len(nc) - 1) if nc[i] >= nc[i - 1] and nc[i] >= nc[i + 1] and nc[i] >= 0.85 * best]
        i = peaks[0] if peaks else int(nc.argmax())
        full, low = xp[c - 100:c + 100], xl[c - 100:c + 100]
        rms[n] = np.sqrt(np.mean(full * full))
        lowish[n] = np.dot(low, low) / (np.dot(full, full) + 1e-12)      # a hiss has little under 900 Hz
        per[n], strength[n] = lags[i], best
    voiced = (strength > 0.6) & (lowish > 0.15) & (rms > rms.max() * 0.02)
    if not voiced.any():
        return np.zeros(nfr, dtype=int)
    med = np.median(per[voiced])
    for n in range(nfr):                                                # an octave off the rest of the word
        if voiced[n] and per[n] > 1.9 * med:
            per[n] /= 2
    near = [n > 0 and voiced[n - 1] and abs(np.log(per[n] / per[n - 1])) < 0.14 for n in range(nfr)]
    for n in range(nfr):                                                # a voice's pitch goes on from frame to frame:
        if voiced[n] and not near[n] and not (n + 1 < nfr and near[n + 1]):   # a lone "period" is noise that happened to repeat
            voiced[n] = False
    on = np.nonzero(voiced)[0]
    for a, b in zip(on[:-1], on[1:]):                                   # the gaps the correlation left
        if b - a > 1 and all(lowish[a + 1:b] > 0.3) and all(rms[a + 1:b] > rms.max() * 0.15):
            for n in range(a + 1, b):
                per[n] = per[a] * (per[b] / per[a]) ** ((n - a) / (b - a))
                voiced[n] = True
    for n in range(1, nfr - 1):                                         # no decision alone among its neighbours
        if voiced[n - 1] == voiced[n + 1] != voiced[n]:
            if not voiced[n]:
                per[n] = np.sqrt(per[n - 1] * per[n + 1])
            voiced[n] = voiced[n - 1]
    n = 0
    while n < nfr:                                                      # nobody voices for under 40 ms
        m = n
        while m < nfr and voiced[m] == voiced[n]:
            m += 1
        if voiced[n] and m - n < 3:
            voiced[n:m] = False
        n = m
    return np.where(voiced, np.clip(np.round(per), PITCH_MIN, PITCH_MAX), 0).astype(int)


def reflection(r, order):
    """Levinson-Durbin: autocorrelation -> reflection coefficients k[0 .. order-1], A(z) = 1 + sum a[j] z^-j"""
    a, e, ks = np.zeros(order + 1), r[0], []
    a[0] = 1.0
    for m in range(1, order + 1):
        k = -(r[m] + np.dot(a[1:m], r[m - 1:0:-1])) / e
        a[1:m + 1] = np.concatenate([a[1:m] + k * a[m - 1:0:-1], [k]])
        e *= 1.0 - k * k
        ks.append(k)
    return np.array(ks)


def analyse(x, pre):
    """-> per frame (rms, period, k[10]): of the pre-emphasised word (the treble up: a small speaker has no bass
    to give, and the consonants are up there)"""
    y = np.append(x[0], x[1:] - pre * x[:-1])
    per = pitch_track(x)
    yp = np.concatenate([np.zeros(WIN), y, np.zeros(WIN + FRAME)])
    w = np.hamming(WIN)
    frames = []
    for n in range(len(per)):
        c = n * FRAME + FRAME // 2 + WIN
        seg = yp[c - WIN // 2:c + WIN // 2] * w
        r = np.array([np.dot(seg[:WIN - i], seg[i:]) for i in range(ORDER + 1)])
        rms = np.sqrt(r[0] / np.dot(w, w))                 # over the window, two pitch periods or more: over the frame
        r[0] = r[0] * 1.0003 + 1e-9                        # alone it would count the pulses in it and flutter
        r = r * LAGWIN                                     # no resonance narrower than a mouth's: none that rings
        k = np.clip(reflection(r, ORDER), -0.985, 0.985)
        frames.append((rms, int(per[n]), k))
    return frames


# ---------------------------------------------------------------- the tables ----
def lar(k):
    return np.log((1 + k) / (1 - k))


def k_tables(words_frames):
    tabs = []
    for i in range(ORDER):
        ks = np.array([k[i] for fr in words_frames for rms, per, k in fr ])
        lo, hi = np.percentile(lar(ks), [1, 99])
        g = np.linspace(lo, hi, 1 << KBITS[i])
        tabs.append((np.exp(g) - 1) / (np.exp(g) + 1))
    return tabs


def make_chirp():
    """an impulse with its frequencies spread over ~20 samples, the low ones first: every frequency as strong as
    in an impulse (so the lattice alone shapes the vowel), the peak a third of an impulse's"""
    n = 128
    k = np.arange(n // 2 + 1)
    t0, t1 = 2.0, 20.0                                    # group delay at 0 Hz and at 4 kHz, samples
    ph = -(2 * np.pi / n) * (t0 * k + (t1 - t0) * k * k / n)
    spec = np.exp(1j * ph)
    spec[0] = 0
    c = np.fft.irfft(spec, n)[:CHIRP_N]
    c[-6:] *= np.linspace(1, 0, 7)[:-1]
    return np.round(c / np.abs(c).max() * 127).astype(int)


def quantise(words_frames, ktabs, pitches, chirp, level, pitch_hz, bend):
    """-> (the energy table, per word a list of frames (e, rep, p, kidx) / None for silence)"""
    csum = np.cumsum(chirp.astype(float) ** 2)
    scale = level * (1 << OUT_SHIFT)
    med = np.median([per for fr in words_frames for rms, per, k in fr if per])
    mid = FS / pitch_hz if pitch_hz else med
    raw = []
    for fr in words_frames:
        peak = max(rms for rms, per, k in fr)
        rows = []
        for rms, per, k in fr:
            n = 10
            ki = [int(np.abs(lar(ktabs[i]) - lar(k[i])).argmin()) for i in range(n)]
            kq = np.array([ktabs[i][ki[i]] for i in range(n)])
            pi = int(np.abs(np.log(pitches / (mid * (per / med) ** bend))).argmin()) + 1 if per else 0
            exc = rms / peak * 0.25 * np.sqrt(np.prod(1 - kq * kq)) * scale   # what must go into the lattice (rms)
            if per:
                p = pitches[pi - 1]
                exc /= np.sqrt(csum[min(p, CHIRP_N) - 1] / p)
            else:
                exc /= NOISE
            rows.append((exc if rms > peak * 0.012 else 0.0, pi, ki))
        raw.append(rows)
    top = max(e for rows in raw for e, pi, ki in rows)
    energy = np.concatenate([[0], np.round(top * 10 ** (-3.0 * np.arange(13, -1, -1) / 20)), [0]]).astype(int)
    out = []
    for rows in raw:
        frames, prev = [], None
        for e, pi, ki in rows:
            ei = int(np.abs(np.log(energy[1:15]) - np.log(e)).argmin()) + 1 if e >= energy[1] * 0.7 else 0
            if not ei:
                frames.append(None)
                continue
            rep = prev is not None and prev[1] == ki and (prev[0] != 0) == (pi != 0)
            frames.append((ei, int(rep), pi, ki))
            prev = (pi, ki)
        while frames and frames[-1] is None:
            frames.pop()
        while frames and frames[0] is None:
            frames.pop(0)
        out.append(frames)
    return energy, out


def pack(frames):
    bits = []

    def put(v, n):
        bits.extend((v >> (n - 1 - i)) & 1 for i in range(n))
    for f in frames:
        if f is None:
            put(0, 4)
            continue
        e, rep, pi, ki = f
        put(e, 4), put(rep, 1), put(pi, 5)
        if not rep:
            for i, v in enumerate(ki):
                put(v, KBITS[i])
    put(15, 4)
    bits += [0] * (-len(bits) % 8)
    return [int("".join(map(str, bits[i:i + 8])), 2) for i in range(0, len(bits), 8)]


# ---------------------------------------------------------------- the header ----
def c_array(ctype, name, vals, per_line=16):
    lines = [", ".join(str(int(v)) for v in vals[i:i + per_line]) for i in range(0, len(vals), per_line)]
    return f"static const {ctype} {name}[{len(vals)}] = {{\n    " + ",\n    ".join(lines) + "};\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("out", nargs="?", default=str(ROOT / "firmware" / "src" / "pm_speech_data.h"))
    ap.add_argument("--src", help="a folder of your own recordings (a.wav .. bye.wav)")
    ap.add_argument("--voice", default="Reed (English (US))", help="the `say` voice (say -v '?')")
    ap.add_argument("--rate", type=int, default=150, help="`say`'s words a minute for the letters")
    ap.add_argument("--word-rate", type=int, default=80, help="`say`'s words a minute for the words")
    ap.add_argument("--pitch", type=float, default=0, help="move the voice's middle pitch here, Hz (0: as spoken)")
    ap.add_argument("--bend", type=float, default=0.5,
                    help="how much of the spoken rise and fall of the pitch is kept: 0 a monotone, 1 all of it")
    ap.add_argument("--preemph", type=float, default=0.8, help="treble lift before analysis, 0 .. 0.95")
    ap.add_argument("--level", type=int, default=24000, help="the peak aimed at, in the mix's scale")
    args = ap.parse_args()

    words = load_words(args)
    frames = [analyse(x, args.preemph) for x in words]
    ktabs = k_tables(frames)
    pitches = np.unique(np.round(PITCH_MIN * (PITCH_MAX / PITCH_MIN) ** (np.arange(31) / 30)).astype(int))
    pitches = np.concatenate([pitches, np.full(31 - len(pitches), pitches[-1])])
    chirp = make_chirp()
    energy, coded = quantise(frames, ktabs, pitches, chirp, args.level, args.pitch, args.bend)

    data, offs = [], []
    for fr in coded:
        offs.append(len(data))
        data += pack(fr)
    if len(data) > 65535:
        sys.exit("more than 64 KB of speech")
    spec = np.abs(np.fft.rfft(chirp, 256))[4:124]
    nfr = sum(len(f) for f in coded)
    where = f"recordings in {args.src}" if args.src else f"macOS say, the voice {args.voice}, {args.rate} / {args.word_rate} words a minute (letters / words)"
    h = ["/* SPDX-License-Identifier: GPL-3.0-only\n * Copyright (C) 2026 PurpleMonkey FM-1 contributors */\n",
         "/* GENERATED by tools/gen_pm_speech.py: do not edit. PurpleMonkey's words as LPC-10 frames for pm_speech.c\n"
         f" * (the format is described there and in the tool). From: {where}; pre-emphasis {args.preemph}.\n"
         f" * {len(WORDS)} words, {nfr} frames ({nfr * FRAME / FS:.1f} s), {len(data)} bytes. */\n",
         "#ifndef PM_SPEECH_DATA_H\n#define PM_SPEECH_DATA_H\n#include <stdint.h>\n",
         f"#define PM_LPC_FS {FS}u\n#define PM_LPC_FRAME {FRAME}u\n#define PM_LPC_STEPS {STEPS}\n#define PM_LPC_ORDER {ORDER}\n"
         f"#define PM_LPC_CHIRP_N {CHIRP_N}u\n#define PM_LPC_NOISE {NOISE}\n#define PM_LPC_SHIFT {OUT_SHIFT}\n",
         "enum { " + ", ".join(f"PM_W_{n}" for n, _, _ in WORDS) + ", PM_NWORD };\n",
         "static const char *const PM_WORD_NAME[PM_NWORD] = {" + ", ".join(f'"{s}"' for _, s, _ in WORDS) + "};\n",
         c_array("uint8_t", "PM_LPC_KBITS", KBITS),
         c_array("uint16_t", "PM_LPC_ENERGY", energy),
         c_array("uint8_t", "PM_LPC_PITCH", np.concatenate([[0], pitches])),
         c_array("int8_t", "PM_LPC_CHIRP", chirp)]
    for i, t in enumerate(ktabs):
        h.append(c_array("int16_t", f"PM_LPC_K{i + 1}", np.round(t * 32768)))
    h.append("static const int16_t *const PM_LPC_K[PM_LPC_ORDER] = {" + ", ".join(f"PM_LPC_K{i + 1}" for i in range(ORDER)) + "};\n")
    h.append(c_array("uint16_t", "PM_LPC_WORD", offs))
    h.append(c_array("uint8_t", "PM_LPC_DATA", data, 24))
    h.append("#endif\n")
    Path(args.out).write_text("".join(h))
    print(f"gen_pm_speech: {args.out}: {len(WORDS)} words from {where}: {nfr} frames, {len(data)} bytes of frames; "
          f"the chirp is flat within {20 * np.log10(spec.max() / spec.min()):.1f} dB from 125 Hz to 3.9 kHz")
    for (name, _, _), fr in zip(WORDS, coded):
        v = [pitches[f[2] - 1] for f in fr if f and f[2]]
        print(f"  {name:7s} {len(fr) * FRAME * 1000 // FS:4d} ms  " + "".join("." if f is None else "v" if f[2] else "s" for f in fr)
              + (f"  {FS / np.median(v):.0f} Hz" if v else ""))


if __name__ == "__main__":
    main()
