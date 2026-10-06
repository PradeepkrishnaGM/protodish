#!/usr/bin/env python3
"""Follows the founding tag groups of a D0 run (initial_tag_groups > 1) through its lineage log.

usage: groups.py RUN_DIR [RUN_DIR ...] [--every 1000] [--groups 3]

RUN_DIR is a seed directory with lineage.bin and census.csv (batch.py --lineage). Every
ancestor belongs to the group whose starting tag is nearest its own; every daughter belongs
to her mother's group (matings between groups are counted). Every --every ticks it prints,
per group, the living cells and the range of their tags, and the smallest tag distance
between cells of different groups. A group is "separate" when that distance to every other
group is more than cluster_gap (0.1), the RULES.md cluster definition.
It reports when each group died out, when the groups first stopped being separate, and when
the census first showed fewer than 3 large clusters. Standard library only.
"""

import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import evolib  # noqa: E402
import lineage_to_csv as lin  # noqa: E402

GAP = 0.1


def tag_dist(a, b):
    d = abs(a - b)
    return d if d < 0.5 else 1.0 - d


def group_gaps(alive, n_groups):
    """Smallest tag distance between cells of each pair of groups (None if one is empty)."""
    items = sorted((tag, g) for g, tag in alive.values())
    gaps = {}
    if not items:
        return gaps
    # The closest cross-group pair is adjacent in sorted order once groups are merged; scan
    # neighbours around the circle, keeping the last tag seen from each group.
    last = {}
    for tag, g in items + [(t + 1.0, g) for t, g in items]:
        for h, t in last.items():
            if h != g:
                key = tuple(sorted((g, h)))
                d = tag - t
                if d <= 0.5 and (key not in gaps or d < gaps[key]):
                    gaps[key] = d
        last[g] = tag
    return gaps


def analyse(run_dir, every, n_groups):
    path = os.path.join(run_dir, "lineage.bin")
    with open(path, "rb") as f:
        data = f.read()
    rows = evolib.read_census(os.path.join(run_dir, "census.csv"))
    first_below_3 = next((r["tick"] for r in rows if r["tick"] > 0 and r["tag_clusters_large"] < 3),
                         None)

    alive = {}  # id -> (group, tag)
    group_of = {}
    centres = None
    cross_matings = 0
    died_out = {}  # group -> exact tick of its last death
    counts = {}
    first_not_separate = None
    next_report = every
    print(f"\n{run_dir}")
    print(f"  {'tick':>6}  " + "  ".join(f"group {g}: cells (tags)".ljust(30) for g in range(n_groups))
          + "  closest groups")

    def report(tick):
        nonlocal first_not_separate
        lo, hi = {}, {}
        for g in range(n_groups):
            tags = sorted(t for h, t in alive.values() if h == g)
            if tags:
                lo[g], hi[g] = tags[0], tags[-1]
        gaps = group_gaps(alive, n_groups)
        living = [g for g in range(n_groups) if counts.get(g, 0) > 0]
        separate = len(living) == n_groups and all(gaps.get(k, 1.0) > GAP for k in gaps)
        if not separate and first_not_separate is None:
            first_not_separate = tick
        closest = min(gaps.items(), key=lambda kv: kv[1]) if gaps else None
        cols = []
        for g in range(n_groups):
            cols.append((f"{counts.get(g, 0):>5} ({lo[g]:.2f}-{hi[g]:.2f})" if counts.get(g, 0) else "    0").ljust(30))
        cl = f"{closest[0][0]}-{closest[0][1]} {closest[1]:.3f}" if closest else "-"
        print(f"  {tick:>6}  " + "  ".join(cols) + f"  {cl}")

    i = lin.HEADER.size
    while i < len(data):
        t = data[i]
        if t == 1:
            _, kind, site, tick, cid, parent, parent2, *genes = lin.BIRTH.unpack_from(data, i)
            i += lin.BIRTH.size
            tag = genes[0]
            while tick >= next_report:
                report(next_report)
                next_report += every
            if kind == 3:  # ancestor
                if centres is None:
                    centres = []
                g = None
                for k, c in enumerate(centres):
                    if tag_dist(c, tag) < 1e-6:
                        g = k
                if g is None:
                    centres.append(tag)
                    g = len(centres) - 1
            else:
                g = group_of[parent]
                if parent2 and group_of.get(parent2, g) != g:
                    cross_matings += 1
            group_of[cid] = g
            alive[cid] = (g, tag)
            counts[g] = counts.get(g, 0) + 1
        else:
            _, cause, site, tick, cid, age = lin.DEATH.unpack_from(data, i)
            i += lin.DEATH.size
            while tick >= next_report:
                report(next_report)
                next_report += every
            g, _ = alive.pop(cid)
            counts[g] -= 1
            if counts[g] == 0:
                died_out[g] = tick
    last_tick = rows[-1]["tick"]
    while next_report <= last_tick:
        report(next_report)
        next_report += every
    starts = ", ".join(f"{c:.3f}" for c in (centres or []))
    print(f"  starting tags {starts}; matings between groups: {cross_matings}")
    print("  groups died out: " + (", ".join(f"group {g} at tick {t}" for g, t in sorted(died_out.items()))
                                   or "none"))
    print(f"  groups first not all separate (a group gone, or two within {GAP}): by tick {first_not_separate}")
    print(f"  census first below 3 large clusters: tick {first_below_3}")
    return died_out, first_not_separate, first_below_3


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("run_dirs", nargs="+")
    ap.add_argument("--every", type=int, default=1000)
    ap.add_argument("--groups", type=int, default=3)
    args = ap.parse_args()
    for d in args.run_dirs:
        analyse(d, args.every, args.groups)


if __name__ == "__main__":
    main()
