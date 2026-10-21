#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Keep old patches' tables exact and every generated mip spectrally bounded."""
import hashlib
import importlib.util
import math
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1]
path = root / 'firmware/engines/tools/gen_wave_bank.py'
spec = importlib.util.spec_from_file_location('wavegen', path)
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)
waves = gen.tables()
assert [b[0] for b in gen.BANKS] == ['BASIC', 'COLOR', 'PPG', 'ESQ', 'VS']
assert len(waves) == 20
old = [v for levels in waves[:8] for samples in levels for v in samples]
assert hashlib.sha256(struct.pack('<' + 'h' * len(old), *old)).hexdigest() == \
    'ae122e67398006e389d0ad64448b17c3c28ef20f7808f7669172755509e971ff'
assert gen.generate() == (root / 'firmware/engines/src/wave_bank.h').read_text()
assert len({tuple(levels[0]) for levels in waves}) == 20
basis = [[complex(math.cos(2*math.pi*k*i/256), math.sin(2*math.pi*k*i/256))
          for i in range(256)] for k in range(1, 129)]
for w, levels in enumerate(waves):
    for cap, samples in zip(gen.CAPS, levels):
        assert len(samples) == 256 and max(map(abs, samples)) == 28000
        assert sum(samples) == 0
        power = [abs(sum(v*z for v, z in zip(samples, row)))**2 for row in basis]
        assert sum(power[cap:]) < sum(power)*1e-8, (w, cap)
print('wave banks: 20 distinct waves, 80 bounded mips, 40 KiB, old eight tables unchanged')
