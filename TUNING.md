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

## M6 summary (2026-10-06)

Tuning stopped after 186 runs of 50,000 ticks: 22 parameter sets, 6 or 24 seeds each.
Another 12 runs repeated earlier ones to add census columns or lineage logs; they gave the
same census.
The RULES.md defaults are unchanged. The experiments and full results follow this summary.

### The four targets under the RULES.md defaults (E0, 24 seeds)

| target | runs passing | why |
| --- | --- | --- |
| It lasts | 20 of 24 (83%) | About 1 run in 6 dies in a predator-prey crash in spring or summer, not in winter |
| It stays diverse (≥ 3 clusters of ≥ 10 cells, half of the final year) | **0 of 24** | Only one tag cluster most of the time; three large clusters appeared in only 1 census row |
| Both sides exist (gene test, half of the final year) | 20 of 24, every survivor | It passes, but most cells are generalists with harvest and photosynthesis both near 0.9 |
| It cycles (yearly peaks within ×3 from year 5) | 19 of 24 | Peaks are steady (worst ratio about ×2) in survivors; one survivor failed at ×3.08 |
| **Balanced** | **0 of 24** | Diversity fails everywhere |

No experiment produced a balanced run. Diversity held over the final year in only one
run: e7_photo_3 seed 6, in 10% of rows.

### What each experiment showed

| experiment | changes | survival | diversity | verdict |
| --- | --- | --- | --- | --- |
| E8 mutation 0.02 / 0.10 | `mutation_base` | 6/6, 6/6 | none | no help |
| E1 season amplitude 8 / 6 | `temp_season_amp` | 5/6, 6/6 | none | amplitude 6: twice the cells, higher lows (6 seeds only) |
| E6 viruses | `outbreak_chance` 1e-5, 1e-4; `spread_chance` 0.3 | 5/6 each | splits during epidemics, gone afterwards | viruses thin lineages, they don't split them |
| E7 photosynthesis | `photo_rate` 1.5, 3; `cost_photosynthesis` 0.4 | 5/6, 4/6, 6/6 | none | at rate 3 producers dominate and predators crash |
| E9 light | `light_base` 0.7, plus the mild world | 6/6, 6/6 | none | no large bodies (largest 11 and 19) |
| E4 drain | `drain_factor` 2 (24 seeds), 5 | 22/24, 1/6 | none | drain 5 is deadly; drain 2 halves extinctions (not statistically clear) |
| E3 bodies | mild world, `share_threshold` 5, `cost_crowding` 0.01 | 4/6 | none | bodies still at most 11 cells |
| D0 three starting groups | option `initial_tag_groups` = 3 | 3/6 | lost within 1–2 winters | lineages that differ only in tag don't persist |
| D1a climate belts | `temp_latitude_amp` 10, `light_latitude_amp` 0.35 (24 seeds) | **24/24** | as E0 (10% of rows with 2) | the safest world tested; rare diet or climate splits |
| D1a + D2a | plus `cost_move` 0.5 | 6/6 | less than D1a | no help |
| D1a + leak | plus `leak_fraction` 0.35 | 6/6 | less than D1a | no help |
| D3b generalist cost | option `cost_generalist` 0.4, 0.8 | 6/6, 6/6 | none | removes photosynthesis instead of splitting cells |

Not run: E2 (starting food), E5 (leak on its own), D1b, D1c, D2b, D3a.

### Why diversity fails

- A tag has no effect except kin recognition and virus targets, so lineages that differ
  only in tag compete as equals. D0 shows that one wins within one or two winters.
- In E0 a cell lives a median 209 ticks, about 10 generations a year. Two lineages need
  roughly 10–20 years side by side before their tags drift more than 0.1 apart.
- Lineages last that long only when they hold different niches: a diet split (D1a seed 2)
  or a climate split (D1a seed 11). Each arose in about 1 run in 12, and never three at
  once.

### Candidate rule changes, for decision (none applied)

1. **Write the M3-1 supply formula into RULES.md.** *Applied 2026-10-06; text only, the engine is unchanged.* The engine already uses the better of
   the two ways to eat (DECISIONS M3-1). RULES.md still says "(… + …) / 2", under which no
   consumer can reach a supply above 0.5. This only brings the text in line with the
   engine.
2. **Stronger climate belts: `temp_latitude_amp` 5 → 10, `light_latitude_amp` 0.2 → 0.35.**
   The best evidence of any change:
   - 0 of 24 extinctions against 4 of 24 (open question 2);
   - median yearly low 374 against 201;
   - lower mean attack (0.31 against 0.40).

   Diversity is unchanged.
3. **Drain factor 3 → 2** (open question 4). At 5, five of 6 runs die. At 2, 2 of 24 die
   against 4 of 24 at 3, and the yearly low is higher (271 against 201). The direction is
   consistent but the difference isn't statistically clear. It has not been tested
   together with change 2.
4. **Season amplitude 10 → 6** (open question 2). Six seeds: all last and cycle, there are
   twice as many cells, and the yearly low is 654. But amplitude 8 had one extinction.
   Weaker evidence than change 2; needs 24 seeds.
5. **Diversity needs either a niche rule or a different target.** No parameter in the
   approved range creates three lasting clusters. The options are:
   - a mechanism that creates lasting niches: fertile spark patches (D1c, a rule change),
     or the generalist cost started from a harvester group and a producer group (an
     extension of D0 and D3b);
   - a different target, such as two large clusters, or diversity measured over genes
     instead of tags.

   Evidence: E8, E6, D0, D2a and D3b above.
6. **Which producer test becomes the target.** Neither the gene test nor the intake test
   shows separate producer and consumer lineages:
   - The gene test is steady through the year but splits generalists by a small
     difference between two high genes.
   - The intake test follows the season (E0: 83% producers in summer, 50% in winter).
   - The intake test leaves 8–49% of cells unclassified in any tick.
7. **Bodies (open question 3) need a rule change.** No parameter tried (mild world, share
   threshold, crowding cost, light) produced a body larger than 19 cells or more than one
   inner cell. Anchored cells exhaust the food within their reach. A rule giving inner
   cells some access to food, or making sharing more effective, would have to be designed
   and tested.

Not recommended, from the evidence: other mutation rates, more frequent outbreaks, dearer
movement, the generalist cost on its own, and leak 0.35.

### RULES.md open questions after M6

| # | question | status |
| --- | --- | --- |
| 1 | Starting food lets 50 cells establish? | Not tested (E2 not run). The defaults establish in every run |
| 2 | Winter harsh enough without extinction? | Winters bring populations to a few hundred cells but caused no E0 extinction. Dormancy stays low (about 0.1–0.2). Stronger belts or milder seasons raise the lows |
| 3 | Can a large body feed itself? | No: bodies stay at 19 cells at most in every setting |
| 4 | Do predators persist at drain 3? | Yes, but they crash their prey in about 1 run in 6; at 5 in nearly every run; at 2 less often |
| 5 | Is a 20% leak enough for partnerships? | A diet partnership appeared in 1 of 24 D1a runs (E0 had no lineage logs to check); a 35% leak did not help in 6 runs |
| 6 | Do viruses hold back the common lineage without wiping it out? | Yes: epidemics thin populations and caused no extinction. Resistance rises only during epidemics |
| 7 | Do producers appear, and do they crowd out consumers? | Producers appear in every run; they crowd out consumers only when photosynthesis is cheap (`photo_rate` 3) |
| 8 | Should the run end at extinction? | Still open; runs end at extinction |

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
- **The end of a run falls at a trough.** Tick 50,000 is mid-spring
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

## E4 Drain factor, E3 Bodies and E0 with 24 seeds (2026-10-06)

**Questions.**
- E4: does a drain factor of 3 let predators persist, or do they wipe out their prey and
  starve (RULES.md open question 4)? The E6 and E7 extinctions were mostly drain crashes.
- E3: can bodies grow beyond a few cells when all three changes are combined: the mild
  world, `share_threshold` 10 → 5 and `cost_crowding` 0.03 → 0.01?
- E0 with seeds 7–24: what is the baseline extinction rate? Six seeds may have been lucky.

**Params.**
- `e4_drain_2`: `drain_factor` 3 → 2.
- `e4_drain_5`: `drain_factor` 3 → 5.
- `e3_body_all`: as above.
- E0: no changes, seeds 1–24.

The E4 and E3 runs took 375 s and the 18 new E0 seeds 428 s.

| experiment | runs | lasts | diverse | both sides, gene | both sides, intake | cycles | balanced |
| --- | --- | --- | --- | --- | --- | --- | --- |
| e0_baseline, seeds 1–24 | 24 | **20** (extinct: seeds 8, 10, 11, 23) | 0 | 20 | 20 | 19 (seed 15: ×3.08, y13) | 0 |
| e4_drain_2 | 6 | 6 | 0 | 6 | 6 | 6 | 0 |
| e4_drain_5 | 6 | **1** | 0 | 1 | 1 | 1 | 0 |
| e3_body_all | 6 | 4 | 0 | 4 | 4 | 4 | 0 |

Means over seeds, years 6–25, rows with living cells:

| experiment | cells | yearly low, median (lowest) | harvest | photo | attack | defense | rows with ≥ 2 large clusters | largest body | in bodies | inner cells, most | gene producers | intake producers |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| e0_baseline (24 seeds) | 1,711 | 201 (1) | 0.90 | 0.94 | 0.40 | 0.54 | 9% | 9 | 8% | 0 | 66% | 72% |
| e4_drain_2 | 2,059 | 368 (13) | 0.88 | 0.94 | 0.30 | 0.45 | 11% | 9 | 7% | 0 | 73% | 74% |
| e4_drain_5 | 1,297 | 147 (1) | 0.91 | 0.89 | 0.48 | 0.46 | 2% | 8 | 9% | 0 | 46% | 72% |
| e3_body_all | 3,114 | 2,096 (4) | 0.94 | 0.70 | 0.37 | 0.76 | 2% | 11 | 7% | 1 | 13% | 34% |

How the extinctions happened (from the highest count in the run's last 2,500 ticks to 0):

| run | extinct at tick | collapse from | season at that point | drained / starved deaths | mean attack / defense |
| --- | --- | --- | --- | --- | --- |
| e0 seed 8 | 13,391 | 3,283 at 10,900 | +0.31 | 8,406 / 4,578 | 0.46 / 0.36 |
| e0 seed 10 | 28,269 | 1,039 at 26,000 | 0.00 | 1,424 / 1,443 | 0.50 / 0.40 |
| e0 seed 11 | 19,697 | 2,591 at 18,600 | +0.95 | 5,951 / 3,388 | 0.55 / 0.39 |
| e0 seed 23 | 35,570 | 1,552 at 34,900 | +0.31 | 1,158 / 1,866 | 0.61 / 0.49 |
| e4_drain_5 seed 1 | 9,569 | 2,331 at 8,700 | +0.81 | 6,722 / 1,843 | 0.43 / 0.19 |
| e4_drain_5 seed 2 | 45,695 | 302 at 45,100 | −0.31 | 45 / 552 | 0.89 / 0.81 |
| e4_drain_5 seed 3 | 21,305 | 1,721 at 20,400 | +0.95 | 5,359 / 2,636 | 0.69 / 0.57 |
| e4_drain_5 seed 4 | 13,519 | 2,363 at 12,200 | +0.59 | 9,283 / 3,378 | 0.46 / 0.38 |
| e4_drain_5 seed 6 | 12,162 | 1,746 at 10,800 | +0.59 | 4,520 / 1,109 | 0.43 / 0.28 |
| e3_body_all seed 3 | 15,374 | 3,477 at 14,100 | +0.31 | 12,374 / 3,265 | 0.78 / 0.63 |
| e3_body_all seed 6 | 9,544 | 3,836 at 8,300 | +0.81 | 9,207 / 3,246 | 0.25 / 0.51 |

**Answers**

- **The baseline extinction rate is about 1 in 6: 4 of 24 runs** (17%; a 95% interval for
  24 runs is roughly 5–37%). The first six seeds were lucky.
  - All four E0 extinctions started in spring or summer, with mean attack above mean
    defense.
  - In two of them drained deaths clearly outnumbered starved ones. In the other two
    (seeds 10 and 23) the two were about equal: the prey ran out and the predators then
    starved.
  - So under the defaults the main risk is a predator-prey crash, not winter.
- **E4: the drain factor decides whether predators destroy the world.**
  - At 5, five of six runs died, mostly in fast summer crashes. Drain caused 2.0–4.1×
    as many deaths as starvation, except in seed 2, which died slowly in winter after
    an arms race (attack 0.89, defense 0.81).
  - At 2, all 6 runs last and cycle. Attack falls to 0.30, and the median yearly low
    nearly doubles (368 against 201).
  - Six seeds can't show whether 2 removes the E0 risk of 1 in 6. Running about 24 seeds
    at drain 2 would answer that.
  - Open question 4: at 3, predators persist but crash their prey in about 1 run in 6.
    At 5 they nearly always do. At 2 they persist at lower attack and no crash happened.
- **E3: bodies do not grow even with all three changes.**
  - The largest body is 11 cells, 7% of cells are in bodies, mean adhesion is 0.07, and
    there was at most 1 inner cell. That is no better than the mild world alone (M4 found
    up to 11 cells) or than E9's mild bright world (19).
  - As agreed, the three changes are not tested one at a time, because the combined run
    showed no growth.
  - Two of 6 runs went extinct in drain crashes. Gene producers fall to 13%: in the mild
    world harvest stays high (0.94) and photosynthesis drops (0.70).
- **Diversity is still 0% everywhere.**

Plots: `runs/<experiment>/overview.png` (E0 now shows all 24 seeds).

## Proposed diversity experiments D0–D3 (2026-10-06)

**Why diversity fails so far.** In E0 a cell lives a median 209 ticks, about 10
generations a year. At the default mutation rate, two clonal lineages have to coexist for
roughly 10–20 years before their tags drift more than 0.1 apart. Meanwhile:
- sweeps, predator crashes and winters merge everything back into one lineage much sooner;
- at the evolved motility (mean 0.68), a lineage spreads across the 128-site grid in
  about 10 years.

Diversity therefore needs niches that last: places or ways of living that keep lineages
apart for decades.

| # | experiment | change | needs a RULES.md change? | status |
| --- | --- | --- | --- | --- |
| D0 | Three starting groups (diagnostic) | The 50 ancestors are split into 3 groups with tags spread evenly around the circle. Tests whether diversity lasts once it exists, separating "never arises" from "doesn't persist" | Only if adopted: RULES.md starts from 50 identical cells. Option `initial_tag_groups`, default 1 | approved, run |
| D1a | Stronger climate belts | `temp_latitude_amp` 5 → 10, `light_latitude_amp` 0.2 → 0.35 | No, params only | approved, run |
| D1b | Belts with milder seasons | D1a plus `temp_season_amp` 10 → 6 | No | not yet |
| D1c | Fertile spark patches | Sparks mostly inside a few fixed, seeded patches | Yes (the Sparks rule) | skipped for now |
| D2a | Dearer movement | `cost_move` 0.2 → 0.5 | No | approved, run combined with D1a |
| D2b | Slow ancestors | `ancestor.motility` 0.5 → 0.1 plus `cost_move` 0.5 | No to run it; yes if adopted (starting values) | not yet |
| D3a | Both feeding modes dearer | `cost_harvest` 0.2 → 0.4, `cost_photosynthesis` 0.3 → 0.5. A weak test: separate linear costs don't penalise doing both | No | not yet |
| D3b | Generalist cost | New upkeep item k × harvest × photosynthesis, at k = 0.4 and 0.8. Option `cost_generalist`, default 0 | Yes if adopted: a new upkeep row | approved, run |

Expectations:
- Slower movement alone is probably not enough: motility 0.68 → 0.2 only slows spreading
  by about 1.8×.
- D3b is the most direct test of separate producer and consumer lineages.

## D0, D1a, D1a + D2a, D3b, and E4 drain 2 with 24 seeds (2026-10-06)

**Params.**
- `d0_three_groups`: `initial_tag_groups` = 3. Tags start at 0.5, 0.833 and 0.167. The
  run writes a lineage log, which `tools/groups.py` uses to follow each group.
- `d1a_belts`: `temp_latitude_amp` 5 → 10, `light_latitude_amp` 0.2 → 0.35.
- `d1a_d2a_belts_move`: D1a plus `cost_move` 0.2 → 0.5.
- `d3b_generalist_0.4` and `_0.8`: `cost_generalist` 0.4 and 0.8.
- `e4_drain_2`: seeds 7–24 added to the 6 from before.

Run times: 415 s for the D1a and D3b runs, 112 s for D0, 486 s for drain 2.

| experiment | runs | lasts | diverse | both sides, gene | both sides, intake | cycles | balanced |
| --- | --- | --- | --- | --- | --- | --- | --- |
| e0_baseline | 24 | 20 | 0 | 20 | 20 | 19 | 0 |
| e4_drain_2 | 24 | 22 (extinct: seeds 7, 24) | 0 | 22 | 22 | 22 | 0 |
| d0_three_groups | 6 | 3 | 0 | 3 | 3 | 3 | 0 |
| d1a_belts | 6 | 6 | 0 | 6 | 6 | 6 | 0 |
| d1a_d2a_belts_move | 6 | 6 | 0 | 6 | 6 | 5 | 0 |
| d3b_generalist_0.4 | 6 | 6 | 0 | **0** | 6 | 6 | 0 |
| d3b_generalist_0.8 | 6 | 6 | 0 | **0** | 5 | 6 | 0 |

Means over seeds, years 6–25, rows with living cells:

| experiment | cells | yearly low, median (lowest) | harvest | photo | attack | defense | motility | share of cells infected | rows with ≥ 2 large clusters | largest body | gene producers | intake producers |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| e0_baseline (24) | 1,711 | 201 (1) | 0.90 | 0.94 | 0.40 | 0.54 | 0.72 | 6.7% | 9% | 9 | 66% | 72% |
| e4_drain_2 (24) | 1,959 | 271 (1) | 0.89 | 0.91 | 0.36 | 0.48 | — | 6.4% | 10% | 10 | 68% | 72% |
| d0_three_groups | 1,364 | 149 (1) | 0.92 | 0.92 | 0.39 | 0.51 | — | 5.1% | 6% | 7 | 51% | 65% |
| d1a_belts | 1,359 | 278 (3) | 0.91 | 0.95 | 0.32 | 0.64 | 0.74 | 2.9% | **23%** | 9 | 70% | 73% |
| d1a_d2a_belts_move | 1,496 | 468 (17) | 0.90 | 0.93 | 0.36 | 0.47 | 0.49 | 3.7% | 2% | 11 | 67% | 73% |
| d3b_generalist_0.4 | 765 | 348 (168) | 0.96 | 0.03 | 0.10 | 0.10 | 0.71 | 0.0% | 0% | 4 | 0% | 1% |
| d3b_generalist_0.8 | 721 | 283 (85) | 0.96 | 0.01 | 0.10 | 0.14 | 0.70 | 0.0% | 0% | 3 | 0% | 0% |

(Motility was computed for E0 over seeds 1–6 only; "—" means it was not computed.)

### D0: how long do the three starting groups stay separate clusters?

From `tools/groups.py`. A group's extinction tick is exact. "Not all separate" is checked
every 2,000 ticks. Daughters belong to their mother's group, and no mating between groups
happened.

| seed | first group extinct | second group extinct | census first below 3 large clusters | run |
| --- | --- | --- | --- | --- |
| 1 | 4,447 | 9,302 | 3,600 | extinct at 29,740 |
| 2 | 2,425 | 6,698 | 2,400 | extinct at 9,588 |
| 3 | 1,770 | 10,418 | 1,300 | extinct at 15,729 |
| 4 | 1,388 | 8,198 | 1,200 | one group to the end |
| 5 | 1,785 | 1,828 | 1,400 | one group to the end |
| 6 | 1,312 | 2,588 | 1,100 | one group to the end |

**Answers**

- **D0: diversity that exists at the start does not persist.**
  - The three groups stay three large clusters for only 1,100–3,600 ticks, the first one
    or two winters.
  - The census drops below 3 large clusters before the first group dies out: a group
    falls below 10 cells in a winter low, then dies soon after.
  - In every seed the groups stop being separate because a group dies out, not because
    their tags drift together. While both lived, their tags stayed well apart (e.g. 0.22
    in seed 6 at tick 2,000).
  - The second group is gone by tick 1,828–10,418. After that each run holds one
    lineage, and 3 of 6 runs went extinct.
  - The groups differ only in tag, which has no effect except kin recognition and
    viruses, so they compete as neutral lineages. Winter lows of a few hundred cells let
    one take over within a year or two. This is why diversity cannot arise either:
    without a niche, lineages coexist far less than the 10–20 years their tags need to
    drift apart.
- **D1a: stronger climate belts help a little, through diet, not climate.**
  - Two large clusters exist in 23% of rows, against 9% in E0. Three appeared in only
    1 row. All 6 runs last and cycle.
  - Mean preferred temperature stays near 17 °C, so no clear cold and heat specialists
    formed.
  - Seed 2 was rerun with a lineage log; its census matches the batch run. At tick 50,000
    its two large clusters were diet specialists in the same warm half of the world:
    90 cells (tags 0.18–0.31, diet 0.96) and 570 cells (tags 0.59–0.76, diet 0.13).
    That is the partners strategy from RULES.md. Both clusters still harvest and
    photosynthesise at about 0.9–0.97.
  - This is one run. Following clusters over time would need lineage logs for all seeds.
- **D1a + D2a: dearer movement did not help.** Motility fell from 0.74 to 0.49, and the
  median winter low rose to 468, but two large clusters existed in only 2% of rows. The
  diversity that D1a gained disappeared.
- **D3b: the generalist cost removes photosynthesis rather than splitting the population.**
  - At both k = 0.4 and 0.8, mean photosynthesis falls to 0.01–0.03. Gene producers go
    to 0%, and the population halves (721–765 cells).
  - Viruses die out (0% of cells infected), and both sides fails in all 12 runs under the
    gene test.
  - The ancestor is a harvester, so a mutant that starts to photosynthesise pays the
    generalist cost before it gains anything. That cost valley blocks any path to a
    producer specialist.
  - About 45–49% of cells have no gross intake in a given tick, against 12% in E0, which
    suggests food within reach is often used up.
  - D3b might work with a producer lineage present from the start (D0-style groups that
    differ in harvest and photosynthesis), or with a smaller k.
- **E4 drain 2 over 24 seeds: 2 extinctions (8%), against 4 of 24 (17%) at the default.**
  - With 24 runs each, this difference is not statistically clear.
  - The median yearly low rises from 201 to 271, and cycles pass in 22 of 24.
  - Drain 2 lowers the risk without removing it.

Plots: `runs/<experiment>/overview.png`.

## D1a with 24 seeds, and D1a with leak 0.35 (2026-10-06)

**Params.**
- `d1a_belts`, seeds 1–24, with lineage logs. Seeds 1–6 were rerun to write the logs.
- `d1a_leak_35`: D1a plus `leak_fraction` 0.2 → 0.35, 6 seeds, with lineage logs.

Run times: 622 s and 138 s. The logs (about 35 MB per run) were deleted after the analysis,
as were the D0 logs.

| experiment | runs | lasts | diverse | both sides, gene | both sides, intake | cycles | balanced |
| --- | --- | --- | --- | --- | --- | --- | --- |
| e0_baseline | 24 | 20 | 0 | 20 | 20 | 19 | 0 |
| d1a_belts | 24 | **24** | 0 | 23 | 24 | 22 | 0 |
| d1a_leak_35 | 6 | 6 | 0 | 6 | 6 | 5 | 0 |

| experiment | runs | cells | yearly low, median (lowest) | attack | defense | rows with ≥ 2 large clusters | seeds with ≥ 2 large clusters in > 25% of rows | rows with ≥ 3 large clusters |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| e0_baseline | 24 | 1,711 | 201 (1) | 0.40 | 0.54 | 9% | 1 | 1 |
| d1a_belts | 24 | 1,482 | 374 (3) | 0.31 | 0.60 | 10% | 3 | 3 |
| d1a_leak_35 | 6 | 1,499 | 388 (1) | 0.37 | 0.48 | 4% | 0 | 0 |

**Runs ending with two or more large clusters** (`tools/final_clusters.py`; home rows are
the shortest arc of rows, around the wrapping grid, that holds 80% of a cluster's cells).
Two of 24 D1a runs, and no D1a + leak run:

| run | cells | tags | diet | photo | harvest | preferred °C | home rows | mean latitude |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| d1a seed 2 | 570 | 0.59–0.76 | 0.13 | 0.97 | 0.91 | 15.0 | 100–32 (warm half) | +0.43 |
| d1a seed 2 | 90 | 0.18–0.31 | 0.96 | 0.97 | 0.94 | 15.7 | 111–38 (warm half) | +0.48 |
| d1a seed 11 | 1,287 | 0.05–0.21 | 0.54 | 0.95 | 0.94 | 11.0 | 18–100 (cold half) | −0.16 |
| d1a seed 11 | 327 | 0.33–0.70 | 0.53 | 0.95 | 0.62 | 18.7 | 109–27 (warm belt) | +0.66 |

Rows 0 and 127 are the warm, bright belt (latitude +1), and row 64 the cold, dim belt.

**Answers**

- **The diversity gain from D1a was luck.**
  - Over 24 seeds, two large clusters exist in 10% of rows, the same as E0 (9%). The 23%
    from the first 6 seeds did not hold up.
  - Three large clusters appear in only 3 rows across all 24 runs.
- **What D1a does is make runs safer.**
  - No D1a run went extinct, against 4 of 24 in E0. The median yearly low nearly doubles
    (374 against 201), and mean attack falls (0.31 against 0.40).
  - Six seeds would not have shown this either way; 24 do.
- **The two runs that end split show both kinds of niche the rules allow.**
  - Seed 2 is a diet partnership: an A eater (diet 0.96) and a B eater (diet 0.13) share
    the warm half.
  - Seed 11 is a climate split: a cold lineage (11 °C, cold half) and a warm one
    (18.7 °C, warm belt, harvest 0.62).
  - Both arise rarely, about 1 run in 12 each.
- **Leak 0.35 does not reward diet partners here.** No run ended with two large clusters,
  and two large clusters existed in 4% of rows. RULES.md open question 5 remains open: with
  6 seeds, more leak shows no sign of helping partnerships beat generalists.
