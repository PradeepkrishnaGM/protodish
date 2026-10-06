#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "environment.hpp"
#include "params.hpp"
#include "rng.hpp"

namespace evo {

struct MatterTotals {
    double food_a = 0.0;
    double food_b = 0.0;
    double minerals = 0.0;
    double cells = 0.0;  // stores + body mass of every living cell
    double total() const { return food_a + food_b + minerals + cells; }
};

class World {
public:
    World(const Params& params, std::uint64_t seed);

    // Runs the ten phases of the current tick, then advances the tick counter.
    void step();

    const Params& params() const { return params_; }
    const Climate& climate() const { return climate_; }
    std::uint64_t tick() const { return tick_; }
    std::uint64_t seed() const { return seed_; }
    int site_count() const { return n_sites_; }

    // Season, temperature and light of the most recently prepared tick
    // (tick 0 before the first step, otherwise the tick just run).
    double season() const { return season_; }
    double row_temperature(int row) const { return row_temperature_[static_cast<std::size_t>(row)]; }
    double row_light(int row) const { return row_light_[static_cast<std::size_t>(row)]; }

    std::span<const double> food_a() const { return cur_.food_a; }
    std::span<const double> food_b() const { return cur_.food_b; }
    std::span<const double> minerals() const { return cur_.minerals; }
    std::span<const std::uint8_t> occupied() const { return occupied_; }

    // Placeholder occupancy until cells exist (M2). Used by tests of disasters.
    std::span<std::uint8_t> occupied_mut() { return occupied_; }

    const std::optional<DisasterEvent>& last_disaster() const { return last_disaster_; }

    MatterTotals matter() const;
    std::uint64_t state_hash() const;

private:
    void prepare_climate();

    void phase_environment();
    void phase_sense() {}
    void phase_move() {}
    void phase_feed() {}
    void phase_attack() {}
    void phase_share() {}
    void phase_infect() {}
    void phase_upkeep() {}
    void phase_divide() {}
    void phase_record() {}

    Params params_;
    Climate climate_;
    Rng rng_;
    std::uint64_t seed_;
    int n_sites_;
    std::uint64_t tick_ = 0;

    double season_ = 0.0;
    std::vector<double> row_temperature_;
    std::vector<double> row_light_;

    SiteState cur_;
    SiteState next_;
    std::vector<std::uint8_t> occupied_;

    std::optional<DisasterEvent> last_disaster_;
};

}  // namespace evo
