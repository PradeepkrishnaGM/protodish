#!/usr/bin/env python3
"""Tag clusters before, during and after each epidemic, against virus-free control windows.

usage: epidemics.py RUN_DIR [RUN_DIR ...] [--min-peak 50] [--tail 1000] [--list]
                     [--control RUN_DIR ...]

RUN_DIR is an experiment directory (runs/<name>), whose seed*/census.csv files are read.
An epidemic is a maximal stretch of consecutive census rows with at least one infected
cell (the M5 definition, at census resolution). For each epidemic whose peak reaches
--min-peak infected cells:
  before   clusters of >= cluster_min_size cells ("large") in the last row before it
  during   the lowest and highest large-cluster count while it lasts
  after    the large-cluster count --tail ticks after its last infected row
  thinning the lowest population during it, as a share of the population before it
The control, for each epidemic, is every window in the same run with the same length and
tail that starts at the same time of year (within 200 ticks) and has no infected cell; each
epidemic's controls are averaged, then the averages are averaged. With --control, the
windows come from those experiments instead (for runs where a virus is almost always present). It shows how much the
cluster count and population move without viruses (seasons, drift). The question is whether epidemics split lineages (more clusters during or after)
or only thin them (fewer cells, same or fewer clusters).
Standard library only.
"""

import argparse
import glob
import os
import statistics as st
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import evolib  # noqa: E402

PHASE_TOLERANCE = 200  # ticks: control windows start at the same time of year, within this


def epidemics(rows):
    """(first, last) row indices of each stretch of rows with infected > 0."""
    out, start = [], None
    for i, r in enumerate(rows):
        if r["infected"] > 0 and start is None:
            start = i
        elif r["infected"] == 0 and start is not None:
            out.append((start, i - 1))
            start = None
    if start is not None:
        out.append((start, len(rows) - 1))
    return out


def window_stats(rows, before, first, last, after):
    lg = [rows[i]["tag_clusters_large"] for i in range(first, last + 1)]
    cells_before = rows[before]["cells"]
    low = min(rows[i]["cells"] for i in range(first, last + 1))
    return {
        "before": rows[before]["tag_clusters_large"],
        "during_min": min(lg),
        "during_max": max(lg),
        "after": rows[after]["tag_clusters_large"],
        "thinning": low / cells_before if cells_before else float("nan"),
    }


def shares(events):
    n = len(events)
    if n == 0:
        return None
    f = lambda pred: sum(1 for e in events if pred(e)) / n  # noqa: E731
    return {
        "n": n,
        "before": st.mean(e["before"] for e in events),
        "during_max": st.mean(e["during_max"] for e in events),
        "after": st.mean(e["after"] for e in events),
        "rise_during": f(lambda e: e["during_max"] > e["before"]),
        "after_up": f(lambda e: e["after"] > e["before"]),
        "after_same": f(lambda e: e["after"] == e["before"]),
        "after_down": f(lambda e: e["after"] < e["before"]),
        "thinning": st.median(e["thinning"] for e in events),
    }


def mean_shares(groups):
    """Average of per-epidemic control shares, so each epidemic weighs the same."""
    per = [shares(g) for g in groups if g]
    if not per:
        return None
    out = {k: st.mean(p[k] for p in per) for k in per[0] if k != "n"}
    out["n"] = len(per)
    return out


def matched_controls(rows, infected_rows, first, length, tail, per_year, phase_tol):
    """Virus-free windows in `rows` with this length and tail, starting at the same time of
    year as row `first` (within phase_tol rows)."""
    out = []
    for b in range(0, len(rows) - length - tail - 1):
        d = (b + 1 - first) % per_year
        if min(d, per_year - d) > phase_tol:
            continue
        cf, cl = b + 1, b + length
        if any(infected_rows[b:cl + tail + 1]):
            continue
        out.append(window_stats(rows, b, cf, cl, cl + tail))
    return out


def load_runs(run_dir):
    runs = []
    for census in sorted(glob.glob(os.path.join(run_dir, "seed*", "census.csv"))):
        rows = evolib.read_census(census)
        runs.append((census, rows, [r["infected"] > 0 for r in rows]))
    return runs


def analyse(run_dir, min_peak, tail_ticks, show_list, control_runs):
    events, controls, endemic, small = [], [], [], 0
    for census, rows, infected_rows in load_runs(run_dir):
        seed = os.path.basename(os.path.dirname(census))
        interval = rows[1]["tick"] - rows[0]["tick"]
        tail = max(1, tail_ticks // interval)
        eps = epidemics(rows)
        yl = evolib.year_length_for(census)
        phase_tol = max(1, PHASE_TOLERANCE // interval)
        for first, last in eps:
            peak = max(rows[i]["infected"] for i in range(first, last + 1))
            if peak < min_peak:
                small += 1
                continue
            if last - first + 1 > len(rows) / 2:
                endemic.append((seed, rows[first]["tick"], rows[last]["tick"], peak))
                continue
            after = last + tail
            if first == 0 or after >= len(rows):
                continue  # no clean before or after
            e = window_stats(rows, first - 1, first, last, after)
            e.update(seed=seed, start=rows[first]["tick"], end=rows[last]["tick"], peak=peak,
                     next_epidemic_in_tail=any(infected_rows[last + 1:after + 1]))
            events.append(e)
            # Control windows for this epidemic: same length, starting at the same time of
            # year (within PHASE_TOLERANCE ticks), with no infected cell from the row before
            # to the end of the tail; from the same run, or from the --control runs.
            # Each epidemic weighs the same in the control average.
            length = last - first + 1
            per_year = yl // interval
            sources = control_runs or [(census, rows, infected_rows)]
            matched = []
            for _, crows, cinf in sources:
                matched += matched_controls(crows, cinf, first, length, tail, per_year, phase_tol)
            if matched:
                controls.append(matched)
    name = os.path.basename(os.path.normpath(run_dir))
    print(f"\n{name}: {len(events)} epidemics with peak >= {min_peak} infected "
          f"({small} smaller ones skipped, {len(endemic)} endemic stretches)")
    for seed, a, b, peak in endemic:
        print(f"  endemic: {seed} ticks {a}-{b}, peak {peak} infected (no before/after)")
    if show_list:
        print(f"  {'seed':<6}{'ticks':>14}{'peak':>6}  large clusters before -> during (min-max) -> "
              f"after  lowest cells")
        for e in events:
            flag = "  (another epidemic in the tail)" if e["next_epidemic_in_tail"] else ""
            print(f"  {e['seed']:<6}{e['start']:>7}-{e['end']:<6}{e['peak']:>6}  {e['before']} -> "
                  f"{e['during_min']}-{e['during_max']} -> {e['after']}"
                  f"   {e['thinning']:.0%}{flag}")
    where = "in the --control runs" if control_runs else "in the same runs"
    print(f"  control: {len(controls)} of {len(events)} epidemics have season-matched "
          f"virus-free windows {where}")
    for label, s in (("epidemics", shares(events)), ("control (no virus)", mean_shares(controls))):
        if s is None:
            print(f"  {label}: none")
            continue
        print(f"  {label:<19} n={s['n']:<4} large clusters before {s['before']:.2f}, max during "
              f"{s['during_max']:.2f}, after {s['after']:.2f} | rise during {s['rise_during']:.0%}, "
              f"after up/same/down {s['after_up']:.0%}/{s['after_same']:.0%}/{s['after_down']:.0%} | "
              f"median lowest cells {s['thinning']:.0%} of before")
    return events, controls


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("run_dirs", nargs="+")
    ap.add_argument("--min-peak", type=int, default=50)
    ap.add_argument("--tail", type=int, default=1000, help="ticks after the epidemic (default 1000)")
    ap.add_argument("--list", action="store_true", help="list every epidemic")
    ap.add_argument("--control", nargs="+", default=[],
                    help="experiment dirs to draw control windows from instead of the same runs")
    args = ap.parse_args()
    control_runs = [r for d in args.control for r in load_runs(d)]
    for d in args.run_dirs:
        analyse(d, args.min_peak, args.tail, args.list, control_runs)


if __name__ == "__main__":
    main()
