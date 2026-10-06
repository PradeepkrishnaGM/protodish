#include "environment.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace evo {

Climate::Climate(const Params& p) : p_(p) {
    season_table_.resize(static_cast<std::size_t>(p.year_length));
    for (int t = 0; t < p.year_length; ++t) {
        season_table_[static_cast<std::size_t>(t)] =
            std::sin(2.0 * std::numbers::pi * t / p.year_length);
    }
    latitude_table_.resize(static_cast<std::size_t>(p.grid_height));
    for (int row = 0; row < p.grid_height; ++row) {
        latitude_table_[static_cast<std::size_t>(row)] =
            std::cos(2.0 * std::numbers::pi * row / p.grid_height);
    }
}

double Climate::temperature(double season, int row) const {
    const double t = p_.temp_base + p_.temp_season_amp * season + p_.temp_latitude_amp * latitude(row);
    return std::clamp(t, p_.temp_min, p_.temp_max);
}

double Climate::light(double season, int row) const {
    const double l = p_.light_base + p_.light_season_amp * season + p_.light_latitude_amp * latitude(row);
    return std::clamp(l, p_.light_min, p_.light_max);
}

int Climate::spark_count(double season) const {
    const long n = std::lround(p_.spark_base + p_.spark_season_amp * season);
    return static_cast<int>(std::max(0L, n));
}

void apply_sparks(const SiteState& cur, SiteState& next, int count, Rng& rng, const Params& p) {
    next.food_a = cur.food_a;
    next.food_b = cur.food_b;
    next.minerals = cur.minerals;
    const auto n_sites = static_cast<std::uint32_t>(cur.minerals.size());
    for (int s = 0; s < count; ++s) {
        const std::uint32_t site = rng.below(n_sites);
        const bool to_a = rng.below(2) == 0;
        const double amount = std::min(p.spark_amount, next.minerals[site]);
        next.minerals[site] -= amount;
        (to_a ? next.food_a : next.food_b)[site] += amount;
    }
}

void apply_spoilage(const SiteState& cur, SiteState& next, const Params& p) {
    const std::size_t n = cur.minerals.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double spoiled_a = cur.food_a[i] * p.spoilage_rate;
        const double spoiled_b = cur.food_b[i] * p.spoilage_rate;
        next.food_a[i] = cur.food_a[i] - spoiled_a;
        next.food_b[i] = cur.food_b[i] - spoiled_b;
        next.minerals[i] = cur.minerals[i] + spoiled_a + spoiled_b;
    }
}

DisasterEvent apply_disaster(std::vector<std::uint8_t>& occupied, std::uint64_t tick, Rng& rng,
                             const Params& p) {
    DisasterEvent ev;
    ev.tick = tick;
    ev.x = static_cast<int>(rng.below(static_cast<std::uint32_t>(p.grid_width)));
    ev.y = static_cast<int>(rng.below(static_cast<std::uint32_t>(p.grid_height)));
    for (int dy = 0; dy < p.disaster_size; ++dy) {
        const int row = (ev.y + dy) % p.grid_height;
        for (int dx = 0; dx < p.disaster_size; ++dx) {
            const int col = (ev.x + dx) % p.grid_width;
            occupied[static_cast<std::size_t>(row * p.grid_width + col)] = 0;
        }
    }
    return ev;
}

}  // namespace evo
