#!/usr/bin/env python3
"""Compare integration access/layouts with balanced order and full per-tick CSV retention."""
import argparse
import csv
import json
import math
import pathlib
import statistics
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable', type=pathlib.Path)
parser.add_argument('output', type=pathlib.Path, help='new output directory')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
layouts = ['lookup', 'dense', 'aos', 'soa']
commands, summary = [], []
for bodies in [10000, 100000]:
    for order in ['ordered', 'shuffled']:
        samples = {layout: [] for layout in layouts}
        reference = None
        for trial in range(4):
            for layout in layouts[trial:] + layouts[:trial]:
                command = [str(args.executable.resolve()), '--layout', layout, '--bodies', str(bodies),
                           '--steps', '100', '--warmup', '10', '--order', order]
                filename = f'{order}-{bodies}-{layout}-{trial+1}.csv'
                commands.append({'argv': command, 'stdout': filename})
                with (args.output / filename).open('w') as output:
                    subprocess.run(command, stdout=output, check=True)
                with (args.output / filename).open() as source:
                    rows = list(csv.DictReader(source))
                if len(rows) != 100:
                    raise ValueError(f'Wrong row count: {filename}')
                checksums = []
                for tick, row in enumerate(rows):
                    if (row['layout'] != layout or row['order'] != order or int(row['step']) != tick or
                        int(row['bodies']) != bodies or row['build_type'] != 'Release'):
                        raise ValueError(f'Unexpected workload/configuration: {filename}')
                    for key in ['gather_ms', 'kernel_ms', 'scatter_ms', 'total_ms', 'checksum']:
                        value = float(row[key])
                        if not math.isfinite(value) or (key.endswith('_ms') and value < 0):
                            raise ValueError(f'Invalid value: {filename}')
                    checksums.append(float(row['checksum']))
                if reference is None:
                    reference = checksums
                if not all(math.isclose(a, b, rel_tol=1e-8, abs_tol=1e-5) for a, b in zip(reference, checksums)):
                    raise ValueError(f'State checksum diverged: {filename}')
                samples[layout].extend(rows)
        result = {'bodies': bodies, 'order': order, 'layouts': {}}
        for layout, rows in samples.items():
            result['layouts'][layout] = {
                field: {'median': statistics.median(float(row[field]) for row in rows),
                        'min': min(float(row[field]) for row in rows),
                        'max': max(float(row[field]) for row in rows)}
                for field in ['gather_ms', 'kernel_ms', 'scatter_ms', 'total_ms']}
        summary.append(result)
        print(bodies, order, {layout: round(result['layouts'][layout]['total_ms']['median'], 6)
                             for layout in layouts}, flush=True)
(args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
(args.output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
