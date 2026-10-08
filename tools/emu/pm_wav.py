#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A WAV of the PurpleMonkey emulator (16-bit stereo) in numbers, for tools/emu/test_pm.sh:

  tools/emu/pm_wav.py FILE.wav [TAIL_SECONDS]

prints: peak (of 32767), samples at full scale (clipping), the largest step between two samples, the rms, and the
peak of the last TAIL_SECONDS (default 1; what is left sounding at the end).
"""
import array
import sys
import wave

w = wave.open(sys.argv[1])
tail = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
d = array.array("h", w.readframes(w.getnframes()))
fs = w.getframerate()
left = d[0::2]
peak = max((abs(x) for x in d), default=0)
full = sum(1 for x in d if abs(x) >= 32767)
jump = max((abs(left[i] - left[i - 1]) for i in range(1, len(left))), default=0)
rms = (sum(x * x for x in d) / max(1, len(d))) ** 0.5
t = d[-int(tail * fs) * 2:]
print(f"peak {peak} full_scale {full} max_step {jump} rms {rms:.0f} tail_peak {max((abs(x) for x in t), default=0)} "
      f"seconds {len(left) / fs:.1f}")
