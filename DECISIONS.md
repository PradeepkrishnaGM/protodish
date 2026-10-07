# Decisions

Interpretations of RULES.md where it is ambiguous or silent, and engineering choices
that affect results. Each entry says what was decided and when.

## M1 World (approved 2026-10-06)

### Rule interpretations

1. **Season curve.** `season(t) = sin(2π · (t mod 2000) / 2000)`. It is 0 and rising at
   tick 0, +1 at tick 500 (midsummer), 0 at tick 1000 and −1 at tick 1500 (midwinter).
2. **When the season applies.** Tick t uses `season(t)` for every phase of that tick. Tick 0
   uses sin(0) = 0.
3. **Latitude on the wrapping grid.** `latitude(row) = cos(2π · row / 128)`. It is +1 on row 0,
   −1 on row 64, and climbs back toward +1, so row 127 borders the warm row 0 with no seam.
4. **Spark count.** `round(375 + 250 × season)` to the nearest integer (half away from zero),
   recomputed each tick. No fraction is carried between ticks.
5. **Spark sites.** Each strike picks a site uniformly from all 16,384, with replacement. A
   site can be struck more than once per tick. Occupied sites can be struck.
6. **What a spark converts.** One fair coin per strike decides A or B. The strike converts
   `min(4, minerals on the site)` into that food. When a site is struck twice in one tick,
   the strikes apply one after the other, so the second sees what the first left.
7. **Spoilage.** 1% of the food A and 1% of the food B on every site turn into minerals each
   tick. It runs after sparks, so food made by a spark this tick also spoils.
8. **Disasters.** They fire at ticks 5,000, 10,000, … and never at tick 0. The 24 × 24 square's
   top-left corner is drawn uniformly (x, then y) and the square wraps around the edges.
   The position is drawn even when the square holds no cells, so the RNG sequence never
   depends on the population.
9. **Tick numbering.** The world starts at tick 0. `step()` runs tick t and then increments
   the counter. Tick-based rules (such as disasters) test t before the increment.

### Engineering choices

- **Determinism scope.** The same seed gives the same history on the same platform (same
  OS, compiler and standard library). Linux and Windows builds may differ slightly in
  floating point, so cross-platform runs are not guaranteed to match bit for bit.
- **No `-ffast-math`, ever.** The build also passes `-ffp-contract=off` so the compiler
  never fuses a multiply and add on its own.
- **Lookup tables.** The season curve (2,000 entries) and the latitude curve (128 entries)
  are computed once when the world is built. Per tick, temperature and light are computed
  per row (128 values) from these tables. They depend only on the row, so no per-site
  arrays are kept for them.
- **Own RNG.** PCG32 (XSH-RR) with hand-written integer and float mappings. Integer draws
  use Lemire's unbiased method, and uniform doubles use 53 bits from two 32-bit draws.
  `std::` distributions are not used, because their output differs between standard
  libraries.
- **Fixed draw order per tick (M1).** Sparks: for each strike, site index, then coin.
  Disaster (only on disaster ticks): x, then y.
- **Precision.** Food, minerals and (later) stores are `double`. Matter totals use
  Neumaier compensated summation.
- **Double buffering inside the environment phase.** Sparks copy the current buffer to the
  next buffer and apply strikes in order onto the next buffer (see item 6). Spoilage reads the
  current buffer and writes the next. The buffers swap after each sub-step.
- **State hash.** 64-bit FNV-1a over the raw bytes of the site arrays, the occupancy array,
  the tick counter and the RNG state.
- **CLI summary rows.** Each row is the state at the start of tick T (after T ticks have run),
  labelled T, with the season of tick T.

## M2 Ancestor life (approved 2026-10-06)

1. **Supply.** (Changed in M3, see M3-1.) `supply = (min(1, F / 20) + min(1, (M / 20) × light × photosynthesis)) / 2`,
   where F is the food within reach that the diet lets the cell eat,
   `Σ (diet × A + (1 − diet) × B)` over its reach, and M is the minerals within reach. Both
   are normalised by 20, as in Move. Each term is capped at 1.
2. **Mutation.** A mutating gene shifts by a uniform draw within ±10% of its range. Values
   that cross a range edge reflect back inside. The tag shifts within ±0.05 and wraps.
3. **Store overflow.** Any amount above 50, from any source, falls onto the cell's own site
   as food of the same kind.
4. **Death.** A cell that cannot pay its full upkeep pays nothing that tick. All of its stores
   and its body mass fall onto its site as food.
5. **Move conflicts.** When several cells pick the same empty site, the seeded RNG picks the
   winner. Each loser stays where it is and pays no move cost. A cell never counts itself as
   kin when scoring sites.
6. **Feeding from several sites.** A cell takes from its own site first, up to its capacity.
   The remaining demand is split equally across the empty sites in reach. If a site
   cannot cover its share, the shortfall is lost for that tick, with no second pass.
7. **Feed contention.** The own site is asked only by its occupant. Each empty site sums the
   requests on it. If they exceed what it holds, every request is scaled by the same factor
   (held / requested). All requests are computed from the current state and granted together.
8. **Costs for genes whose phases come later.** The full upkeep table applies from M2 on,
   including attack, defense, photosynthesis and resistance, even before those genes act.
9. **Two mothers, one empty site.** The RNG picks the winner. The loser does not divide, pays
   nothing, gets no cooldown and may try again next tick.
10. **Cooldown timing.** A cell that divides at tick t can divide again at tick t+6 at the
    earliest. Divide sets cooldown to 5. In the Divide phase, a cell with cooldown above 0
    decrements it and is not ready that tick.
11. **Tag mutation size.** ±0.05, as RULES.md states explicitly, not 10% of the tag's range.
12. **All 20 genes mutate from M2 on.** Adhesion, mating and role split have no effect until
    M4. Until then every daughter is a released clone and every cell is outer.
13. **Extinction.** For now the run continues as an empty world, and the CLI reports the tick
    when the last cell died. RULES.md open question 8 remains open. (Superseded after M2:
    see "Between M2 and M3".)
14. **Sense values hold for the whole tick.** Thermal efficiency and supply are computed once in
    Sense, at the cell's site at the start of the tick, and travel with the cell if it moves.
    Feed uses the Sense value. Upkeep's temperature multiplier uses the cell's current site,
    as RULES.md says "the total is multiplied by 0.5 + temperature / 30".

### Engineering choices (M2)

- **Two-pass phases for cell state** (accepted after M2 in place of literal double buffering). Each phase first gathers every decision from the current
  state, without writing (targets, requests, costs, ready mothers). It then commits all of
  them. Nothing a cell does in a phase is seen by another cell in the same phase, which is
  the effect double buffering is meant to give. The difference is that the 30 cell arrays
  aren't copied every phase. Site food arrays are committed the same way.
- **Iteration order.** Every phase visits cells in ascending site index. Neighbor directions are
  ordered N, NE, E, SE, S, SW, W, NW.
- **Fixed draw order (M2).**
  - Move: each awake free cell draws its motility chance. A cell that moves, with more than
    one best site, draws once to break the tie. Contested sites are then resolved in
    ascending target order, with one draw per contested site.
  - Divide: each ready cell with more than one empty neighbor draws its daughter's site.
    Contested sites are resolved as in Move. Then, in ascending mother order, each daughter
    draws per gene: one draw for the chance and, if it hits, one for the shift.
- **IDs.** IDs start at 1 and count up. 0 means "no parent", which is what the 50 ancestors have.

**M2 result (accepted 2026-10-06).** With RULES.md values, all 8 seeds tested went extinct between
ticks 2,708 and 9,345. The ancestor (preferred 15 °C) loses energy above about 23 °C, and the
population collapses every summer. Balance is left for M6 tuning.

## Between M2 and M3 (approved 2026-10-06)

- **Extinction ends the run.** The CLI stops in the tick the last cell dies, writes a final
  row and reports the tick. RULES.md open question 8 (should ancestors drift back in?) stays
  open. A run that starts with 0 cells is not "extinct" and runs its full length.
- **Params files.** `evolve --params FILE` applies `key = value` lines on top of the RULES.md
  defaults.
  - Keys are the `Params` field names, or `ancestor.<gene>` with the gene names from
    `genome.hpp`.
  - `#` starts a comment.
  - These are errors and stop the run before it starts: unknown or duplicate keys,
    unparsable or non-finite values, ancestor genes out of range, and values that fail
    `validate_params` (for example, `clone_cost` not equal to daughter store plus body mass).
  - The field list exists once (the `EVO_PARAMS` X-macro), so struct fields and file keys
    cannot drift apart.
  - `--dump-params` prints the effective values in the same format.
  - With no file, a run is bit-identical to the defaults.

## M3 Conflict and stress (approved 2026-10-06)

1. **Supply is the better of the two ways to eat.**
   `supply = max(min(1, F / 20), min(1, (M / 20) × light × photosynthesis))`, with F and M
   as in M2-1. This departs from RULES.md's "(… + …) / 2", under which no consumer could
   reach a supply above 0.5, so a dormancy gene above 0.5 would mean permanent dormancy.
   (Written into RULES.md on 2026-10-06, after M6. The engine did not change.)
2. **Photosynthesis draws minerals the way feeding draws food.** Own site first, then the
   remaining demand split equally over the empty sites in reach. An over-asked site splits
   in proportion, and there is no second pass. Light is taken at the cell's current site;
   thermal efficiency is the Sense value (M2-14).
3. **Only awake cells are threats.** A dormant cell cannot attack, so Move's threat count
   ignores it. Dormant cells can still be prey, with their defense doubled.
4. **Satiation uses combined room.** The room is (50 − A) + (50 − B), measured at the start of
   Attack. An attacker's total drain over all its prey is capped at 2 × room and shared out
   in proportion. Victim scaling (several attackers taking more than the victim holds) is
   applied after the satiation cap.
5. **Leak applies to gross Feed intake only.** That means food taken plus food made, per
   kind, not drained food. Leak goes to the cells on occupied neighbor sites, dormant ones
   included, split evenly. Received leak is not leaked again. Overflow above 50 is applied
   once, after keeps and leak receipts are added.
6. **Poor-environment stress.** Capacity is A capacity + B capacity + photosynthesis
   capacity. Intake is gross Feed intake before leak, excluding received leak and drained
   food. The stress rises when intake < 0.5 × capacity; a cell with capacity 0 never gets
   it.
7. **Dormant cells are frozen in time.** Stress neither fades nor rises, age does not
   increase, and the division cooldown does not count down.
8. **Death cause.** `Drained` if the cell lost any matter to an attack in its final tick,
   otherwise `Starved`. Disaster kills are always `Disaster`.

### Engineering choices (M3)

- **Relations are evaluated on demand.** `is_kin` and `treats_as_prey` depend only on genes
  and dormancy, not position, so Move and Attack evaluate them after cells have moved.
  No neighbor masks are stored.
- **Attack commit order.** All drains are subtracted from victims first, using proportions
  from the victims' stores at the start of Attack. Then attackers' gains and scraps are
  added. A cell that both attacks and is attacked is therefore handled the same way
  regardless of site order.
- **M3 adds no RNG draws.** Dormancy, leak, photosynthesis and attack are deterministic.
- **Test labels.** Long invariant runs are in the doctest suite `slow` (ctest label `slow`); everything
  else is `fast`. `ctest -LE slow` is the quick check. The full `ctest` runs before every
  milestone commit.
- **Speed (M3).** About 750 ticks/s at about 3,000 cells, so 50,000 ticks takes about 1 minute.
  Accepted for now; optimisation is scheduled for M6.

**M3 result (accepted 2026-10-06).** With RULES.md values (and M3-1), the 3 test seeds now survive
10,000 ticks. Photosynthesis, dormancy and, in one seed, attack evolve from zero.

## M4 Bodies (approved 2026-10-06)

1. **Role-split values are not clamped.** Effective harvest, attack and defense may leave
   the gene range (for example 0.8 × 1.5 = 1.2). Upkeep scales with them, which keeps the
   effect self-limiting.
2. **Upkeep uses the adjusted values.** Harvest, attack and defense costs use the
   role-adjusted values ("every later rule uses these adjusted values"). The dormant ×2 on
   defense is applied on top; the two multiplications commute.
3. **Daughters bond only to cells that existed at the start of Divide.** An attached daughter
   bonds to her mother and to every pre-existing cell next to her site that is bonded to the
   mother. Daughters born in the same tick never bond to each other, so the result does not
   depend on processing order.
4. **Mating partners.** A partner must be ready by the full test, with the shared empty site
   counting as its empty neighbor. It must not be bonded to the mother, and the two must
   regard each other as kin. It must not be a mother winning its own division this tick,
   and it must not already be a partner this tick. A cell takes part in at most one birth
   per tick. Ready cells that lost a contested site are eligible.
5. **Bodies have at least 2 cells.** Bodies are the connected groups of the bond graph and
   are not stored. Free cells are counted separately. This only matters for the M6 census.
6. **Divide draw order.** For each winning mother, in ascending site order:
   - adhesion;
   - the mating chance, only if the daughter is released;
   - the partner choice, only if there is more than one candidate;
   - one coin per gene, in table order, only when mating;
   - mutation as before.

### Engineering choices (M4)

- **Bond storage.** Each cell has an 8-bit mask, one bit per direction (N, NE, …, NW).
  Bonds are kept symmetric: bit d at s ⇔ bit (d + 4) mod 8 at neighbor(s, d). Bonded cells
  never move, so masks are never remapped.
- **Matings conserve matter.** `validate_params` requires 2 × `mating_cost` = daughter
  store + body mass, as it already does for `clone_cost`.

**M4 result (accepted 2026-10-06).** Under RULES.md defaults, after 10,000 ticks 2–12% of cells
are in bodies of 2–4 cells, mean adhesion is about 0.05, and matings are rare (1–3 per run). In a
mild test world, bodies reach about 11 cells at most. Adhesion ≥ 0.9 dies out, and inner cells
almost never form, because anchored cells exhaust the food within reach.

## M5 Viruses (approved 2026-10-06)

1. **One snapshot per Infect phase.** Every virus rule reads the infection state at the start
   of the phase. A cell infected this tick cannot pass the virus on until the next tick. A
   cell that recovers this tick still makes its spread attempts this tick. Burden and
   infection stress are judged at Upkeep, so a new case pays this tick and a cell that
   recovered this tick does not.
2. **Dormant infected cells are frozen.** They neither catch, pass nor clear a virus
   (consistent with M3-7). They still pay the infection cost inside their one-tenth upkeep.
   Their stress is frozen, so they gain no infection stress.
3. **Several viruses reach one cell.** Each successful spread attempt is an intent. If more
   than one reaches the same healthy cell, the RNG picks one, as in Move and Divide.
4. **Drift happens after the match.** The match is checked with the source's virus tag, then
   the passing copy may drift. A drifted virus that no longer matches its new host stays in
   that host. The source keeps its own tag. Outbreak viruses carry the host's tag exactly
   and do not drift.
5. **Viruses carry no matter.** The virus dies with its host and moves with it.
   Daughters are born healthy.
6. **Spread and resistance use the raw genes.** Role split does not adjust resistance.

### Engineering choices (M5)

- **Storage.** `infected` (uint8) and `virus_tag` (double) per site. Both are in the state hash.
- **Fixed draw order (M5).** All in one Infect phase:
  - Spread: cells infected and awake at the start of the phase, in ascending site order.
    For each, directions N to NW, with one draw per awake, healthy, matching neighbor.
  - Conflicts: in ascending target order, one draw per target with more than one source.
  - Drift: in ascending target order, one draw for the chance and, if it hits, one for the shift.
  - Recovery: one draw per carrier from the spread step, in ascending order.
  - Outbreak: one draw per awake cell that was healthy at the start of the phase and did not
    catch a virus, in ascending order.
  - As elsewhere, a draw is made even when its chance is 0.
- **CLI.** The summary CSV gains an `infected` column after `cells`.
- **Test change.** The M1 disaster test fills the grid with 16,384 clones on 1 A + 1 B. It now
  sets `outbreak_chance = 0`, because an outbreak sweeps the grid and the burden starves the
  cells, which is not what that test is about.

**M5 result (accepted 2026-10-06).** Three seeds × 10,000 ticks, default world:

| outbreak chance | 0 | 1e-6 (default) | 1e-4 |
| --- | --- | --- | --- |
| epidemics (runs of ticks with ≥ 1 infected cell) | 0 | 5–8 | 1–6, mostly endemic |
| ticks with a virus present | 0 | 34–48% | 96–100% |
| peak infected cells | 0 | 860–2,240 | 1,690–2,890 |
| infected share of cell-ticks | 0 | 4–11% | 22–29% |
| mean cells, ticks 5,000–10,000 | 1,450–1,760 | 1,750–2,170 | 1,530–2,020 |
| mean resistance at 10,000 | 0.03–0.10 | 0.06–0.08 | 0.04–0.06 |

No run went extinct. At the default rate, an epidemic lasts up to about 2,800 ticks and dies
out. Neither rate gave resistance a clear advantage within 10,000 ticks. In the mild body
world (constant 15 °C, 1,500 sparks), mean population in ticks 5,000–10,000 was 3,070–3,720
without viruses, 2,600–3,990 at 1e-6 and 2,480–2,860 at 1e-4. Different settings change the
RNG stream, so each comparison is between different histories. These are indications, not
measurements.

## M6 Records and tuning (approved 2026-10-06)

### Rule interpretations

1. **Balanced run: peak-ratio target.** Years are 2,000 ticks, counted from 1. Year 5 is
   compared with year 4 first, then each later year with the one before it, up to year 25.
   A pair passes when the larger peak is at most 3 × the smaller. A year's peak is the
   largest cell count after any tick in it, from the census column `peak_cells`. Only
   complete years are compared. An extinct run fails this target as well as "it lasts".
2. **Balanced run: diversity.** Tag clusters follow RULES.md: the circle of living tags is
   cut wherever two neighboring tags are more than 0.1 apart (a gap of exactly 0.1 does not
   cut), and each arc between cuts is a cluster. With no cut, all cells form one cluster.
   The diversity target counts only clusters of at least 10 cells. The census writes both
   counts (`tag_clusters` and `tag_clusters_large`).
3. **Balanced run: "it lasts"** is judged at the end: cells alive at tick 50,000.
4. **Diversity and both sides are judged over the final year** (changed after E0,
   2026-10-06). The final year is the census rows with tick in (last − 2,000, last], which
   is 20 rows. A target passes when it holds in at least half of them. The final-tick result
   is reported too, but does not decide the target. Tick 50,000 is mid-spring (season 0,
   rising; see M7d for season names), right after the winter low in population, so the final
   tick alone was the harshest point to judge.
5. **Two producer tests.** The target uses the RULES.md gene test: a producer's
   photosynthesis gene is higher than its harvest gene. The census also reports an intake
   test. Which one becomes the target is decided after seeing both:
   - A producer made more than half of its gross Feed intake (M3-5: food taken plus food
     made, before leak) by photosynthesis in the tick just run. Every other cell with
     intake is a consumer.
   - Cells with no gross intake that tick count as neither: dormant cells, daughters born
     that tick, and cells living only on leak or drain. They are counted in `no_intake`.
   - Drained food is not part of gross intake, so a pure predator lands in `no_intake`,
     not among the consumers.

### Census (CSV, one row every `census_interval` = 100 ticks)

- Each row is the state at the start of tick T (after T ticks), with the season of tick T,
  like the old summary CSV, which the census replaces. It keeps the matter totals and the
  state hash.
- Columns: tick, year (T / year_length), season, cells, free cells, cells in bodies,
  bodies, bodies by size (2, 3–4, 5–8, 9–16, 17–32, 33+), largest body, inner cells,
  infected, dormant, producers, consumers, the intake-based producers, consumers and
  no-intake cells (M6-5), both tag-cluster counts, then the interval
  columns, the mean of each gene, matter totals and the hash.
- Interval columns cover the ticks since the previous row: `peak_cells`, births by kind
  and deaths by cause.
- `dormant` counts cells that were dormant in the tick just run. Daughters born in that
  tick were never sensed and are not counted.
- Gene means are over all living cells, dormant ones included (`nan` with no cells). The
  tag mean is written but means little, because the tag is circular.
- New Params fields `census_interval`, `cluster_gap` and `cluster_min_size` affect only the
  records, never the simulation.

### Lineage log (binary, optional)

- Written only with `evolve --lineage FILE`. Format in `core/records.hpp`: a 64-byte header
  (magic `EVOLIN01`, version, record sizes, seed, params hash), then 100-byte birth and
  16-byte death records, little-endian.
- Ticks, IDs and ages are u32, sites u16, and genes float32. Float32 is enough for the
  family tree, while replays come from the seed. Writing stops with an error if a value
  does not fit.
- Ancestors are logged as births of a fourth kind, `ancestor`, at tick 0, so the tree has
  roots.
- `LineageWriter` lives in the core (no Godot code), buffers about 1 MB and is fed by the
  caller after each `step()`. `phase_record` stays empty.
- Measured size: 7.7 MB per 10,000 ticks in the default world, so 40–100 MB for 50,000
  ticks. The census is about 0.5 MB.
- `tools/lineage_to_csv.py` dumps a log to `.births.csv` and `.deaths.csv`.

### Engineering choices (M6)

- **Golden hashes.** `tests/data/golden_hashes.txt` holds the state hash every 1,000 ticks,
  up to 10,000, for seeds 1–3 in the default world and in the mild world
  (`tests/data/mild.params`). They were recorded from commit 87ccaf0, before any speed
  work. The slow test `golden` checks them. A deliberate rule change has to regenerate
  the file and say why here.
- **Speed work.** Every change keeps all golden hashes identical:
  - The occupied-site list is rebuilt only in Sense, Feed and Divide, the phases that
    follow a change in occupancy (disaster, Move, Upkeep deaths). The scan is branch-free.
  - Move works out each cell in the 5 × 5 window around a mover once, instead of up to 3
    times per candidate site. Window sites are reached through the neighbor table.
  - Phases reuse scratch buffers that belong to the World, instead of allocating per tick.
    Per-site buffers are zero on entry and each phase resets what it touched.
  - The prey test runs its cheapest comparison first, and the relation helpers are
    inline.
  - LTO was tried and gave nothing, so it is not used.

  | single process, seed 1, 10,000 ticks | before | after |
  | --- | --- | --- |
  | default world | 808 ticks/s | 1,112 ticks/s |
  | mild world | 537 ticks/s | 783 ticks/s |

  This CPU is an i5-10500T (6 cores, 12 MB shared L3). With 6 runs at once, each runs at
  about 470 ticks/s, about 2,850 ticks/s in total, limited by shared cache and lower
  all-core clocks. The largest remaining cost is Move's commit, which copies about 40
  per-site arrays for every moving cell. Going further would mean storing genes per cell
  (array of structs) rather than per gene, which departs from the structure-of-arrays
  rule in the project's working rules and is not done.
- **Tools** (`tools/`): `check_balance.py`, `batch.py` and `lineage_to_csv.py` use only the
  standard library. `plot_run.py` needs matplotlib, installed in `.venv` from
  `requirements.txt`. The batch runner starts one single-threaded `evolve` per job and
  resumes interrupted batches.
- **Epidemic analysis** (`tools/epidemics.py`, M6). An epidemic is a stretch of consecutive
  census rows with at least one infected cell (M5's definition, at census resolution).
  Only epidemics peaking at 50 or more infected cells are analysed. One that covers more
  than half the run counts as endemic and has no before or after. For each epidemic it
  reports the large-cluster count before, the lowest and highest during, and the count
  1,000 ticks after. Control windows are virus-free, have the same length, and start at
  the same time of year (within 200 ticks), from the same runs or from `--control` runs.
  Each epidemic's matched windows are averaged, so every epidemic weighs the same.
- **Experimental options (approved 2026-10-06).** These are not in RULES.md. Each defaults
  to the RULES.md behavior, and the golden hashes are unchanged with the defaults. They are
  written into RULES.md only if adopted.
  - `initial_tag_groups` (D0, default 1): ancestor i joins group i mod G, with tag
    ancestor.tag + g / G, wrapped. With G = 3 the groups start at 0.5, 0.833 and 0.167,
    with 17, 17 and 16 cells. No extra random draws are made, so the ancestors' sites are
    the same as with G = 1.
  - `cost_generalist` (D3b, default 0): one more upkeep item, k × harvest × photosynthesis,
    added last. Harvest is the role-adjusted value, as for the harvest cost (M4-2). It is
    scaled by temperature and dormancy like the rest of upkeep. With k = 0 it adds exactly
    +0.0.
- **`tools/groups.py`** follows D0's founding groups through the lineage log. A daughter
  belongs to her mother's group. Groups are "separate" while every pair of groups is more
  than 0.1 apart in tag.

## Project name (2026-10-07)

- **The project is renamed from Evolving Life to Protodish.** The new name is used in the
  title of RULES.md and in its overview. There is no README yet;
  M8 will write it under the new name.
- **Unchanged:** the C++ namespace `evo`, the `evolve` binary, the folder
  `evolving-life`, the CMake project name `evolving_life`, the `EVO_` macro prefix and the
  lineage-log magic `EVOLIN01`. The last of these keeps existing logs readable.

## M7 Godot app (plan approved 2026-10-07)

### App behavior

1. **End world** kills every cell the way a death does: its stores and body mass fall onto
   its site as food, so total matter is unchanged. The world then stays paused until
   Restart.
2. **Lineages** in the statistics panel shows both tag-cluster counts: all clusters (the
   RULES.md definition) and those with at least `cluster_min_size` cells (the M6 diversity
   target), as "12 (4 with ≥ 10 cells)".
3. **Speed** runs from 1 tick every 8 frames up to 64 ticks per frame, in powers of two. The
   slower settings let single cells be followed.
4. **No lineage log in the app.** The app is for watching; the CLI records.
5. **Presets.** A menu loads a params file: "Default (RULES.md)" and "Stable (climate belts,
   D1a)" to start. Changing the preset restarts the world.
6. **Extinction** pauses the app and shows the tick the last cell died, as the CLI ends its run.

### Engineering choices (M7)

- **Versions.** Godot 4.7.2 (Fedora package). godot-cpp is a submodule in `extern/godot-cpp`,
  pinned at tag 10.0.0-stable. Since v10, godot-cpp is versioned apart from Godot and picks
  the API with `GODOTCPP_API_VERSION`. It is set to 4.7. Its bundled
  `extension_api-4-7.json` is identical, apart from the header, to the output of
  `godot --dump-extension-api` from the installed 4.7.2. `compatibility_minimum` is 4.7.
- **Build.** `cmake -S . -B build-godot -G Ninja -DEVO_BUILD_GODOT=ON`, then
  `ninja -C build-godot`. The option is off by default, so the core, CLI and tests build
  without godot-cpp. With it on, `evo_core` is built as position-independent code and
  linked into `godot/bin/libprotodish.linux.template_debug.x86_64.so`. The golden hashes
  still pass. godot-cpp headers are system headers, so our warning flags skip them.
- **libstdc++ is linked dynamically for local builds.** Fedora ships no static libstdc++.
  M8 release builds turn `GODOTCPP_USE_STATIC_CPP` back on.
- **Renderer.** Compatibility (`gl_compatibility`), for Intel graphics viewed over Remote
  Desktop.
- **Wrapper.** `ProtodishWorld` (RefCounted) owns one `evo::World` and holds no simulation
  logic. Ticks run on the main thread, N `step()` calls per frame. The view is painted in
  C++ into a 128 × 128 RGB8 Image, one pixel per site, shown scaled up with
  nearest-neighbor filtering. The state hash is exposed as 16 hex digits, because it does
  not fit a signed 64-bit integer.
- **`.godot/extension_list.cfg` is written after each build** (`cmake/write_extension_list.cmake`),
  if it is missing. Without it, a run of the project without the editor does not load the
  extension. The editor's first scan of the project also aborts in Godot 4.7.2
  (`EditorHelp::_gen_extensions_docs`) when it finds the extension only during the scan;
  with the file present beforehand it does not (3 of 3 runs against 0 of 3).
- **Tests.** ctest label `godot`: `godot_import` (headless import, a fixture) and
  `godot_headless` (`godot/tests/run_tests.gd`). They check that the class is registered,
  that the wrapper's hash after 1,000 ticks equals the CLI's for the same seed, that runs
  are deterministic, and the rendered images. Snapshots go to `build-godot/godot_out/`.
  `godot/tests/screenshot.gd` saves a full-window screenshot under `xvfb-run`.
- **Run the app:** `godot --path godot`.
- **View colors live in the core** (`core/view.{hpp,cpp}`, M7b). The painter has no Godot code and
  only reads the world, so doctest checks the exact colors on hand-built worlds. The wrapper copies
  its buffer into the Image. Display constants (colors, ground scale) are named constants there,
  not Params, because they do not affect the simulation.
- **View modes (M7b).** Empty sites are dark in every mode except Ground.
  - **Lineage:** hue = tag (saturation 0.85, value 0.95).
  - **Energy:** A + B in store, from red at 0 through amber to pale yellow at `store_max` (50)
    or more. Amber, the midpoint, is about the 12 A + 12 B a cell needs to divide. The red end was
    raised in M7c, after review, to (190, 60, 60). Its contrast ratio against the background is
    above 3, and a test checks it.
  - **Feeding type:** green producer, orange consumer, by the RULES.md gene test
    (photosynthesis > harvest).
  - **Infection:** infected cells in the hue of their virus tag, healthy cells grey.
  - **Ground:** red = food A, green = food B, blue = minerals, each sqrt(amount / full) with
    full = 12 for food and 50 for minerals. That is about the 99th percentile per site in default
    runs at ticks 2,500–10,000, where most sites hold under 2 food and minerals reach 180.
    Cells are not drawn.
  - **Ground layers (M7c, after review):** Ground: food A, Ground: food B and Ground: minerals show
    one layer alone. They run from black through the layer's color, reached at 0.7 on the same
    square-root scale, to white at "full" or more. The view menu keeps the five RULES.md modes, and a
    second menu, shown only for Ground, picks All layers or one layer.
- **Legend (M7c).** `view_legend()` in the core builds each mode's legend from the same colors the
  painter uses, so the two cannot drift apart. It is made of swatches and 16-color gradients with
  labels at both ends. Swatches have a thin grey border, so the empty-site swatch shows on the dark
  panel.
- **Controls (M7c).**
  - Restart uses the seed in the field. If the field does not hold a whole number of 0 or more, the
    current seed is used and shown again. Enter in the field restarts too.
  - New seed restarts with a random seed from 1 to 999,999,999. It is drawn from Godot's own random
    source, never the simulation's.
  - The speed slider has 10 steps: 1 tick every 8, 4 or 2 frames, then 1 to 64 ticks per frame.
    The default is 4 ticks per frame.
  - Keys: Space starts and pauses; 1–5 pick the five views.
- **End world** calls `World::clear_cells()`. It records no death events, because no rule killed the
  cells and the app writes no lineage log. The world is not marked extinct. The wrapper refuses to
  step an ended world until Restart.
- **Presets** are the `*.params` files in `godot/presets/`. Default is listed first, the rest in name
  order, and each file's `# name:` line is its label. GDScript reads the text with `FileAccess`,
  and the wrapper applies it with `apply_params` and `validate_params`. A bad preset is reported in
  the status line and the current world is kept. Reading text rather than a file path keeps
  presets working from an exported build. M8 must add `*.params` to the export filter.
- **No `class_name` in the app scripts.** Scripts are preloaded as constants, because `class_name`
  resolves only through the editor's class cache. A fresh checkout run with `godot --path godot`
  would fail to parse without it.
- **Statistics panel (M7d).**
  - It updates every 0.2 s, on Restart and on End world. The left status line shows only the run
    state (running, paused, extinct, ended), so tick and cell counts appear in one place.
  - **Year** is counted from 1, as in M6-1: year 1 is ticks 0–1,999.
  - **Season name** (display only): each season is the quarter of the year centered on its peak,
    because RULES.md calls season +1 "midsummer" and −1 "midwinter". Spring is ticks 1,750–249 of the
    year, summer 250–749, autumn 750–1,249 and winter 1,250–1,749. Tick 0 is mid-spring, matching
    "a run starts in spring, at 0 and rising". Approved 2026-10-07; M6-4 and TUNING.md were
    corrected to match (they had called tick 50,000 "the first tick of a spring"). The season value
    is shown too.
  - **Temperature and light** are the lowest and highest over the 128 rows in the tick just run.
  - **Lineages** shows both cluster counts (M7-2). Producers and consumers use the RULES.md gene
    test.
  - **Matter** shows cells, food (A + B, with each kind) and minerals, each with its share of the
    total.
  - `take_census(w, with_hash = false)` skips the state hash, which took about 4.5 of the 5 ms per
    census at 2,000 cells. The CLI keeps the hash. A panel update now costs about 0.4 ms.
- **Population graph (M7d).** It shows cells, producers and infected cells over the whole run, with
  faint lines at year boundaries.
  - `PopulationHistory` (`core/history.{hpp,cpp}`) records the world after every tick, so a crash
    inside one frame still shows. Each graph point is the peak of the ticks it covers.
  - Memory is bounded: at 1,048,576 samples, neighboring samples merge in pairs, keeping peaks, and
    each sample then covers twice as many ticks. That is about 12 MB at most.
  - Recording costs nothing measurable: the wrapper runs 1,177 ticks/s, the CLI 1,055 ticks/s
    (seed 1, 10,000 ticks).
- **UI tests.** `godot_ui` (`godot/tests/ui_tests.gd`) loads the real scene headless and presses
  every control through its signals. It covers speed (exact ticks per frame), pause, seed, presets,
  view and layer menus, the legend, End world and Restart.
