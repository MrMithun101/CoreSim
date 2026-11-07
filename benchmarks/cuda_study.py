#!/usr/bin/env python3
"""Milestone 21: verified same-host CPU/CUDA study, or explicit CPU-only rehearsal."""
import argparse
import datetime as dt
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

from particle_study import ROOT, command, write_csv


def validate(row, backend, count, opt):
    expected = dict(schema=1, workload='constant-acceleration-v1', layout='aos', backend=backend,
                    particles=count, steps=opt.steps, seed=opt.seed, warmup=opt.warmup,
                    workers=opt.workers if backend == 'parallel' else 0, build_type='Release',
                    sanitizer='none', verified=True)
    if any(row.get(k) != v for k,v in expected.items()) or row['verified'] is not True:
        raise ValueError('inconsistent or unverified trial')
    if not math.isfinite(row['end_to_end_ms']) or row['end_to_end_ms'] <= 0:
        raise ValueError('invalid end-to-end time')
    if not math.isfinite(row['max_absolute_error']) or row['max_absolute_error'] < 0:
        raise ValueError('invalid numerical error')
    phases = [row[k] for k in ('upload_ms','kernel_ms','download_ms')]
    if backend == 'cuda':
        if (not all(isinstance(t,(int,float)) and math.isfinite(t) and t >= 0 for t in phases)
            or not row['gpu'] or row['device'] != opt.device or min(row[k] for k in ('runtime','toolkit','driver_api')) <= 0):
            raise ValueError('invalid CUDA timings or metadata')
    elif any(t is not None for t in phases) or row['reference_digest'] != row['actual_digest'] or row['max_absolute_error'] != 0:
        raise ValueError('invalid CPU parity or fabricated device timings')


def preflight(ctest, build, device):
    # Device tests exercise every visible GPU, plus a runner/profile smoke test on device zero.
    args = [ctest, '--test-dir', str(build), '-L', '^cuda$']
    tests = json.loads(command(args+['--show-only=json-v1']))['tests']
    names = {t['name'] for t in tests if not any(p['name']=='DISABLED' and p['value'] for p in t['properties'])}
    if not {'coresim.cuda_device','coresim.cuda_runner','coresim.cuda_profile'} <= names:
        raise ValueError('configure with BUILD_TESTING=ON, CORESIM_ENABLE_CUDA=ON and CORESIM_CUDA_DEVICE_TESTS=ON; build all targets')
    log = command(args+['--no-tests=error','--output-on-failure'])
    driver = Path('/proc/driver/nvidia/version')
    driver_version = driver.read_text().strip() if driver.is_file() else command(
        ['nvidia-smi','--query-gpu=driver_version','--format=csv,noheader'])
    if not driver_version:
        raise ValueError('missing NVIDIA driver version')
    return dict(tests=sorted(names), log=log, driver_version=driver_version, selected_device=device)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',required=True,type=Path,help='one Release build for all backends')
    p.add_argument('--output',required=True,type=Path,help='new result directory')
    p.add_argument('--ctest',default='ctest')
    p.add_argument('--cpu-only',action='store_true',help='rehearsal only: do not produce GPU results or speedups')
    p.add_argument('--steps',type=int,default=120); p.add_argument('--warmup',type=int,default=60)
    p.add_argument('--trials',type=int,default=5); p.add_argument('--workers',type=int,default=4)
    p.add_argument('--seed',type=int,default=7); p.add_argument('--order-seed',type=int,default=21)
    p.add_argument('--device',type=int,default=0)
    o=p.parse_args()
    if not (1<=o.steps<=10000 and 1<=o.warmup<=10000 and 1<=o.workers<=64 and 1<=o.trials<=100 and
            0<=o.seed<2**32 and 0<=o.device<2**31):
        p.error('invalid study bounds')
    if o.output.exists(): p.error('output already exists')
    build=o.build.resolve(strict=True); binary=(build/'coresim_particle_profile').resolve(strict=True)
    gate=None if o.cpu_only else preflight(o.ctest,build,o.device)
    counts=(1000,10000,100000); backends=['serial','parallel']+([] if o.cpu_only else ['cuda'])
    git=['git','-c','core.trustctime=false','-c','core.checkStat=minimal']
    meta=dict(schema=1,mode='cpu-only-rehearsal' if o.cpu_only else 'hardware-study',
              start_utc=dt.datetime.now(dt.timezone.utc).isoformat(),host=platform.platform(),architecture=platform.machine(),
              logical_cpus=os.cpu_count(), source_revision_at_run=command(git+['rev-parse','HEAD'],ROOT),
              source_dirty_at_run=bool(command(git+['status','--porcelain'],ROOT)),
              binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),gate=gate,
              counts=counts,backends=backends,steps=o.steps,warmup=o.warmup,trials=o.trials,workers=o.workers,
              seed=o.seed,order_seed=o.order_seed,cuda_visible_devices=os.environ.get('CUDA_VISIBLE_DEVICES'))
    if Path('/proc/cpuinfo').exists():
        meta['cpu_model']=next((s.split(':',1)[1].strip() for s in Path('/proc/cpuinfo').read_text().splitlines() if s.startswith('model name')), platform.machine())
    elif platform.system()=='Darwin':
        try:
            meta['cpu_model']=command(['/usr/sbin/sysctl','-n','machdep.cpu.brand_string'])
        except subprocess.CalledProcessError:
            meta['cpu_model']=platform.machine()+' (model unavailable)'
    db=build/'compile_commands.json'
    if db.exists():
        meta['compile_commands']=[r.get('command',r.get('arguments')) for r in json.loads(db.read_text()) if Path(r['file']).name in ('CudaParticles.cu','ParticleSimulation.cpp','profile_main.cpp')]
    rng=random.Random(o.order_seed); rows=[]; digests={}
    for trial in range(1,o.trials+1):
        cases=[(n,b) for n in counts for b in backends]; rng.shuffle(cases)
        for n,b in cases:
            args=['--backend',b,'--particles',str(n),'--steps',str(o.steps),'--warmup',str(o.warmup),'--seed',str(o.seed)]
            if b=='parallel': args+=['--workers',str(o.workers)]
            if b=='cuda': args+=['--device',str(o.device)]
            row=json.loads(command([str(binary)]+args)); validate(row,b,n,o)
            if digests.setdefault(n,row['reference_digest'])!=row['reference_digest']:
                raise ValueError('reference digest differs for equivalent inputs')
            rows.append(dict(trial=trial, sequence=len(rows)+1, **row))
    summaries=[]; report=['# '+('CPU-only rehearsal — no GPU measurements' if o.cpu_only else 'CPU/CUDA hardware study'),'',
        f"{meta.get('cpu_model',meta['architecture'])}; {o.trials} trials, {o.steps} ticks per timed batch, {o.warmup} warmup ticks, seed {o.seed}.",
        '', 'Times are milliseconds per batch. Device intervals are medians; dashes mean not applicable.', '',
        '| Particles | Backend | End-to-end median (min–max) | Upload | Kernel batch | Download | Max abs error |',
        '|---:|---|---:|---:|---:|---:|---:|']
    if gate:
        gpu=next(r for r in rows if r['backend']=='cuda')
        report[3:3]=['',f"GPU: {gpu['gpu']}, compute capability {gpu['capability']}; CUDA runtime/toolkit: {gpu['runtime']}/{gpu['toolkit']}. Installed driver: {gate['driver_version'].splitlines()[0]}. Hardware preflight passed."]
    for n in counts:
        medians={}
        for b in backends:
            group=[r for r in rows if r['particles']==n and r['backend']==b]
            times=[r['end_to_end_ms'] for r in group]; medians[b]=statistics.median(times)
            phases={k:(statistics.median(r[k] for r in group) if b=='cuda' else None) for k in ('upload_ms','kernel_ms','download_ms')}
            err=max(r['max_absolute_error'] for r in group)
            summaries.append(dict(particles=n,backend=b,median_ms=medians[b],min_ms=min(times),max_ms=max(times),**phases,max_absolute_error=err))
            cells=' | '.join('—' if t is None else f'{t:.5g}' for t in phases.values())
            report.append(f'| {n:,} | {b} | {medians[b]:.5g} ({min(times):.5g}–{max(times):.5g}) | {cells} | {err:.5g} |')
    if gate:
        report+=['','End-to-end ratios (CPU median divided by CUDA median; below 1 means CPU was faster):','']
        for n in counts:
            times={r['backend']:r['median_ms'] for r in summaries if r['particles']==n}
            report.append(f"- {n:,}: serial/CUDA {times['serial']/times['cuda']:.3f}×; {o.workers}-worker/CUDA {times['parallel']/times['cuda']:.3f}×.")
    meta['end_utc']=dt.datetime.now(dt.timezone.utc).isoformat()
    o.output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=o.output.parent) as tmp:
        out=Path(tmp)/'study'; out.mkdir()
        for name,obj in [('metadata',meta),('trials',rows),('summary',summaries)]:
            (out/(name+'.json')).write_text(json.dumps(obj,indent=2,allow_nan=False)+'\n')
        keys=dict.fromkeys(k for r in rows for k in r)
        write_csv(out/'trials.csv',[{k:r.get(k) for k in keys} for r in rows]);write_csv(out/'summary.csv',summaries)
        (out/'report.md').write_text('\n'.join(report)+'\n');out.rename(o.output)
    print(f"{len(rows)} verified trials: {meta['mode']}; {o.output}")

if __name__=='__main__':
    try:
        main()
    except (OSError,ValueError,subprocess.CalledProcessError) as error:
        raise SystemExit(f'Study aborted: {error}') from None
