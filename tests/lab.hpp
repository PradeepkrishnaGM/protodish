#pragma once

// Helpers for building small scenes on a cleared, spark-free, spoilage-free world at
// tick 0 (season 0). Row 32 has latitude 0, so its temperature is 15 °C, its light 0.5
// and its upkeep multiplier exactly 1.

#include "world.hpp"

namespace lab {

constexpr int kW = 128;
inline int at(int row, int col) { return row * kW + col; }

inline evo::Params params() {
    evo::Params p;
    p.initial_cells = 0;
    p.spark_base = 0.0;
    p.spark_season_amp = 0.0;
    p.spoilage_rate = 0.0;
    return p;
}

inline void clear_ground(evo::World& w) {
    for (int s = 0; s < w.site_count(); ++s) w.set_site(s, 0.0, 0.0, 0.0);
}

// An immobile cell that does not feed.
inline evo::Genome still(const evo::Params& p) {
    evo::Genome g = p.ancestor;
    g[evo::kMotility] = 0.0;
    g[evo::kHarvest] = 0.0;
    return g;
}

inline evo::Genome eater(const evo::Params& p, double diet) {
    evo::Genome g = still(p);
    g[evo::kHarvest] = 1.0;
    g[evo::kDiet] = diet;
    return g;
}

}  // namespace lab
