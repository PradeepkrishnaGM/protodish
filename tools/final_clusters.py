#!/usr/bin/env python3
"""Describes the large tag clusters alive at the end of runs, from their lineage logs.

usage: final_clusters.py SEED_DIR [SEED_DIR ...] [--all] [--min-size 10]

SEED_DIR holds lineage.bin and census.csv (batch.py --lineage). The cells alive at the end
are rebuilt from the log, split into tag clusters as in the census (gaps wider than 0.1),
and every cluster of at least --min-size cells is described: size, tag range, mean diet,
photosynthesis and harvest, and its home rows (the shortest arc of rows, around the
wrapping grid, that holds 80% of its cells) with its mean latitude. Only runs whose final
census row has two or more large clusters are described, unless --all is given.
Standard library only.
"""

import argparse
import math
import os
import statistics as st
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import evolib  # noqa: E402
import lineage_to_csv as lin  # noqa: E402

GENE_INDEX = {g: i for i, g in enumerate(evolib.GENE_RANGES)}
GAP = 0.1
HOME_SHARE = 0.8


def alive_at_end(path):
    with open(path, "rb") as f:
        data = f.read()
    alive = {}
    i = lin.HEADER.size
    while i < len(data):
        if data[i] == 1:
            _, kind, site, tick, cid, parent, parent2, *genes = lin.BIRTH.unpack_from(data, i)
            alive[cid] = (site, genes)
            i += lin.BIRTH.size
        else:
            _, cause, site, tick, cid, age = lin.DEATH.unpack_from(data, i)
            alive.pop(cid, None)
            i += lin.DEATH.size
    return list(alive.values())


def clusters(cells):
    cells = sorted(cells, key=lambda c: c[1][0])
    tags = [c[1][0] for c in cells]
    n = len(tags)
    if n == 0:
        return []
    cuts = [i for i in range(1, n) if tags[i] - tags[i - 1] > GAP]
    if (1.0 - tags[-1]) + tags[0] > GAP:
        cuts = [0] + cuts
    if not cuts:
        return [cells]
    out = []
    for k, a in enumerate(cuts):
        b = cuts[k + 1] if k + 1 < len(cuts) else cuts[0] + n
        out.append([cells[j % n] for j in range(a, b)])
    return out


def home_rows(rows, height):
    """Shortest circular arc of rows holding HOME_SHARE of the cells: (first, last)."""
    counts = [0] * height
    for r in rows:
        counts[r] += 1
    need = math.ceil(HOME_SHARE * len(rows))
    best = None
    for start in range(height):
        total = 0
        for length in range(1, height + 1):
            total += counts[(start + length - 1) % height]
            if total >= need:
                if best is None or length < best[1]:
                    best = (start, length)
                break
    start, length = best
    return start, (start + length - 1) % height


def describe(seed_dir, min_size, show_all):
    rows = evolib.read_census(os.path.join(seed_dir, "census.csv"))
    last = rows[-1]
    if not show_all and last["tag_clusters_large"] < 2:
        return None
    params = evolib.read_params(os.path.join(seed_dir, "params.txt"))
    width, height = int(params.get("grid_width", 128)), int(params.get("grid_height", 128))
    cells = alive_at_end(os.path.join(seed_dir, "lineage.bin"))
    big = [c for c in clusters(cells) if len(c) >= min_size]
    print(f"\n{seed_dir}: tick {last['tick']}, {len(cells)} cells, "
          f"{last['tag_clusters_large']} large / {last['tag_clusters']} clusters in the census")
    out = []
    for c in sorted(big, key=len, reverse=True):
        g = lambda name: st.mean(x[1][GENE_INDEX[name]] for x in c)  # noqa: E731
        rws = [x[0] // width for x in c]
        a, b = home_rows(rws, height)
        lat = st.mean(math.cos(2 * math.pi * r / height) for r in rws)
        tags = [x[1][0] for x in c]
        d = {"cells": len(c), "tag_lo": min(tags), "tag_hi": max(tags), "diet": g("diet"),
             "photosynthesis": g("photosynthesis"), "harvest": g("harvest"),
             "preferred_temp": g("preferred_temp"), "rows": (a, b), "latitude": lat}
        out.append(d)
        print(f"  {d['cells']:>5} cells  tags {d['tag_lo']:.2f}-{d['tag_hi']:.2f}  diet {d['diet']:.2f}  "
              f"photo {d['photosynthesis']:.2f}  harvest {d['harvest']:.2f}  "
              f"pref temp {d['preferred_temp']:.1f}  home rows {a}-{b}  mean latitude {lat:+.2f}")
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("seed_dirs", nargs="+")
    ap.add_argument("--all", action="store_true", help="describe every run, not only those "
                    "ending with two or more large clusters")
    ap.add_argument("--min-size", type=int, default=10)
    args = ap.parse_args()
    for d in args.seed_dirs:
        describe(d, args.min_size, args.all)


if __name__ == "__main__":
    main()
