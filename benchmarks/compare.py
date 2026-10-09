#!/usr/bin/env python3
"""Run sequential, alternating-order collision comparisons; retain every raw tick."""
import argparse
import csv
import json
import math
import pathlib
import statistics
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("executable", type=pathlib.Path)
parser.add_argument("output", type=pathlib.Path, help="new directory for raw CSV and summary")
args = parser.parse_args()
executable = args.executable.resolve()
args.output.mkdir(parents=True, exist_ok=False)
records = []
commands = []
for scene, bodies in [("separated-spheres", 1000), ("separated-spheres", 5000),
                      ("separated-spheres", 10000), ("paired-spheres", 10000)]:
    samples = {"collision-naive": [], "collision-spatial": []}
    for trial in range(3):
        modes = list(samples) if trial % 2 == 0 else list(reversed(samples))
        for mode in modes:
            command = [str(executable), "--benchmark", mode, "--bodies", str(bodies),
                       "--steps", "10", "--warmup", "2", "--scene", scene, "--cell-size", "3"]
            filename = f"{scene}-{bodies}-{mode}-{trial + 1}.csv"
            commands.append({"argv": command, "stdout": filename})
            with (args.output / filename).open("w") as output:
                subprocess.run(command, stdout=output, check=True)
            with (args.output / filename).open() as source:
                rows = list(csv.DictReader(source))
            if len(rows) != 10:
                raise ValueError(f"Wrong row count: {filename}")
            all_pairs = bodies * (bodies - 1) // 2
            for tick, row in enumerate(rows):
                expected_contacts = bodies // 2 if scene == "paired-spheres" else 0
                if (int(row["schema"]) != 2 or int(row["step"]) != tick or
                    int(row["bodies"]) != bodies or row["benchmark"] != mode or
                    row["scene"] != scene or int(row["contacts"]) != expected_contacts or
                    int(row["correction_checks"]) != expected_contacts * 4):
                    raise ValueError(f"Unexpected workload/counter data: {filename}")
                pairs = int(row["candidate_pairs"])
                if not (0 <= pairs <= all_pairs and int(row["collision_checks"]) == pairs):
                    raise ValueError(f"Invalid candidate/check counts: {filename}")
                if mode == "collision-naive" and pairs != all_pairs:
                    raise ValueError(f"Naive reference skipped pairs: {filename}")
                reduction = 100 * (1 - pairs / all_pairs)
                if not math.isclose(float(row["pair_reduction_percent"]), reduction, abs_tol=1e-6):
                    raise ValueError(f"Incorrect reduction: {filename}")
                for name, value in row.items():
                    if name.endswith("_ms") and (not math.isfinite(float(value)) or float(value) < 0):
                        raise ValueError(f"Invalid duration: {filename}")
            samples[mode].extend(rows)
    result = {"scene": scene, "bodies": bodies, "algorithms": {}}
    for mode, rows in samples.items():
        result["algorithms"][mode] = {
            name: {"median": statistics.median(float(row[name]) for row in rows),
                   "min": min(float(row[name]) for row in rows),
                   "max": max(float(row[name]) for row in rows)}
            for name in rows[0] if name.endswith("_ms") or name in
            ("candidate_pairs", "collision_checks", "contacts", "pair_reduction_percent")}
    result["physics_speedup"] = (result["algorithms"]["collision-naive"]["physics_step_ms"]["median"] /
                                 result["algorithms"]["collision-spatial"]["physics_step_ms"]["median"])
    records.append(result)
    print(f"{scene}, {bodies}: {result['physics_speedup']:.2f}x median physics speedup", flush=True)
(args.output / "summary.json").write_text(json.dumps(records, indent=2) + "\n")
(args.output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
