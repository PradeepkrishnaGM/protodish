#pragma once

#include "genome.hpp"
#include "params.hpp"
#include "rng.hpp"

namespace evo {

// Chance per gene: base × (1 + factor × p), where p is mutability × stress
// (averaged over both parents in a mating).
double mutation_chance(double mutability_times_stress, const Params& p);

// Each gene, in table order, mutates with `chance`. A mutating gene shifts by a uniform
// amount within ±mutation_step of its range and reflects at the edges. The tag shifts
// within ±tag_mutation_step and wraps. Draws: one per gene, plus one per mutation.
void mutate(Genome& g, double chance, Rng& rng, const Params& p);

}  // namespace evo
