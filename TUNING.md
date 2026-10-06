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

*Rejudged 2026-10-06 under DECISIONS M6-4 (final year) and with the intake test (M6-5).*
E0 was rerun to add the intake columns. The state hashes match the first run and the
golden file. Over the final year, diversity holds in 0% of rows in every seed. Both sides
holds in 100% of rows in every seed under both the gene and the intake test. The overall
result is unchanged: lasts 6, diverse 0, both sides 6 (intake 6), cycles 6, balanced 0.

| seed | mean cells, years 6–25 | lowest count, years 6–25 | mean harvest | mean photosynthesis | mean attack | mean defense | largest body ever | peak infected | ticks with ≥ 2 large clusters |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 926 | 10 | 0.95 | 0.93 | 0.71 | 0.68 | 6 | 1,236 | 1% |
| 2 | 1,738 | 11 | 0.90 | 0.96 | 0.52 | 0.68 | 6 | 2,105 | 11% |
| 3 | 2,961 | 410 | 0.85 | 0.95 | 0.06 | 0.10 | 7 | 2,200 | 0% |
| 4 | 1,032 | 12 | 0.94 | 0.96 | 0.55 | 0.75 | 5 | 251 | 7% |
| 5 | 1,374 | 3 | 0.91 | 0.94 | 0.66 | 0.58 | 7 | 1,946 | 5% |
| 6 | 1,962 | 24 | 0.87 | 0.95 | 0.22 | 0.48 | 7 | 1,405 | 1% |

(Gene columns are means over years 6–25. These are from the first E0 run.)

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

## E8 Mutation rate and E1 Season strength (2026-10-06)

**Questions.**
- E8: is the mutation rate what limits diversity (the number of tag clusters)?
- E1: is the summer heat, or more generally the strength of the seasons, what breaks the
  cycles? Do the near-extinct winters seen in E0 erase diversity?

**Params.**
- E8: `mutation_base` 0.05 → 0.02 (`e8_mut_0.02`) or 0.10 (`e8_mut_0.10`).
- E1: `temp_season_amp` 10 → 8 (`e1_season_amp_8`) or 6 (`e1_season_amp_6`).

Light and sparks keep their seasonal swing in E1. All runs use 6 seeds × 50,000 ticks and
are judged under DECISIONS M6-1 to M6-5. The 30 runs, with the E0 rerun, took 842 s.

| experiment | lasts | diverse (final year) | both sides, gene | both sides, intake | cycles | balanced |
| --- | --- | --- | --- | --- | --- | --- |
| e0_baseline | 6 | 0 | 6 | 6 | 6 | 0 |
| e8_mut_0.02 | 6 | 0 | 4 | 6 | 6 | 0 |
| e8_mut_0.10 | 6 | 0 | 6 | 6 | 5 (seed 5: ×4.42, y10) | 0 |
| e1_season_amp_8 | 5 (seed 6 extinct at tick 16,192) | 0 | 5 | 5 | 5 | 0 |
| e1_season_amp_6 | 6 | 0 | 6 | 6 | 6 | 0 |

Means over seeds, years 6–25 (rows with living cells only):

| experiment | mean cells | yearly low, median (lowest) | harvest | photo | attack | defense | tolerance | rows with ≥ 2 large clusters | largest body | gene producer share | intake producer share |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| e0_baseline | 1,665 | 196 (3) | 0.90 | 0.95 | 0.45 | 0.54 | 0.10 | 4% | 7 | 66% | 69% |
| e8_mut_0.02 | 1,871 | 290 (17) | 0.90 | 0.87 | 0.35 | 0.40 | 0.05 | 8% | 9 | 46% | 72% |
| e8_mut_0.10 | 1,787 | 320 (7) | 0.86 | 0.95 | 0.33 | 0.73 | 0.05 | 11% | 14 | 72% | 67% |
| e1_season_amp_8 | 2,131 | 456 (0) | 0.85 | 0.94 | 0.39 | 0.61 | 0.05 | 10% | 8 | 78% | 71% |
| e1_season_amp_6 | 3,138 | 654 (2) | 0.87 | 0.96 | 0.21 | 0.59 | 0.07 | 12% | 9 | 75% | 78% |

**Answers**

- **E8: no.** Mutation 0.4× or 2× the default does not make three tag clusters. Diversity
  holds in 0% of final-year rows in all 12 runs. Two clusters of 10 or more cells exist in
  8–11% of rows, against 4% in E0. Three appeared only briefly: 3 rows (ticks
  32,000–32,200) in e8_mut_0.02 seed 2, and 4 rows (ticks 29,700–30,000) in
  e1_season_amp_6 seed 1. Nothing else in E0, E8 or E1 reached three. Mean tolerance falls from 0.10 to about
  0.05 in all four non-default experiments, so kin recognition narrows. At 0.10 one seed
  failed the cycles target. At 0.02 two seeds had no gene consumer at the end, though the
  intake test still found 211–250 cells living mainly on food.
- **E1: weaker seasons give more cells, but not diversity.** At amplitude 6 the mean
  population almost doubles (3,138 against 1,665), the median winter low rises from 196 to
  654, attack falls (0.21 against 0.45), and all 6 seeds pass the cycles target. Diversity
  is still 0% in every run.
  - Single winters can still come close to wiping out a run: seed 6 at amplitude 6 fell to
    2 cells at tick 11,700.
  - At amplitude 8, seed 6 went extinct. In the winter of year 8 (ticks 15,100–15,600),
    starvation and drain together took it from 956 cells to 10. Dormancy stayed near 0.1,
    so almost no cell hibernated. The last 2 cells lived on until tick 16,192 without
    dividing, then starved.
  - The E0 bottlenecks are therefore not the only thing erasing diversity. With winter
    lows above 600 cells, the living tags still form one cluster.
- **What diversity seems to need.** The tag has no effect of its own, and every birth can
  shift it by up to 0.05, so mutation keeps filling gaps between lineages. A gap wider than
  0.1 needs two lineages that stopped producing intermediates long ago. Neither mutation
  rate nor milder seasons provide that. Experiments that might: E6 (viruses punish common
  tags, which favours rare ones), and space, because a 128 × 128 world where free cells
  wander mixes everything.

**The two producer tests**

| | gene test (photosynthesis > harvest) | intake test (> ½ of gross intake from light) |
| --- | --- | --- |
| E0, share of classified cells, summer (season > 0.5) | 66% | 83% |
| E0, same, winter (season < −0.5) | 67% | 50% |
| e8_mut_0.02, summer / winter | 45% / 48% | 85% / 54% |
| e1_season_amp_6, summer / winter | 77% / 74% | 94% / 58% |
| cells counted by neither (no intake that tick) | — | 8–31% |

- **The gene test is steady through the year but splits cells that are much alike.** Mean
  harvest and photosynthesis are both about 0.9. In e8_mut_0.02 it found no consumers at
  all at the end of 2 seeds.
- **The intake test follows the seasons.** The same cells count as producers in summer and
  as consumers in winter, when light is low. In these runs it mostly measures the season
  and the generalists' current diet, not two lineages.
- **The intake test leaves many cells out.** Up to 31% of cells (e1_season_amp_6) have no
  gross intake in a given tick. They are dormant, newborn, or living on leak or drain, and
  count as neither.
- **Neither test currently shows separate producer and consumer lineages.**

Plots: `runs/<experiment>/overview.png`.

## E6 Viruses, E7 Photosynthesis, E9 Light (2026-10-06)

**Questions.**
- E6: do viruses check the common lineage without wiping it out, and does resistance rise
  over 25 years? Do epidemics split lineages (more tag clusters) or only thin them?
- E7: do producers crowd out consumers?
- E9: does a brighter world let large photosynthetic bodies form, since inner cells get
  light and recycle their own waste minerals?

**Params**

| file | change from the defaults |
| --- | --- |
| e6_outbreak_1e-5 | `outbreak_chance` 1e-6 → 1e-5 |
| e6_outbreak_1e-4 | `outbreak_chance` 1e-6 → 1e-4 |
| e6_spread_0.3 | `spread_chance` 0.2 → 0.3 |
| e7_photo_1.5 | `photo_rate` 2 → 1.5 |
| e7_photo_3 | `photo_rate` 2 → 3 |
| e7_photo_cost_0.4 | `cost_photosynthesis` 0.3 → 0.4 |
| e9_light_0.7 | `light_base` 0.5 → 0.7 (light is clamped at 1) |
| e9_light_0.7_mild | `light_base` 0.7 in the mild world (constant 15 °C, 1,500 sparks) |

The 48 runs (6 seeds each) took 1,293 s.

| experiment | lasts | diverse | both sides, gene | both sides, intake | cycles | balanced |
| --- | --- | --- | --- | --- | --- | --- |
| e0_baseline | 6 | 0 | 6 | 6 | 6 | 0 |
| e6_outbreak_1e-5 | 5 | 0 | 5 | 5 | 5 | 0 |
| e6_outbreak_1e-4 | 5 | 0 | 5 | 5 | 5 | 0 |
| e6_spread_0.3 | 5 | 0 | 4 | 5 | 5 | 0 |
| e7_photo_1.5 | 5 | 0 | 5 | 5 | 4 | 0 |
| e7_photo_3 | 4 | 0 (seed 6: 10%) | 3 | 4 | 4 | 0 |
| e7_photo_cost_0.4 | 6 | 0 | 5 | 6 | 6 | 0 |
| e9_light_0.7 | 6 | 0 | 6 | 6 | 5 | 0 |
| e9_light_0.7_mild | 6 | 0 | 6 | 6 | 6 | 0 |

Means over seeds, years 6–25, rows with living cells:

| experiment | cells | yearly low, median (lowest) | harvest | photo | attack | defense | resistance | share of cells infected | rows with ≥ 2 large clusters | largest body | in bodies | gene producers | intake producers |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| e0_baseline | 1,665 | 196 (3) | 0.90 | 0.95 | 0.45 | 0.54 | 0.09 | 5.6% | 4% | 7 | 7% | 66% | 69% |
| e6_outbreak_1e-5 | 1,755 | 408 (1) | 0.88 | 0.93 | 0.36 | 0.46 | 0.10 | 11.7% | 16% | 7 | 8% | 65% | 72% |
| e6_outbreak_1e-4 | 1,594 | 318 (1) | 0.88 | 0.94 | 0.39 | 0.51 | 0.10 | 18.7% | 12% | 8 | 7% | 69% | 72% |
| e6_spread_0.3 | 1,799 | 264 (1) | 0.89 | 0.93 | 0.42 | 0.63 | 0.11 | 6.3% | 5% | 8 | 7% | 63% | 72% |
| e7_photo_1.5 | 1,530 | 431 (3) | 0.93 | 0.67 | 0.30 | 0.40 | 0.06 | 4.5% | 8% | 5 | 3% | 37% | 37% |
| e7_photo_3 | 2,320 | 832 (1) | 0.82 | 0.96 | 0.39 | 0.69 | 0.09 | 6.4% | 3% | 10 | 8% | 83% | 93% |
| e7_photo_cost_0.4 | 1,609 | 332 (4) | 0.93 | 0.77 | 0.33 | 0.38 | 0.06 | 6.4% | 2% | 8 | 6% | 46% | 58% |
| e9_light_0.7 | 2,271 | 688 (6) | 0.87 | 0.96 | 0.30 | 0.79 | 0.09 | 4.7% | 11% | 11 | 9% | 80% | 90% |
| e9_light_0.7_mild | 4,250 | 3,143 (560) | 0.94 | 0.86 | 0.17 | 0.86 | 0.07 | 2.8% | 7% | 19 | 9% | 35% | 55% |

**How the 6 extinctions happened**

Five were heavy drain crashes in summer or autumn, not winter die-offs or epidemics:
- In four of them, mean attack was above mean defense, and drain killed 2.1–2.7× as many
  cells as starvation did in the final collapse.
- In the fifth (e7_photo_3 seed 5), drain and starvation deaths were about equal, with
  attack and defense both high (0.84 and 0.86).

| run | extinct at tick | collapse | infected at most | drained / starved deaths | attack / defense |
| --- | --- | --- | --- | --- | --- |
| e6_outbreak_1e-4 seed 4 | 24,340 | 1,144 cells → 0 from tick 22,100 | 35 | 4,724 / 2,275 | 0.63 / 0.42 |
| e6_outbreak_1e-5 seed 2 | 27,985 | 1,977 → 0 from 26,700 (midsummer) | 141 | 4,160 / 1,568 | 0.78 / 0.54 |
| e6_spread_0.3 seed 6 | 19,504 | 2,694 → 0 from 18,700 (midsummer) | 437 | 4,183 / 2,031 | 0.72 / 0.60 |
| e7_photo_3 seed 4 | 10,049 | 4,087 → 0 from 8,100 | 555 | 15,085 / 5,902 | 0.52 / 0.38 |
| e7_photo_3 seed 5 | 29,524 | 2,021 → 0 from 28,600 (midsummer) | 0 | 2,628 / 2,694 | 0.84 / 0.86 |
| e7_photo_1.5 seed 2 | 40,470 | slow fall from 75 cells at 38,000 | 1 | 0 / 81 | 0.02 / 0.06 |

The sixth (e7_photo_1.5 seed 2) is different. That lineage had lost photosynthesis (mean
0.01) and shrank by starvation over about 2,500 ticks. The drain crashes bear on RULES.md
open question 4: at the default drain factor, predators can wipe out their prey and then
themselves. E4 tests this directly.

### Do epidemics split lineages or only thin them?

`tools/epidemics.py` finds every epidemic (consecutive census rows with an infected cell)
whose peak reaches 50 infected cells. For each, it records the number of tag clusters of at
least 10 cells ("large clusters"):
- before: in the last row before the epidemic;
- during: the highest count while it lasts;
- after: 1,000 ticks after its last infected row.

It also records the lowest population during the epidemic, as a share of the population
before it. Each epidemic is compared with virus-free control windows of the same length
that start at the same time of year (within 200 ticks). The controls come from E0, E1 and
E8: with outbreaks at 1e-5 and 1e-4, some cell is infected almost all the time, so those
runs have no virus-free windows of their own.

```
python3 tools/epidemics.py runs/e0_baseline runs/e6_* \
    --control runs/e0_baseline runs/e1_* runs/e8_*
```

| experiment | epidemics (peak ≥ 50) | large clusters before → max during → after | more clusters during (control) | after: up / same / down (control) | lowest cells, share of before, median (control) |
| --- | --- | --- | --- | --- | --- |
| e0_baseline | 55 | 1.05 → 1.09 → 1.13 | 4% (4%) | 11 / 85 / 4% (2 / 96 / 2%) | 22% (37%) |
| e6_outbreak_1e-5 | 82 | 1.22 → 1.28 → 1.15 | 7% (4%) | 2 / 88 / 10% (3 / 93 / 4%) | 58% (58%) |
| e6_outbreak_1e-4 | 31, plus 1 endemic stretch | 1.16 → 1.45 → 1.13 | 26% (7%) | 6 / 84 / 10% (4 / 92 / 4%) | 97% (53%) |
| e6_spread_0.3 | 61 | 1.10 → 1.16 → 1.05 | 7% (4%) | 3 / 89 / 8% (2 / 95 / 3%) | 24% (44%) |

The endemic stretch is seed 3 of e6_outbreak_1e-4, infected from tick 23,600 to the end;
it has no before or after. Control figures are for the 55, 78, 22 and 61 epidemics that
have matched windows.

**Answers**

- **Viruses mostly thin lineages; any split they make does not last.**
  - In 84–89% of epidemics, the number of large clusters 1,000 ticks after the end equals
    the number before. That is close to the control (92–96%).
  - With more viruses, a second large cluster appears during an epidemic more often than in
    the control: 7% against 4% at 1e-5 and with faster spread, and 26% against 7% at 1e-4.
    A virus kills the cells whose tags it matches, which can open a gap in the middle of
    the tag range.
  - These splits close again: after an E6 epidemic, the count is lower than before about
    as often as it is higher, or more often (down 8–10%, up 2–6%).
  - Across a whole run, two large clusters exist more often when viruses are frequent
    (16% of rows at 1e-5 and 12% at 1e-4, against 4% in E0). Three large clusters were
    reached in only 8 rows of one seed at 1e-5 and in 1 row at 1e-4. No E6 run met the
    diversity target over its final year.
- **Viruses thin populations at low rates.**
  - At the default rate and with faster spread, the population falls to a median 22–24%
    of its pre-epidemic size during an epidemic, against 37–44% in matched virus-free
    windows.
  - At 1e-5 there is no difference (58% against 58%).
  - At 1e-4, epidemics last for years and often start at a winter low, so this measure
    says little (97%).
- **Resistance does not rise for good.** Mean resistance over years 6–25 stays at 0.10–0.11
  in E6, the same as E0 (0.09). Its highest per-seed peak rises from 0.13–0.27 (E0) to
  0.09–0.42 (1e-4), and then falls back.
- **E6 and open question 6:** viruses hold the common lineage back without wiping it out.
  None of the 3 E6 extinctions was an epidemic.

**E7: producers crowd out consumers only when photosynthesis is cheap.**
- At `photo_rate` 3, 83% of cells are gene producers and 93% intake producers. Two of 6
  runs went extinct in predator crashes, and in seed 2 no gene consumer was left at the
  end.
- At `photo_rate` 1.5, or with photosynthesis costing more (0.4), photosynthesis falls to a
  mean of 0.67 or 0.77. Gene producers drop to 37–46%, and harvest stays high (0.93).
- The two producer tests agree closely at 1.5 (37% and 37%), which they don't in the other
  E7 settings. The census can't show whether this means two lineages or one mixed
  population that photosynthesises less. The lineage log could.
- `cost_photosynthesis` 0.4 is the only E7 setting in which all 6 runs survive and cycle.

**E9: no large photosynthetic bodies.**
- Bodies stay small: the largest is 11 cells at `light_base` 0.7 and 19 in the mild bright
  world, about 9% of cells are in bodies, mean adhesion stays at 0.07, and inner cells
  hardly occur (at most 1).
- Brighter light mostly raises the population: 2,271 cells, and 4,250 in the mild bright
  world, where the yearly low never falls below 560.
- Defense rises (0.79 and 0.86), and so does the gene producer share at 0.7 (80%), but the
  intake test shows only 55% producers in the mild bright world.
- The mild bright world is the steadiest setting so far: all 6 seeds last and cycle, with
  a median yearly low of 3,143 cells. Diversity is still 0%.

Plots: `runs/<experiment>/overview.png`.
