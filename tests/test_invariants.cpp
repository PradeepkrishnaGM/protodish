// The project's required invariants: matter conservation and determinism.

#include <cmath>

#include "doctest.h"
#include "genome.hpp"
#include "world.hpp"

namespace {
constexpr double kRelTol = 1e-9;
}

TEST_CASE("invariant: matter is conserved every tick") {
    evo::Params p;
    evo::World w(p, 2026);
    const double expected = 16384.0 * (4.0 + 4.0 + 2.0) + 50.0 * (15.0 + 15.0 + 4.0 + 4.0);
    const double start = w.matter().total();
    CHECK(start == doctest::Approx(expected).epsilon(kRelTol));

    double worst = 0.0;
    for (int t = 0; t < 10000; ++t) {  // spans five years and the disaster at 5000
        const double before = w.matter().total();
        w.step();
        const double after = w.matter().total();
        worst = std::max(worst, std::fabs(after - before) / before);
        REQUIRE(std::fabs(after - before) <= kRelTol * before);
        REQUIRE(std::fabs(after - start) <= kRelTol * start);
    }
    MESSAGE("worst relative change in one tick: " << worst);
}

TEST_CASE("invariant: no site ever goes negative") {
    evo::Params p;
    evo::World w(p, 5);
    for (int t = 0; t < 4000; ++t) w.step();
    for (int i = 0; i < w.site_count(); ++i) {
        REQUIRE(w.food_a()[static_cast<std::size_t>(i)] >= 0.0);
        REQUIRE(w.food_b()[static_cast<std::size_t>(i)] >= 0.0);
        REQUIRE(w.minerals()[static_cast<std::size_t>(i)] >= 0.0);
    }
}

TEST_CASE("invariant: same seed gives same hash after 1000 ticks, different seeds differ") {
    evo::Params p;
    evo::World a(p, 123), b(p, 123), c(p, 124);
    CHECK(a.state_hash() == b.state_hash());
    for (int t = 0; t < 1000; ++t) {
        a.step();
        b.step();
        c.step();
    }
    CHECK(a.state_hash() == b.state_hash());
    CHECK(a.state_hash() != c.state_hash());
}

namespace {
void require_ranges(const evo::World& w) {
    const auto& c = w.cells();
    const double store_max = w.params().store_max;
    int alive = 0;
    bool ok = true;
    for (std::size_t s = 0; s < c.alive.size(); ++s) {
        if (!c.alive[s]) continue;
        ++alive;
        ok &= c.store_a[s] >= 0.0 && c.store_a[s] <= store_max;
        ok &= c.store_b[s] >= 0.0 && c.store_b[s] <= store_max;
        ok &= c.stress[s] >= 0.0 && c.stress[s] <= 1.0;
        for (std::size_t i = 0; i < evo::kGeneCount; ++i) {
            const auto& info = evo::kGeneInfo[i];
            const double v = c.genes[i][s];
            ok &= v >= info.min && (info.circular ? v < info.max : v <= info.max);
        }
        if (!ok) {
            FAIL_CHECK("out of range at site " << s << ", tick " << w.tick());
            break;
        }
    }
    REQUIRE(ok);
    REQUIRE(alive == w.cell_count());
}
}  // namespace

TEST_CASE("invariant: 50 ancestors run 10,000 ticks; matter and ranges hold every tick") {
    for (const std::uint64_t seed : {1ULL, 2ULL, 3ULL}) {
        CAPTURE(seed);
        evo::Params p;
        evo::World w(p, seed);
        const double start = w.matter().total();
        int peak = 0;
        std::uint64_t births = 0;
        for (int t = 0; t < 10000; ++t) {
            w.step();
            births += w.births().size();
            peak = std::max(peak, w.cell_count());
            REQUIRE(std::fabs(w.matter().total() - start) <= kRelTol * start);
            require_ranges(w);
        }
        MESSAGE("seed " << seed << ": " << w.cell_count() << " cells at 10k, peak " << peak
                        << ", births " << births);
    }
}
