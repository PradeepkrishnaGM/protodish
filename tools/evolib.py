"""Shared helpers for the tools: reading census CSVs and params, and the balance targets.

Standard library only, so check_balance.py and batch.py run without the .venv.
"""

import csv
import math
import os

# RULES.md "What a balanced run looks like".
TARGET_TICKS = 50_000
MIN_LARGE_CLUSTERS = 3
FIRST_COMPARED_YEAR = 5  # 1-based: year 5 is compared with year 4 (DECISIONS M6)
PEAK_RATIO = 3.0

# Gene ranges, from core/genome.hpp (tag is circular and has no meaningful mean).
GENE_RANGES = {
    "tag": (0.0, 1.0),
    "tolerance": (0.0, 0.5),
    "harvest": (0.0, 1.0),
    "diet": (0.0, 1.0),
    "photosynthesis": (0.0, 1.0),
    "preferred_temp": (0.0, 30.0),
    "attack": (0.0, 1.0),
    "defense": (0.0, 1.0),
    "resistance": (0.0, 1.0),
    "adhesion": (0.0, 1.0),
    "share": (0.0, 1.0),
    "role_split": (-1.0, 1.0),
    "motility": (0.0, 1.0),
    "appetite": (0.0, 1.0),
    "caution": (0.0, 1.0),
    "boldness": (0.0, 1.0),
    "sociability": (-1.0, 1.0),
    "dormancy": (0.0, 1.0),
    "mutability": (0.0, 1.0),
    "mating": (0.0, 1.0),
}


def read_census(path):
    """Returns the census rows as a list of dicts with numeric values (hash stays a string)."""
    rows = []
    with open(path, newline="") as f:
        for raw in csv.DictReader(f):
            row = {}
            for k, v in raw.items():
                if k == "hash":
                    row[k] = v
                elif k in ("season",) or k.startswith("mean_") or k in (
                        "food_a", "food_b", "minerals", "in_cells", "total"):
                    row[k] = float(v)
                else:
                    row[k] = int(v)
            rows.append(row)
    return rows


def read_params(path):
    """Reads a key = value params file (as written by evolve --dump-params)."""
    out = {}
    with open(path) as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if not line or "=" not in line:
                continue
            k, v = (s.strip() for s in line.split("=", 1))
            try:
                out[k] = float(v)
            except ValueError:
                out[k] = v
    return out


def year_length_for(census_path, default=2000):
    """The year length from a params.txt next to the census, if there is one."""
    p = os.path.join(os.path.dirname(os.path.abspath(census_path)), "params.txt")
    if os.path.exists(p):
        return int(read_params(p).get("year_length", default))
    return default


def yearly_peaks(rows, year_length):
    """Peak population per year (0-based years), from the peak_cells column.

    A row at tick T covers the states after ticks T-interval+1 .. T. Year y holds the rows
    with y*L < T <= (y+1)*L. Only complete years are returned.
    """
    peaks = {}
    last_tick = rows[-1]["tick"]
    for r in rows[1:]:
        y = (r["tick"] - 1) // year_length
        peaks[y] = max(peaks.get(y, 0), r["peak_cells"])
    complete = last_tick // year_length
    return [peaks.get(y, 0) for y in range(complete)]


def check_balance(rows, year_length, target_ticks=TARGET_TICKS):
    """Evaluates the four balanced-run targets. Returns a dict of results."""
    last = rows[-1]
    extinct = last["cells"] == 0
    res = {
        "ticks": last["tick"],
        "final_cells": last["cells"],
        "extinct": extinct,
    }

    # 1. It lasts.
    res["lasts"] = (not extinct) and last["tick"] >= target_ticks
    if not extinct and last["tick"] < target_ticks:
        res["lasts_note"] = f"ran only {last['tick']} ticks"

    # 2. It stays diverse: tag clusters of at least cluster_min_size cells.
    res["large_clusters"] = last["tag_clusters_large"]
    res["all_clusters"] = last["tag_clusters"]
    res["diverse"] = last["tag_clusters_large"] >= MIN_LARGE_CLUSTERS

    # 3. Both sides exist.
    res["producers"] = last["producers"]
    res["consumers"] = last["consumers"]
    res["both_sides"] = last["producers"] > 0 and last["consumers"] > 0

    # 4. It cycles without crashing: from year 5 on, each peak within x3 of the previous.
    peaks = yearly_peaks(rows, year_length)
    res["yearly_peaks"] = peaks
    worst_ratio, worst_year = 1.0, None
    first = FIRST_COMPARED_YEAR - 1  # 0-based index of year 5
    compared = 0
    for y in range(first, len(peaks)):
        a, b = peaks[y - 1], peaks[y]
        ratio = math.inf if min(a, b) == 0 else max(a, b) / min(a, b)
        compared += 1
        if ratio > worst_ratio:
            worst_ratio, worst_year = ratio, y + 1
    res["worst_peak_ratio"] = worst_ratio
    res["worst_peak_year"] = worst_year  # 1-based, the later year of the worst pair
    res["years_compared"] = compared
    res["cycles"] = (not extinct) and compared > 0 and worst_ratio <= PEAK_RATIO

    res["balanced"] = res["lasts"] and res["diverse"] and res["both_sides"] and res["cycles"]
    return res


def mark(ok):
    return "PASS" if ok else "fail"


def summary_line(name, r):
    ratio = "inf" if math.isinf(r["worst_peak_ratio"]) else f"{r['worst_peak_ratio']:.2f}"
    year = f" (y{r['worst_peak_year']})" if r["worst_peak_year"] else ""
    lasts = mark(r["lasts"])
    if r["extinct"]:
        lasts += f" extinct@{r['ticks']}"
    elif "lasts_note" in r:
        lasts += f" ({r['lasts_note']})"
    return (f"{name:<28} {lasts:<20} {mark(r['diverse'])} {r['large_clusters']:>2}/{r['all_clusters']:<4} "
            f"{mark(r['both_sides'])} {r['producers']:>5}/{r['consumers']:<5} "
            f"{mark(r['cycles'])} {ratio:>6}{year:<6} {'BALANCED' if r['balanced'] else '-'}")


SUMMARY_HEADER = (f"{'run':<28} {'lasts':<20} {'diverse lg/all':<15} {'both sides P/C':<17} "
                  f"{'cycles worst':<18} result")
