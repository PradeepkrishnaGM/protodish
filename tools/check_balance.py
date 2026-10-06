#!/usr/bin/env python3
"""Checks the four "balanced run" targets of RULES.md against census CSVs.

usage: check_balance.py CENSUS.csv [CENSUS.csv ...] [--year-length N] [--ticks N] [--peaks]

A run passes when
  1. it lasts: cells are alive after --ticks ticks (default 50,000);
  2. it stays diverse: at least 3 tag clusters of >= cluster_min_size cells at the end;
  3. both sides exist: producers and consumers are both alive at the end;
  4. it cycles: from year 5 on, each year's peak population is within x3 of the previous
     year's. An extinct run fails 1 and 4.
The year length comes from a params.txt next to each census (written by batch.py), else 2000.
Exit status is 0 when every run is balanced, 1 otherwise.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import evolib  # noqa: E402


def run_name(path):
    d = os.path.dirname(os.path.abspath(path))
    return os.path.join(os.path.basename(os.path.dirname(d)), os.path.basename(d))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("census", nargs="+")
    ap.add_argument("--year-length", type=int, help="override the year length")
    ap.add_argument("--ticks", type=int, default=evolib.TARGET_TICKS, help="ticks a run must last")
    ap.add_argument("--peaks", action="store_true", help="also print each run's yearly peaks")
    args = ap.parse_args()

    print(evolib.SUMMARY_HEADER)
    all_ok = True
    for path in args.census:
        rows = evolib.read_census(path)
        yl = args.year_length or evolib.year_length_for(path)
        r = evolib.check_balance(rows, yl, args.ticks)
        all_ok &= r["balanced"]
        print(evolib.summary_line(run_name(path), r))
        if args.peaks:
            print("   peaks:", " ".join(str(p) for p in r["yearly_peaks"]))
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
