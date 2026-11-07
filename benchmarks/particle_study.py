#!/usr/bin/env python3
"""Run isolated CPU particle trials. Standard library only; timings come from C++."""
import argparse
import csv
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def command(args, cwd=None):
    return subprocess.check_output(args, text=True, cwd=cwd).strip()


def validate(row, case, options):
    count, layout, workers = case
    expected = dict(schema=1, workload='constant-acceleration-v1', particles=count,
                    layout=layout, backend='parallel' if workers else 'serial', workers=workers,
                    seed=options.seed, steps=options.steps, warmup=options.warmup,
                    worker_tasks=min(count, workers)*options.steps, verification='exact-final-state',
                    build_type='Release', sanitizer='none')
    for key, value in expected.items():
        if row.get(key) != value:
            raise ValueError(f'{key}: expected {value}, got {row.get(key)}')
    for key in ('elapsed_ms', 'ms_per_tick', 'updates_per_second'):
        if not (math.isfinite(row[key]) and row[key] > 0):
            raise ValueError(f'invalid {key}')
    if not (math.isclose(row['elapsed_ms']/options.steps, row['ms_per_tick']) and
            math.isclose(1000*count/row['ms_per_tick'], row['updates_per_second'])):
        raise ValueError('inconsistent throughput or tick time')
    peaks = [row['peak_process_rss_'+key+'_bytes'] for key in ('baseline', 'ready', 'after')]
    if not (all(x is None for x in peaks) or all(type(x) is int and x > 0 for x in peaks) and peaks == sorted(peaks)):
        raise ValueError('invalid process RSS measurements')
    if row['owned_capacity_bytes'] < 72*count:
        raise ValueError('particle buffer capacity below payload size')


def write_csv(path, rows):
    with path.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='new output directory')
    parser.add_argument('--counts', type=int, nargs='+', default=[1000, 10000, 100000])
    parser.add_argument('--workers', type=int, nargs='+', default=[1, 4, 8])
    parser.add_argument('--steps', type=int, default=1000)
    parser.add_argument('--warmup', type=int, default=60)
    parser.add_argument('--trials', type=int, default=5)
    parser.add_argument('--seed', type=int, default=7)
    parser.add_argument('--order-seed', type=int, default=19)
    o = parser.parse_args()
    for name, values, lo, hi in [('counts', o.counts, 1, 100000), ('workers', o.workers, 1, 64)]:
        if len(set(values)) != len(values) or any(not lo <= x <= hi for x in values):
            parser.error(f'{name} must be unique and in {lo}..{hi}')
    if not (1 <= o.steps <= 100000 and 0 <= o.warmup <= 10000 and 1 <= o.trials <= 100 and 0 <= o.seed < 2**32):
        parser.error('invalid steps, warmup, trials or seed')
    if o.output.exists():
        parser.error('output directory already exists')
    binary = o.binary.resolve(strict=True)
    git = ['git', '-c', 'core.trustctime=false', '-c', 'core.checkStat=minimal']
    meta = dict(schema=1, utc_start=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                binary_name=binary.name, binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                source_revision_at_run=command(git+['rev-parse','HEAD'], ROOT),
                source_dirty_at_run=bool(command(git+['status','--porcelain'], ROOT)),
                os=platform.platform(), architecture=platform.machine(), logical_cpus=os.cpu_count(),
                workload_seed=o.seed, order_seed=o.order_seed, counts=o.counts, workers=o.workers,
                steps=o.steps, warmup=o.warmup, trials=o.trials, python=platform.python_version())
    if platform.system() == 'Darwin':
        meta['cpu_model'] = command(['/usr/sbin/sysctl','-n','machdep.cpu.brand_string'])
        meta['physical_memory_bytes'] = int(command(['/usr/sbin/sysctl','-n','hw.memsize']))
    elif Path('/proc/cpuinfo').exists():
        meta['cpu_model'] = next((x.split(':',1)[1].strip() for x in Path('/proc/cpuinfo').read_text().splitlines() if x.startswith('model name')), platform.machine())
    db = binary.parent/'compile_commands.json'
    if db.exists():
        meta['compile_commands'] = [r.get('command', r.get('arguments')) for r in json.loads(db.read_text())
                                    if Path(r['file']).name in ('ParticleBenchmark.cpp','ParticleSimulation.cpp','SoASimulation.cpp','JobSystem.cpp')]
    cases = [(n, layout, w) for n in o.counts for layout in ('aos','soa') for w in [0]+o.workers]
    rng = random.Random(o.order_seed)
    rows, digests = [], {}
    for trial in range(1, o.trials+1):
        order = cases.copy()
        rng.shuffle(order)
        for n, layout, w in order:
            args = ['--particles',str(n),'--layout',layout,'--backend','parallel' if w else 'serial',
                    '--steps',str(o.steps),'--warmup',str(o.warmup),'--seed',str(o.seed)]
            if w:
                args += ['--workers',str(w)]
            row = json.loads(command([str(binary)]+args))
            validate(row, (n,layout,w), o)
            if digests.setdefault(n,row['digest']) != row['digest']:
                raise ValueError('digest changed across equivalent trials')
            rows.append(dict(trial=trial, sequence=len(rows)+1, **row))
    summary = []
    for n, layout, w in cases:
        group = [r for r in rows if (r['particles'],r['layout'],r['workers'])==(n,layout,w)]
        times = [r['ms_per_tick'] for r in group]
        peaks = [r['peak_process_rss_after_bytes'] for r in group]
        summary.append(dict(particles=n,layout=layout,workers=w,trials=len(times),median_ms=statistics.median(times),
                            min_ms=min(times),max_ms=max(times),median_updates_per_second=statistics.median(r['updates_per_second'] for r in group),
                            median_peak_rss_bytes=statistics.median(peaks) if all(p is not None for p in peaks) else None,
                            owned_capacity_bytes=group[0]['owned_capacity_bytes']))
    meta['utc_end'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    o.output.parent.mkdir(parents=True, exist_ok=True)
    # Publish only a completely validated study. Never overwrite existing evidence.
    with tempfile.TemporaryDirectory(dir=o.output.parent) as tmp:
        out = Path(tmp)/'study'; out.mkdir()
        for name, data in [('metadata',meta),('trials',rows),('summary',summary)]:
            (out/(name+'.json')).write_text(json.dumps(data,indent=2,allow_nan=False)+'\n')
        write_csv(out/'trials.csv', rows)
        write_csv(out/'summary.csv', summary)
        out.rename(o.output)
    print(f'{len(rows)} verified trials; results in {o.output}')

if __name__ == '__main__':
    main()
