#include "doctest.h"
#include "rng.hpp"

TEST_CASE("rng: same seed gives same sequence, different seeds differ") {
    evo::Rng a(42), b(42), c(43);
    bool any_diff = false;
    for (int i = 0; i < 1000; ++i) {
        const auto x = a.next_u32();
        CHECK(x == b.next_u32());
        if (x != c.next_u32()) any_diff = true;
    }
    CHECK(any_diff);
}

TEST_CASE("rng: below stays in range and covers it") {
    evo::Rng r(7);
    int counts[6] = {};
    for (int i = 0; i < 60000; ++i) {
        const auto v = r.below(6);
        REQUIRE(v < 6u);
        ++counts[v];
    }
    for (int c : counts) CHECK(c > 9000);
}

TEST_CASE("rng: uniform is in [0, 1)") {
    evo::Rng r(9);
    double sum = 0.0;
    for (int i = 0; i < 100000; ++i) {
        const double u = r.uniform();
        REQUIRE(u >= 0.0);
        REQUIRE(u < 1.0);
        sum += u;
    }
    CHECK(sum / 100000 == doctest::Approx(0.5).epsilon(0.01));
}
