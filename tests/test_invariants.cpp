// The project's required invariants: matter conservation and determinism.

#include <cmath>

#include "doctest.h"
#include "world.hpp"

namespace {
constexpr double kRelTol = 1e-9;
}

TEST_CASE("invariant: matter is conserved every tick") {
    evo::Params p;
    evo::World w(p, 2026);
    const double expected = 16384.0 * (4.0 + 4.0 + 2.0);
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
