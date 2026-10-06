#include <algorithm>

#include "doctest.h"
#include "environment.hpp"
#include "world.hpp"

using doctest::Approx;

TEST_CASE("climate: season curve") {
    evo::Params p;
    evo::Climate c(p);
    CHECK(c.season(0) == 0.0);
    CHECK(c.season(1) > 0.0);  // rising at the start
    CHECK(c.season(500) == Approx(1.0));
    CHECK(c.season(1000) == Approx(0.0));
    CHECK(c.season(1500) == Approx(-1.0));
    CHECK(c.season(2000) == c.season(0));  // one year per 2,000 ticks
    CHECK(c.season(2500) == c.season(500));
}

TEST_CASE("climate: latitude curve wraps smoothly") {
    evo::Params p;
    evo::Climate c(p);
    CHECK(c.latitude(0) == Approx(1.0));
    CHECK(c.latitude(64) == Approx(-1.0));
    CHECK(c.latitude(32) == Approx(0.0));
    CHECK(c.latitude(127) == Approx(c.latitude(1)));  // symmetric across the wrap
}

TEST_CASE("climate: temperature and light stay in range") {
    evo::Params p;
    evo::Climate c(p);
    for (int t = 0; t < p.year_length; ++t) {
        const double s = c.season(static_cast<std::uint64_t>(t));
        for (int row = 0; row < p.grid_height; ++row) {
            const double temp = c.temperature(s, row);
            const double light = c.light(s, row);
            REQUIRE(temp >= 0.0);
            REQUIRE(temp <= 30.0);
            REQUIRE(light >= 0.0);
            REQUIRE(light <= 1.0);
        }
    }
    CHECK(c.temperature(1.0, 0) == Approx(30.0));
    CHECK(c.temperature(-1.0, 64) == Approx(0.0));
    CHECK(c.temperature(0.0, 32) == Approx(15.0));
    CHECK(c.light(1.0, 0) == Approx(1.0));
    CHECK(c.light(-1.0, 64) == Approx(0.0));
}

TEST_CASE("climate: spark counts") {
    evo::Params p;
    evo::Climate c(p);
    CHECK(c.spark_count(c.season(1500)) == 125);  // midwinter
    CHECK(c.spark_count(c.season(500)) == 625);   // midsummer
    CHECK(c.spark_count(c.season(0)) == 375);
}

namespace {
evo::SiteState uniform_sites(std::size_t n, double a, double b, double m) {
    evo::SiteState s;
    s.resize(n);
    std::fill(s.food_a.begin(), s.food_a.end(), a);
    std::fill(s.food_b.begin(), s.food_b.end(), b);
    std::fill(s.minerals.begin(), s.minerals.end(), m);
    return s;
}
}  // namespace

TEST_CASE("spoilage moves exactly 1% of each food into minerals") {
    evo::Params p;
    const auto cur = uniform_sites(16, 4.0, 2.0, 1.0);
    evo::SiteState next;
    next.resize(16);
    evo::apply_spoilage(cur, next, p);
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK(next.food_a[i] == Approx(3.96));
        CHECK(next.food_b[i] == Approx(1.98));
        CHECK(next.minerals[i] == Approx(1.06));
    }
}

TEST_CASE("sparks convert at most 4 minerals per strike and conserve matter") {
    evo::Params p;
    const auto cur = uniform_sites(4, 0.0, 0.0, 10.0);
    evo::SiteState next;
    next.resize(4);
    evo::Rng rng(1);
    evo::apply_sparks(cur, next, 3, rng, p);
    double total = 0.0;
    double food = 0.0;
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(next.minerals[i] >= 0.0);
        total += next.food_a[i] + next.food_b[i] + next.minerals[i];
        food += next.food_a[i] + next.food_b[i];
    }
    CHECK(total == Approx(40.0));
    CHECK(food == Approx(12.0));  // 3 strikes × 4, every site has ≥ 8 for two strikes
}

TEST_CASE("sparks never take more minerals than a site holds") {
    evo::Params p;
    const auto cur = uniform_sites(1, 0.0, 0.0, 6.0);
    evo::SiteState next;
    next.resize(1);
    evo::Rng rng(3);
    evo::apply_sparks(cur, next, 5, rng, p);  // five strikes on the only site
    CHECK(next.minerals[0] == 0.0);
    CHECK(next.food_a[0] + next.food_b[0] == Approx(6.0));
}

TEST_CASE("disaster fires at tick 5000 and clears a wrapped 24 x 24 square") {
    evo::Params p;
    evo::World w(p, 11);
    auto occ = w.occupied_mut();
    std::fill(occ.begin(), occ.end(), std::uint8_t{1});

    while (w.tick() < 5000) w.step();
    CHECK_FALSE(w.last_disaster().has_value());
    CHECK(std::count(occ.begin(), occ.end(), 0) == 0);

    w.step();  // runs tick 5000
    REQUIRE(w.last_disaster().has_value());
    const auto ev = *w.last_disaster();
    CHECK(ev.tick == 5000);
    CHECK(std::count(occ.begin(), occ.end(), 0) == 24 * 24);
    for (int dy = 0; dy < 24; ++dy) {
        for (int dx = 0; dx < 24; ++dx) {
            const int row = (ev.y + dy) % p.grid_height;
            const int col = (ev.x + dx) % p.grid_width;
            CHECK(occ[static_cast<std::size_t>(row * p.grid_width + col)] == 0);
        }
    }

    std::fill(occ.begin(), occ.end(), std::uint8_t{1});
    while (w.tick() < 10000) w.step();
    CHECK(w.last_disaster()->tick == 5000);  // nothing in between
    w.step();
    CHECK(w.last_disaster()->tick == 10000);
}

TEST_CASE("disaster square wraps around both edges") {
    evo::Params p;
    std::vector<std::uint8_t> occ(static_cast<std::size_t>(p.grid_width * p.grid_height), 1);
    // Find a seed whose square crosses both edges, then check its corners.
    for (std::uint64_t seed = 0; seed < 10000; ++seed) {
        evo::Rng probe(seed);
        const int x = static_cast<int>(probe.below(128));
        const int y = static_cast<int>(probe.below(128));
        if (x + 24 <= 128 || y + 24 <= 128) continue;
        evo::Rng rng(seed);
        const auto ev = evo::apply_disaster(occ, 5000, rng, p);
        CHECK(ev.x == x);
        CHECK(ev.y == y);
        CHECK(occ[0] == 0);                                   // wrapped corner
        CHECK(occ[static_cast<std::size_t>(y * 128 + x)] == 0);  // origin corner
        CHECK(std::count(occ.begin(), occ.end(), 0) == 24 * 24);
        return;
    }
    FAIL("no wrapping seed found");
}

TEST_CASE("world: climate of the current tick is exposed") {
    evo::Params p;
    evo::World w(p, 1);
    CHECK(w.season() == 0.0);
    CHECK(w.row_temperature(0) == Approx(20.0));
    CHECK(w.row_light(64) == Approx(0.3));
    while (w.tick() <= 500) w.step();  // last tick run is 500
    CHECK(w.season() == Approx(1.0));
    CHECK(w.row_temperature(0) == Approx(30.0));
}
