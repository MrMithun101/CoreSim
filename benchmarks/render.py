#!/usr/bin/env python3
"""Run serial, alternating renderer comparisons; retain every row and process metadata."""
import argparse
import csv
import hashlib
import json
import platform
import statistics
import subprocess
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--binary', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--modes', nargs='+', default=['individual', 'instanced'], choices=['individual', 'instanced'])
p.add_argument('--counts', nargs='+', type=int, default=[1000, 10000, 25000])
p.add_argument('--trials', type=int, default=3)
p.add_argument('--frames', type=int, default=30)
p.add_argument('--warmup', type=int, default=10)
p.add_argument('--geometry', choices=['cubes', 'mixed'], default='cubes')
a = p.parse_args()
if a.output.exists() or min(a.trials, a.frames, *a.counts) <= 0 or a.warmup < 0:
    p.error('Output must be new; counts, trials and frames positive; warmup nonnegative')
a.output.mkdir(parents=True)
binary = a.binary.resolve()
def command(*args):
    r = subprocess.run(args, text=True, capture_output=True)
    return r.stdout.strip() if r.returncode == 0 else r.stderr.strip()
metadata = dict(binary=str(binary), sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                revision=command('git', 'rev-parse', 'HEAD'), dirty=command('git', 'status', '--porcelain'),
                platform=platform.platform(), machine=platform.machine(),
                hardware=command('sysctl', 'machdep.cpu.brand_string', 'hw.memsize', 'hw.model', 'hw.physicalcpu', 'hw.logicalcpu') if platform.system() == 'Darwin' else command('lscpu'),
                options=vars(a) | {'binary': str(a.binary), 'output': str(a.output)}, invocations=[])
checksums = {}
summary = []
try:
    for count in a.counts:
        for trial in range(a.trials):
            for mode in a.modes[::1 if trial % 2 == 0 else -1]:
                stem = f'{a.geometry}-{count}-{mode}-{trial}'
                args = [str(binary), '--mode', mode, '--objects', str(count), '--frames', str(a.frames),
                        '--warmup', str(a.warmup), '--geometry', a.geometry]
                r = subprocess.run(args, text=True, capture_output=True, timeout=600)
                (a.output / (stem + '.csv')).write_text(r.stdout)
                (a.output / (stem + '.log')).write_text(r.stderr)
                metadata['invocations'].append(dict(command=args, returncode=r.returncode, file=stem))
                if r.returncode: raise RuntimeError(r.stderr)
                rows = list(csv.DictReader(r.stdout.splitlines()))
                assert len(rows) == a.frames
                for i, row in enumerate(rows):
                    assert row['schema'] == '1' and row['mode'] == mode and row['geometry'] == a.geometry
                    assert int(row['objects']) == count and int(row['sample']) == i
                    expected = count if mode == 'individual' else (2 if a.geometry == 'mixed' and count > 1 else 1)
                    assert int(row['draw_calls']) == expected
                    assert 0 < float(row['cpu_submission_ms']) <= float(row['frame_ms'])
                    assert abs(float(row['fps']) * float(row['frame_ms']) - 1000) < 1e-6
                    key = (count, a.geometry)
                    assert row['image_checksum'] == checksums.setdefault(key, row['image_checksum'])
                stats = {'objects': count, 'trial': trial, 'mode': mode}
                for field in ['cpu_submission_ms', 'frame_ms', 'fps']:
                    values = [float(row[field]) for row in rows]
                    for label, func in [('median', statistics.median), ('min', min), ('max', max)]:
                        stats[field + '_' + label] = func(values)
                summary.append(stats)
                print(stem, stats, flush=True)
finally:
    (a.output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    (a.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
