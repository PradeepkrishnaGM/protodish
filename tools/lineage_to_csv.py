#!/usr/bin/env python3
"""Dumps a binary lineage log (evolve --lineage) to two CSVs: births and deaths.

usage: lineage_to_csv.py LINEAGE.bin [--out PREFIX]

Writes PREFIX.births.csv and PREFIX.deaths.csv (PREFIX defaults to the input path without
.bin). The format is described in core/records.hpp. Ancestors appear as births of kind
"ancestor" at tick 0. Genes are stored as 32-bit floats.
"""

import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import evolib  # noqa: E402

GENES = list(evolib.GENE_RANGES)
KINDS = ["attached_clone", "released_clone", "mating", "ancestor"]
CAUSES = ["disaster", "drained", "starved"]
HEADER = struct.Struct("<8sIIIIQQ24x")
BIRTH = struct.Struct("<BBHIIII" + "f" * len(GENES))
DEATH = struct.Struct("<BBHIII")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("lineage")
    ap.add_argument("--out", help="output prefix")
    args = ap.parse_args()
    prefix = args.out or (args.lineage[:-4] if args.lineage.endswith(".bin") else args.lineage)

    with open(args.lineage, "rb") as f:
        data = f.read()
    magic, version, n_genes, birth_size, death_size, seed, phash = HEADER.unpack_from(data, 0)
    if magic != b"EVOLIN01" or version != 1:
        sys.exit(f"{args.lineage}: not a version-1 lineage log")
    if n_genes != len(GENES) or birth_size != BIRTH.size or death_size != DEATH.size:
        sys.exit(f"{args.lineage}: record layout does not match this tool")

    n_births = n_deaths = 0
    with open(prefix + ".births.csv", "w") as fb, open(prefix + ".deaths.csv", "w") as fd:
        fb.write("tick,id,parent,parent2,site,kind," + ",".join(GENES) + "\n")
        fd.write("tick,id,age,site,cause\n")
        i = HEADER.size
        while i < len(data):
            t = data[i]
            if t == 1:
                _, kind, site, tick, cid, parent, parent2, *genes = BIRTH.unpack_from(data, i)
                fb.write(f"{tick},{cid},{parent},{parent2},{site},{KINDS[kind]},"
                         + ",".join(f"{g:.7g}" for g in genes) + "\n")
                i += BIRTH.size
                n_births += 1
            elif t == 2:
                _, cause, site, tick, cid, age = DEATH.unpack_from(data, i)
                fd.write(f"{tick},{cid},{age},{site},{CAUSES[cause]}\n")
                i += DEATH.size
                n_deaths += 1
            else:
                sys.exit(f"{args.lineage}: bad record type {t} at byte {i}")
    print(f"seed {seed}, params {phash:016x}: {n_births} births (with ancestors), "
          f"{n_deaths} deaths -> {prefix}.births.csv, {prefix}.deaths.csv")


if __name__ == "__main__":
    main()
