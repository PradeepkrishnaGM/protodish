# Tuning

Results of M6 tuning experiments. Every experiment is a params file in `experiments/`,
run with 6 seeds × 50,000 ticks:

```
python3 tools/batch.py --params experiments/<name>.params --seeds 1-6 --plot
```

Runs land in `runs/<name>/` (not in git), with `summary.txt` and `overview.png`. The
balance targets and how they are judged are in DECISIONS.md, M6 1–3. RULES.md defaults are
never changed without approval. A changed setting also changes the RNG stream, so
experiments are compared by the spread over seeds, not run by run.

## E0 Baseline (2026-10-06)

**Question.** Which of the four balanced-run targets fail under the RULES.md defaults over
25 years?

**Params.** None (`experiments/e0_baseline.params` is empty). Batch wall time was 160 s for
6 parallel runs.

| seed | lasts | diverse (clusters ≥ 10 / all) | both sides (P / C at end) | cycles (worst peak ratio, year) | balanced |
| --- | --- | --- | --- | --- | --- |
| 1 | pass | fail (1 / 1) | pass (351 / 252) | pass (1.87, y8) | no |
| 2 | pass | fail (1 / 1) | pass (2,436 / 754) | pass (1.72, y20) | no |
| 3 | pass | fail (1 / 1) | pass (3,346 / 210) | pass (1.86, y6) | no |
| 4 | pass | fail (1 / 1) | pass (444 / 107) | pass (2.00, y15) | no |
| 5 | pass | fail (1 / 1) | pass (1,703 / 186) | pass (1.89, y22) | no |
| 6 | pass | fail (2 / 2) | pass (264 / 109) | pass (1.83, y9) | no |

Summary: lasts 6/6, both sides 6/6, cycles 6/6, **diverse 0/6**, balanced 0/6.

| seed | mean cells, years 6–25 | lowest count, years 6–25 | mean harvest | mean photosynthesis | mean attack | mean defense | largest body ever | peak infected | ticks with ≥ 2 large clusters |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 926 | 10 | 0.95 | 0.93 | 0.71 | 0.68 | 6 | 1,236 | 1% |
| 2 | 1,738 | 11 | 0.90 | 0.96 | 0.52 | 0.68 | 6 | 2,105 | 11% |
| 3 | 2,961 | 410 | 0.85 | 0.95 | 0.06 | 0.10 | 7 | 2,200 | 0% |
| 4 | 1,032 | 12 | 0.94 | 0.96 | 0.55 | 0.75 | 5 | 251 | 7% |
| 5 | 1,374 | 3 | 0.91 | 0.94 | 0.66 | 0.58 | 7 | 1,946 | 5% |
| 6 | 1,962 | 24 | 0.87 | 0.95 | 0.22 | 0.48 | 7 | 1,405 | 1% |

(Gene columns are means over years 6–25.)

**Observations**

- **The M2/M3 summer crash is gone.** No run went extinct, and yearly peaks stay within
  about ×2 of each other in every seed.
- **Winters come close to extinction.** In 5 of 6 seeds the winter low falls to 3–24
  cells. The cycles target only looks at peaks, so it does not see this. One unlucky
  winter could end a run.
- **Diversity is the only failing target.** All cells form one tag cluster almost the
  whole time; two clusters of 10 or more cells existed in 0–11% of census rows. In seed 1
  at tick 50,000, the 603 living tags span 0.50–0.70 with no gap wider than 0.025. The
  population is one continuous smear of tags, and the yearly winter bottleneck (down to a
  handful of cells) prunes it back to one lineage.
- **The end of a run falls at a trough.** Tick 50,000 is the first tick of a spring
  (season 0, rising), right after winter, when the population is smallest. Judging
  diversity and "both sides" at that moment is the strictest possible choice.
- **"Both sides" passes on a technicality.** Mean harvest (0.85–0.95) and mean
  photosynthesis (0.93–0.96) both run close to their maximum. Most cells are generalists
  that do both, and the producer test (photosynthesis > harvest) splits them by a small
  difference between two high genes. The means suggest there are no distinct producer
  and consumer lineages; the census alone cannot show this for certain.
- **An attack-defense arms race** develops in 4 of 6 seeds (seeds 1, 2, 4 and 5: mean
  attack 0.52–0.71, mean defense 0.58–0.75). Seed 6 is milder (0.22 and 0.48). Seed 3
  stayed peaceful (0.06 and 0.10) and has the largest, steadiest population.
- **Bodies stay at 2–7 cells**, as in M4. Mean adhesion is about 0.06.
- **Viruses cause epidemics** of up to about 2,200 infected cells. Mean resistance over
  years 6–25 is 0.05–0.13 per seed. It peaks at 0.13–0.27 and falls back, which fits a
  cost of resistance that pays only during epidemics.

Plots: `runs/e0_baseline/overview.png`.
