#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Measure each core preset through the actual mixer, FX and master in a fresh process."""
import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys

probe = sys.argv[1] if len(sys.argv) > 1 else 'build/host/core_mix_probe'
report = Path(sys.argv[2] if len(sys.argv) > 2 else 'build/host/core-mix-health.json')
engines = (0, 3, 4, 5, 6, 12, 13, 7, 2, 8, 9, 10)


def measure(case):
    p = subprocess.run([probe, *map(str, case)], capture_output=True, text=True, check=True)
    r = json.loads(p.stdout)
    if r['rail_samples'] or not 0 < r['peak'] < 1:
        raise AssertionError(r)
    r["case"] = list(case)
    return r


cases = [(e, p, n) for e in engines for p in range(8) for n in (1, 8)]
cases += [(7, 0, n, 0, bank, pos) for bank in range(5) for pos in (0, 42, 85, 127) for n in (1, 8)]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    results = list(pool.map(measure, cases))
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps(results, indent=2) + '\n')
print(f"core mix: {len(results)} independent runs, zero PCM rail hits, max peak {max(r['peak'] for r in results):.6f} FS")
