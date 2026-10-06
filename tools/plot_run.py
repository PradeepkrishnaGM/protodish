#!/usr/bin/env python3
"""Plots census CSVs: population, producers, infected cells, tag clusters, body sizes and gene means.

usage: plot_run.py CENSUS.csv [CENSUS.csv ...] [--out FILE.png] [--title TEXT]

With one census, draws one run. With several (for example all seeds of one experiment),
draws each run as a thin line in every panel, so the spread between seeds is visible; the
body-size panel then shows the mean over the runs. Needs matplotlib (.venv, requirements.txt).
"""

import argparse
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import matplotlib.ticker  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import evolib  # noqa: E402

# Categorical slots, in fixed order (dataviz reference palette, light mode).
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300", "#4a3aa7", "#e34948"]
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
GRID = "#e4e3df"
# Sequential blue ramp for the body-size buckets (small -> large).
BUCKET_RAMP = ["#c6dcf5", "#9cc2ee", "#6aa3e3", "#3e83d4", "#2462b0", "#16437c"]
BUCKETS = ["bodies_2", "bodies_3_4", "bodies_5_8", "bodies_9_16", "bodies_17_32", "bodies_33up"]
BUCKET_LABELS = ["2", "3-4", "5-8", "9-16", "17-32", "33+"]

GENE_GROUPS = [
    ("Feeding genes", ["harvest", "diet", "photosynthesis", "preferred_temp", "tolerance"]),
    ("Conflict and heredity genes", ["attack", "defense", "resistance", "mutability", "mating"]),
    ("Body genes", ["adhesion", "share", "role_split"]),
    ("Behavior genes", ["motility", "appetite", "caution", "boldness", "sociability", "dormancy"]),
]


def style_axes(ax, title, ylabel=None):
    ax.set_facecolor(SURFACE)
    ax.set_title(title, loc="left", fontsize=10, color=TEXT, fontweight="bold")
    ax.grid(axis="y", color=GRID, linewidth=0.6)
    ax.tick_params(colors=TEXT_2, labelsize=8, length=0)
    for side in ("top", "right", "left"):
        ax.spines[side].set_visible(False)
    ax.spines["bottom"].set_color(GRID)
    if ylabel:
        ax.set_ylabel(ylabel, fontsize=8, color=TEXT_2)


def years(rows, yl):
    return [r["tick"] / yl for r in rows]


def line_width(n_runs):
    return 1.6 if n_runs == 1 else 0.9


def plot_series(ax, runs, yl, specs):
    """specs: list of (column or function, label, colour)."""
    lw = line_width(len(runs))
    alpha = 1.0 if len(runs) == 1 else 0.55
    for k, (col, label, colour) in enumerate(specs):
        for i, rows in enumerate(runs):
            y = [col(r) if callable(col) else r[col] for r in rows]
            ax.plot(years(rows, yl), y, color=colour, linewidth=lw, alpha=alpha,
                    label=label if i == 0 else None)
    if len(specs) > 1:
        ax.legend(fontsize=7, frameon=False, labelcolor=TEXT_2, loc="best", ncol=len(specs))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("census", nargs="+")
    ap.add_argument("--out", help="PNG path (default: next to the first census)")
    ap.add_argument("--title")
    ap.add_argument("--year-length", type=int)
    args = ap.parse_args()

    runs = [evolib.read_census(p) for p in args.census]
    yl = args.year_length or evolib.year_length_for(args.census[0])
    out = args.out or os.path.splitext(args.census[0])[0] + ".png"
    title = args.title or (os.path.basename(os.path.dirname(os.path.abspath(args.census[0])))
                           + (f" ({len(runs)} runs)" if len(runs) > 1 else ""))

    fig, axes = plt.subplots(5, 2, figsize=(13, 15), sharex=True, facecolor=SURFACE)
    fig.suptitle(title, x=0.06, ha="left", fontsize=13, color=TEXT, fontweight="bold")
    ax = axes.flat

    style_axes(ax[0], "Population", "cells")
    plot_series(ax[0], runs, yl, [("cells", "all cells", SERIES[0]),
                                  ("free_cells", "free", SERIES[1]),
                                  ("body_cells", "in bodies", SERIES[2])])

    style_axes(ax[1], "Producers and consumers", "cells")
    plot_series(ax[1], runs, yl, [("producers", "producers", SERIES[2]),
                                  ("consumers", "consumers", SERIES[0])])

    style_axes(ax[2], "Infected cells", "cells")
    plot_series(ax[2], runs, yl, [("infected", "infected", SERIES[0])])

    style_axes(ax[3], "Tag clusters", "clusters")
    plot_series(ax[3], runs, yl, [("tag_clusters", "all", SERIES[1]),
                                  ("tag_clusters_large", "of at least the minimum size", SERIES[0])])
    ax[3].axhline(evolib.MIN_LARGE_CLUSTERS, color=TEXT_2, linewidth=0.8, linestyle=(0, (4, 3)))
    ax[3].text(0, evolib.MIN_LARGE_CLUSTERS, " target", fontsize=7, color=TEXT_2, va="bottom")
    ax[3].yaxis.set_major_locator(matplotlib.ticker.MaxNLocator(integer=True))
    ax[3].set_ylim(bottom=0)

    # Body sizes: number of bodies per size bucket, stacked (mean over runs).
    style_axes(ax[4], "Bodies by size (cells per body)", "bodies")
    base = runs[0]
    n = min(len(r) for r in runs)
    x = years(base[:n], yl)
    stacks = [[sum(r[i][b] for r in runs) / len(runs) for i in range(n)] for b in BUCKETS]
    ax[4].stackplot(x, stacks, colors=BUCKET_RAMP, labels=BUCKET_LABELS, edgecolor=SURFACE,
                    linewidth=0.3)
    ax[4].legend(fontsize=7, frameon=False, labelcolor=TEXT_2, loc="upper left", ncol=6)

    style_axes(ax[5], "Largest body", "cells")
    plot_series(ax[5], runs, yl, [("largest_body", "largest body", SERIES[0])])

    for k, (name, genes) in enumerate(GENE_GROUPS):
        a = ax[6 + k]
        style_axes(a, name + " (mean, as a fraction of the range)")
        specs = []
        for j, g in enumerate(genes):
            lo, hi = evolib.GENE_RANGES[g]
            specs.append((lambda r, g=g, lo=lo, hi=hi: (r["mean_" + g] - lo) / (hi - lo), g, SERIES[j]))
        plot_series(a, runs, yl, specs)
        a.set_ylim(-0.02, 1.02)

    for a in axes[-1]:
        a.set_xlabel("years", fontsize=8, color=TEXT_2)
    fig.tight_layout(rect=(0, 0, 1, 0.98))
    fig.savefig(out, dpi=110, facecolor=SURFACE)
    print(out)


if __name__ == "__main__":
    main()
