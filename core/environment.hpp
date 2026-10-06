#pragma once

#include <cstdint>
#include <vector>

#include "params.hpp"
#include "rng.hpp"

namespace evo {

// Food and minerals on every site, one array per quantity.
struct SiteState {
    std::vector<double> food_a;
    std::vector<double> food_b;
    std::vector<double> minerals;

    void resize(std::size_t n) {
        food_a.assign(n, 0.0);
        food_b.assign(n, 0.0);
        minerals.assign(n, 0.0);
    }
};

// Season and latitude lookup tables, plus the climate formulas built on them.
class Climate {
public:
    explicit Climate(const Params& p);

    double season(std::uint64_t tick) const {
        return season_table_[static_cast<std::size_t>(tick % season_table_.size())];
    }
    double latitude(int row) const { return latitude_table_[static_cast<std::size_t>(row)]; }

    double temperature(double season, int row) const;
    double light(double season, int row) const;
    int spark_count(double season) const;

private:
    Params p_;
    std::vector<double> season_table_;
    std::vector<double> latitude_table_;
};

struct DisasterEvent {
    std::uint64_t tick = 0;
    int x = 0;  // top-left column
    int y = 0;  // top-left row
};

// Phase 1 sub-steps. Each reads `cur` and writes `next`; the caller swaps.
void apply_sparks(const SiteState& cur, SiteState& next, int count, Rng& rng, const Params& p);
void apply_spoilage(const SiteState& cur, SiteState& next, const Params& p);

// Draws the position of a disaster square (x, then y).
DisasterEvent draw_disaster(std::uint64_t tick, Rng& rng, const Params& p);

}  // namespace evo
