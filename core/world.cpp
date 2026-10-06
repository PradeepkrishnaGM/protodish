#include "world.hpp"

#include <cmath>
#include <utility>

namespace evo {

namespace {

// Neumaier compensated sum.
double accurate_sum(std::span<const double> values) {
    double sum = 0.0;
    double comp = 0.0;
    for (const double v : values) {
        const double t = sum + v;
        if (std::fabs(sum) >= std::fabs(v)) {
            comp += (sum - t) + v;
        } else {
            comp += (v - t) + sum;
        }
        sum = t;
    }
    return sum + comp;
}

class Fnv1a {
public:
    void bytes(const void* data, std::size_t n) {
        const auto* p = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < n; ++i) {
            h_ ^= p[i];
            h_ *= 0x100000001b3ULL;
        }
    }
    template <typename T>
    void value(const T& v) { bytes(&v, sizeof(T)); }
    template <typename T>
    void array(const std::vector<T>& v) { bytes(v.data(), v.size() * sizeof(T)); }
    std::uint64_t digest() const { return h_; }

private:
    std::uint64_t h_ = 0xcbf29ce484222325ULL;
};

}  // namespace

World::World(const Params& params, std::uint64_t seed)
    : params_(params),
      climate_(params_),
      rng_(seed),
      seed_(seed),
      n_sites_(params.grid_width * params.grid_height) {
    const auto n = static_cast<std::size_t>(n_sites_);
    cur_.resize(n);
    next_.resize(n);
    occupied_.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        cur_.food_a[i] = params_.initial_food_a;
        cur_.food_b[i] = params_.initial_food_b;
        cur_.minerals[i] = params_.initial_minerals;
    }
    row_temperature_.resize(static_cast<std::size_t>(params_.grid_height));
    row_light_.resize(static_cast<std::size_t>(params_.grid_height));
    prepare_climate();
}

void World::prepare_climate() {
    season_ = climate_.season(tick_);
    for (int row = 0; row < params_.grid_height; ++row) {
        row_temperature_[static_cast<std::size_t>(row)] = climate_.temperature(season_, row);
        row_light_[static_cast<std::size_t>(row)] = climate_.light(season_, row);
    }
}

void World::step() {
    phase_environment();
    phase_sense();
    phase_move();
    phase_feed();
    phase_attack();
    phase_share();
    phase_infect();
    phase_upkeep();
    phase_divide();
    phase_record();
    ++tick_;
}

void World::phase_environment() {
    prepare_climate();

    apply_sparks(cur_, next_, climate_.spark_count(season_), rng_, params_);
    std::swap(cur_, next_);

    apply_spoilage(cur_, next_, params_);
    std::swap(cur_, next_);

    if (tick_ > 0 && tick_ % static_cast<std::uint64_t>(params_.disaster_interval) == 0) {
        last_disaster_ = apply_disaster(occupied_, tick_, rng_, params_);
    }
}

MatterTotals World::matter() const {
    MatterTotals m;
    m.food_a = accurate_sum(cur_.food_a);
    m.food_b = accurate_sum(cur_.food_b);
    m.minerals = accurate_sum(cur_.minerals);
    m.cells = 0.0;  // no cells until M2
    return m;
}

std::uint64_t World::state_hash() const {
    Fnv1a h;
    h.value(tick_);
    h.value(rng_.state());
    h.value(rng_.increment());
    h.array(cur_.food_a);
    h.array(cur_.food_b);
    h.array(cur_.minerals);
    h.array(occupied_);
    return h.digest();
}

}  // namespace evo
