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

1. **Supply.** `supply = (min(1, F / 20) + min(1, (M / 20) × light × photosynthesis)) / 2`,
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
