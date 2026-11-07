"""Exercise real CPU trials, report export, and refusal to publish an ungated GPU study."""
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile

script=Path(__file__).resolve().parents[1]/'benchmarks/cuda_study.py'
with tempfile.TemporaryDirectory() as tmp:
    output=Path(tmp)/'results'
    args=[sys.executable,str(script),'--build',sys.argv[1],'--ctest',sys.argv[2],
          '--output',str(output),'--steps','2','--warmup','1','--trials','1','--workers','2']
    fail=subprocess.run(args,capture_output=True,text=True)
    assert fail.returncode and 'CORESIM_CUDA_DEVICE_TESTS' in fail.stderr and not output.exists()
    subprocess.run(args+['--cpu-only'],check=True)
    rows=json.loads((output/'trials.json').read_text())
    assert len(rows)==6 and all(r['verified'] and r['kernel_ms'] is None for r in rows)
    with (output/'trials.csv').open() as f: assert len(list(csv.DictReader(f)))==6
    assert json.loads((output/'metadata.json').read_text())['mode']=='cpu-only-rehearsal'
    assert 'CPU-only rehearsal' in (output/'report.md').read_text()
    assert len(json.loads((output/'summary.json').read_text()))==6
    assert subprocess.run(args+['--cpu-only'],capture_output=True).returncode!=0
