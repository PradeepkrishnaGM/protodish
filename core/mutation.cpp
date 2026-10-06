#include "mutation.hpp"

#include <algorithm>
#include <cmath>

namespace evo {

double mutation_chance(double mutability_times_stress, const Params& p) {
    return p.mutation_base * (1.0 + p.mutation_stress_factor * mutability_times_stress);
}

void mutate(Genome& g, double chance, Rng& rng, const Params& p) {
    for (std::size_t i = 0; i < kGeneCount; ++i) {
        if (!rng.chance(chance)) continue;
        const GeneInfo& info = kGeneInfo[i];
        const double step = info.circular ? p.tag_mutation_step
                                          : p.mutation_step * (info.max - info.min);
        double v = g[i] + (2.0 * rng.uniform() - 1.0) * step;
        if (info.circular) {
            v -= std::floor(v);  // wrap into [0, 1)
            if (v >= 1.0) v = 0.0;  // -tiny wraps to 1.0 after rounding
        } else {
            if (v > info.max) v = 2.0 * info.max - v;
            if (v < info.min) v = 2.0 * info.min - v;
            v = std::clamp(v, info.min, info.max);  // guards rounding only
        }
        g[i] = v;
    }
}

}  // namespace evo
