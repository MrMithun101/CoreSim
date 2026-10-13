#!/usr/bin/env python3
"""Retain balanced worker-scaling and grain experiments, validating every CSV tick."""
import argparse
import csv
import json
import math
import pathlib
import platform
import statistics
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable', type=pathlib.Path)
parser.add_argument('output', type=pathlib.Path, help='new output directory')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
metadata = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
            'platform': platform.platform(), 'machine': platform.machine(),
            'method': 'Five trials; 10 warmup + 50 measured ticks per process; rotated configuration order.'}
(args.output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
commands, summary = [], []
workloads = [('free-flight', n, order) for n in [1000, 10000, 100000, 1000000]
             for order in ['ordered', 'shuffled']]
workloads += [('integration', 1000000, 'ordered')]
workloads += [('collisions', 10000, order) for order in ['ordered', 'shuffled']]
experiments = [(w,n,o,[(p,16384) for p in [0,1,2,4,8]]) for w,n,o in workloads]
experiments += [('free-flight',100000,'ordered',[(4,g) for g in [1,1024,16384,65536,1000000]])]
for experiment, (workload, bodies, order, configs) in enumerate(experiments):
    samples = {config: [] for config in configs}
    reference = None
    for trial in range(5):
        configs_rotated = configs[trial:] + configs[:trial]
        for workers, grain in configs_rotated:
            command = [str(args.executable.resolve()), '--workload', workload, '--bodies', str(bodies),
                       '--workers', str(workers), '--grain', str(grain), '--order', order,
                       '--steps', '50', '--warmup', '10']
            filename = f'{experiment}-{workload}-{order}-{bodies}-w{workers}-g{grain}-{trial+1}.csv'
            commands.append({'argv': command, 'stdout': filename})
            # Save incrementally so even interrupted or failed runs retain their invocation history.
            (args.output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
            with (args.output / filename).open('w') as output:
                subprocess.run(command, stdout=output, check=True)
            with (args.output / filename).open() as source:
                rows = list(csv.DictReader(source))
            if len(rows) != 50:
                raise ValueError(f'Wrong row count: {filename}')
            states = []
            for tick, row in enumerate(rows):
                expected = {'schema':'1', 'workload':workload, 'order':order, 'build_type':'Release',
                            'bodies':str(bodies), 'workers':str(workers), 'grain':str(grain),
                            'step':str(tick), 'warmup':'10'}
                if any(row[key] != value for key,value in expected.items()):
                    raise ValueError(f'Wrong configuration: {filename}')
                for key in ['total_ms','collision_ms','checksum']:
                    value = float(row[key])
                    if not math.isfinite(value) or (key.endswith('_ms') and value < 0):
                        raise ValueError(f'Invalid value: {filename}')
                if float(row['collision_ms']) > float(row['total_ms']):
                    raise ValueError(f'Invalid timing boundaries: {filename}')
                states.append((row['checksum'], row['candidates'], row['contacts']))
            if reference is None:
                reference = states
            if states != reference:
                raise ValueError(f'State or collision counters diverged: {filename}')
            samples[(workers,grain)].extend(rows)
    result = {'workload': workload, 'bodies': bodies, 'order': order, 'configs': []}
    for (workers,grain), rows in samples.items():
        result['configs'].append({'workers':workers, 'grain':grain, **{
            field: {'median':statistics.median(float(r[field]) for r in rows),
                    'min':min(float(r[field]) for r in rows), 'max':max(float(r[field]) for r in rows),
                    'trial_medians':[statistics.median(float(r[field]) for r in rows[t:t+50])
                                     for t in range(0,250,50)]}
            for field in ['total_ms','collision_ms']}})
    summary.append(result)
    (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(workload,bodies,order,[(c['workers'],c['grain'],round(c['total_ms']['median'],6))
                               for c in result['configs']],flush=True)
