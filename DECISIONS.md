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
