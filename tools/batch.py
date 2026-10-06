#!/usr/bin/env python3
"""Runs several params files x seeds in parallel, one single-threaded evolve process per job.

usage: batch.py --params FILE.params [FILE.params ...] [--seeds 1-6] [--ticks 50000]
                [-j 6] [--out runs] [--lineage] [--plot] [--force]

Each run goes to OUT/<params name>/seed<N>/:
  census.csv   the census (written as census.csv.tmp until the run finishes)
  params.txt   the effective params (evolve --dump-params)
  stderr.txt   evolve's messages (speed, extinction)
  lineage.bin  only with --lineage (about 40-100 MB per 50,000-tick run)
  done.txt     exit status and wall time; a run with done.txt and status 0 is skipped
               unless --force is given, so an interrupted batch can be resumed.
At the end, the balance targets are checked for every run and summarised per params file
in OUT/<params name>/summary.txt. With --plot, OUT/<params name>/overview.png shows all
seeds of that params file (needs the .venv; see requirements.txt).
Each run is deterministic: the same params and seed always give the same census.
"""

import argparse
import concurrent.futures as cf
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import evolib  # noqa: E402


def parse_seeds(text):
    seeds = []
    for part in text.split(","):
        if "-" in part:
            a, b = part.split("-")
            seeds.extend(range(int(a), int(b) + 1))
        else:
            seeds.append(int(part))
    return seeds


def exp_name(params_path):
    return os.path.splitext(os.path.basename(params_path))[0]


def is_done(run_dir):
    p = os.path.join(run_dir, "done.txt")
    if not os.path.exists(p):
        return False
    with open(p) as f:
        return f.readline().strip() == "status 0"


def run_one(evolve, params_path, seed, ticks, run_dir, lineage):
    os.makedirs(run_dir, exist_ok=True)
    with open(os.path.join(run_dir, "params.txt"), "w") as f:
        subprocess.run([evolve, "--params", params_path, "--dump-params"], stdout=f, check=True)
    census = os.path.join(run_dir, "census.csv")
    cmd = [evolve, "--params", params_path, "--seed", str(seed), "--ticks", str(ticks),
           "--census", census + ".tmp"]
    if lineage:
        cmd += ["--lineage", os.path.join(run_dir, "lineage.bin")]
    start = time.monotonic()
    with open(os.path.join(run_dir, "stderr.txt"), "w") as err:
        status = subprocess.run(cmd, stderr=err, stdout=subprocess.DEVNULL).returncode
    secs = time.monotonic() - start
    if status == 0:
        os.replace(census + ".tmp", census)
    with open(os.path.join(run_dir, "done.txt"), "w") as f:
        f.write(f"status {status}\nseconds {secs:.1f}\nparams {os.path.abspath(params_path)}\n"
                f"seed {seed}\nticks {ticks}\n")
    return status, secs


def summarise(out_dir, names, seeds, ticks, plot):
    print()
    print(evolib.SUMMARY_HEADER)
    for name in names:
        lines = [evolib.SUMMARY_HEADER]
        counts = {"lasts": 0, "diverse": 0, "both_sides": 0, "cycles": 0, "balanced": 0}
        censuses = []
        for seed in seeds:
            census = os.path.join(out_dir, name, f"seed{seed}", "census.csv")
            if not os.path.exists(census):
                lines.append(f"{name}/seed{seed}: no census (run failed?)")
                print(lines[-1])
                continue
            censuses.append(census)
            rows = evolib.read_census(census)
            r = evolib.check_balance(rows, evolib.year_length_for(census), ticks)
            for k in counts:
                counts[k] += bool(r[k])
            line = evolib.summary_line(f"{name}/seed{seed}", r)
            lines.append(line)
            print(line)
        n = len(censuses)
        total = (f"{name}: {n} runs; lasts {counts['lasts']}, diverse {counts['diverse']}, "
                 f"both sides {counts['both_sides']}, cycles {counts['cycles']}, "
                 f"balanced {counts['balanced']}")
        lines.append(total)
        print(total)
        print()
        with open(os.path.join(out_dir, name, "summary.txt"), "w") as f:
            f.write("\n".join(lines) + "\n")
        if plot and censuses:
            py = os.path.join(ROOT, ".venv", "bin", "python")
            subprocess.run([py if os.path.exists(py) else sys.executable,
                            os.path.join(HERE, "plot_run.py"), *censuses,
                            "--out", os.path.join(out_dir, name, "overview.png"),
                            "--title", f"{name} ({n} seeds)"], check=False)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--params", nargs="+", required=True, help="params files, one experiment each")
    ap.add_argument("--seeds", default="1-6", help="e.g. 1-6 or 1,4,9 (default 1-6)")
    ap.add_argument("--ticks", type=int, default=evolib.TARGET_TICKS)
    ap.add_argument("-j", "--jobs", type=int, default=6, help="parallel runs (default 6)")
    ap.add_argument("--out", default=os.path.join(ROOT, "runs"))
    ap.add_argument("--evolve", default=os.path.join(ROOT, "build", "evolve"))
    ap.add_argument("--lineage", action="store_true", help="also write lineage logs")
    ap.add_argument("--plot", action="store_true", help="draw overview.png per params file")
    ap.add_argument("--force", action="store_true", help="rerun finished runs")
    args = ap.parse_args()

    names = [exp_name(p) for p in args.params]
    if len(set(names)) != len(names):
        sys.exit("params files must have distinct names")
    seeds = parse_seeds(args.seeds)
    jobs = []
    for path, name in zip(args.params, names):
        for seed in seeds:
            run_dir = os.path.join(args.out, name, f"seed{seed}")
            if args.force or not is_done(run_dir):
                jobs.append((path, name, seed, run_dir))
    print(f"{len(jobs)} runs to do ({len(names) * len(seeds) - len(jobs)} already done), "
          f"{args.jobs} at a time, {args.ticks} ticks each")

    start = time.monotonic()
    failed = 0
    with cf.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {pool.submit(run_one, args.evolve, p, s, args.ticks, d, args.lineage): (n, s)
                   for p, n, s, d in jobs}
        for k, fut in enumerate(cf.as_completed(futures), 1):
            name, seed = futures[fut]
            status, secs = fut.result()
            failed += status != 0
            print(f"[{k}/{len(jobs)}] {name}/seed{seed}: "
                  f"{'ok' if status == 0 else f'FAILED ({status})'} in {secs:.0f} s", flush=True)
    if jobs:
        print(f"batch wall time {time.monotonic() - start:.0f} s")

    summarise(args.out, names, seeds, args.ticks, args.plot)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
