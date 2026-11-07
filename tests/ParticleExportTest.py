"""Exercise the installed CLI schema, warmup reset and both encoders."""
import csv
import io
import json
import math
import subprocess
import sys

args = [sys.argv[1], '--particles', '257', '--steps', '13', '--seed', '0']
def run(*opts):
    return subprocess.check_output(args + list(opts), text=True)

j = json.loads(run('--warmup','0'))
c, = csv.DictReader(io.StringIO(run('--layout','soa','--backend','parallel','--workers','3','--warmup','7','--format','csv')))
assert j.keys()==c.keys() and j['digest']==c['digest']
assert c['verification']=='exact-final-state' and int(c['worker_tasks'])==39
assert j['workers']==j['worker_tasks']==0 and j['layout']=='aos'
assert float(c['elapsed_ms'])>0 and math.isclose(float(c['ms_per_tick'])*13,float(c['elapsed_ms']))
assert int(c['owned_capacity_bytes']) >= 257*72
for bad in (['--layout','other'], ['--format','xml'], ['--workers','4'], ['--warmup','-1'], ['--steps','1']):
    assert subprocess.run(args+bad,capture_output=True).returncode!=0
