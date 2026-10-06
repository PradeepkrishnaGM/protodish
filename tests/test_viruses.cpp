// Unit tests for M5: outbreak, spread, drift, recovery and burden.

#include <cmath>

#include "doctest.h"
#include "lab.hpp"

using doctest::Approx;
using lab::at;
using lab::clear_ground;
using lab::still;

namespace {

// The lab world with no random outbreaks and no drift; each test turns on what it needs.
evo::Params virus_params() {
    evo::Params p = lab::params();
    p.outbreak_chance = 0.0;
    p.drift_chance = 0.0;
    return p;
}

evo::Genome tagged(const evo::Params& p, double tag) {
    evo::Genome g = still(p);
    g[evo::kTag] = tag;
    return g;
}

// Dormant on cleared ground: supply 0 is below a dormancy gene of 0.5.
evo::Genome sleeper(const evo::Params& p, double tag) {
    evo::Genome g = tagged(p, tag);
    g[evo::kDormancy] = 0.5;
    return g;
}

}  // namespace

TEST_CASE("params: virus chances outside [0, 1] are rejected") {
    evo::Params p;
    p.spread_chance = 1.5;
    CHECK_FALSE(evo::validate_params(p).empty());
    p = evo::Params{};
    p.virus_match = 0.6;
    CHECK_FALSE(evo::validate_params(p).empty());
}

TEST_CASE("outbreak: a healthy awake cell catches a virus with its own tag; a dormant one does not") {
    auto p = virus_params();
    p.outbreak_chance = 1.0;
    evo::World w(p, 1);
    clear_ground(w);
    w.add_cell(at(32, 10), tagged(p, 0.3), 20, 20, 0);
    w.add_cell(at(32, 20), sleeper(p, 0.3), 20, 20, 0);
    w.step();
    const auto& c = w.cells();
    CHECK(c.infected[at(32, 10)] == 1);
    CHECK(c.virus_tag[at(32, 10)] == 0.3);
    CHECK(c.infected[at(32, 20)] == 0);
}

TEST_CASE("spread: only to awake, matching neighbors, with chance × (1 − resistance)") {
    auto p = virus_params();
    p.spread_chance = 1.0;
    evo::World w(p, 1);
    clear_ground(w);
    const int src = at(32, 10);
    w.add_cell(src, tagged(p, 0.5), 20, 20, 0);
    w.set_cell_virus(src, 0.5);
    w.add_cell(at(32, 11), tagged(p, 0.54), 20, 20, 0);  // matches (0.04)
    w.add_cell(at(32, 9), tagged(p, 0.56), 20, 20, 0);   // too far (0.06)
    auto resistant = tagged(p, 0.5);
    resistant[evo::kResistance] = 1.0;
    w.add_cell(at(31, 10), resistant, 20, 20, 0);
    w.add_cell(at(33, 10), sleeper(p, 0.5), 20, 20, 0);  // dormant: does not catch
    w.step();
    const auto& c = w.cells();
    CHECK(c.infected[src] == 1);
    CHECK(c.infected[at(32, 11)] == 1);
    CHECK(c.virus_tag[at(32, 11)] == 0.5);  // the virus's tag, not the host's
    CHECK(c.infected[at(32, 9)] == 0);
    CHECK(c.infected[at(31, 10)] == 0);
    CHECK(c.infected[at(33, 10)] == 0);
}

TEST_CASE("dormant infected cells neither pass nor clear their virus, but pay its burden") {
    auto p = virus_params();
    p.spread_chance = 1.0;
    p.recovery_chance = 1.0;
    evo::World w(p, 1);
    clear_ground(w);
    auto g = sleeper(p, 0.5);
    g[evo::kResistance] = 1.0;
    const int src = at(32, 10);
    w.add_cell(src, g, 20, 20, 0);
    w.set_cell_virus(src, 0.5);
    w.add_cell(at(32, 11), tagged(p, 0.5), 20, 20, 0);
    w.step();
    const auto& c = w.cells();
    CHECK(c.awake[src] == 0);
    CHECK(c.infected[src] == 1);
    CHECK(c.infected[at(32, 11)] == 0);
    CHECK(c.stress[src] == 0.0);  // frozen (DECISIONS M3-7)
    // (alive 0.2 + resistance 0.2 + infection 0.3) × one tenth.
    CHECK(w.minerals()[src] == Approx((0.2 + 0.2 + 0.3) * 0.1));
}

TEST_CASE("spread: a newly infected cell does not pass the virus on in the same tick") {
    auto p = virus_params();
    p.spread_chance = 1.0;
    evo::World w(p, 1);
    clear_ground(w);
    for (int col = 10; col <= 12; ++col) w.add_cell(at(32, col), tagged(p, 0.5), 20, 20, 0);
    w.set_cell_virus(at(32, 10), 0.5);
    w.step();
    CHECK(w.cells().infected[at(32, 11)] == 1);
    CHECK(w.cells().infected[at(32, 12)] == 0);
    w.step();
    CHECK(w.cells().infected[at(32, 12)] == 1);
}

TEST_CASE("spread: a target reached by two viruses catches one of them") {
    auto p = virus_params();
    p.spread_chance = 1.0;
    bool saw_low = false, saw_high = false;
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        evo::World w(p, seed);
        clear_ground(w);
        w.add_cell(at(32, 9), tagged(p, 0.5), 20, 20, 0);
        w.add_cell(at(32, 10), tagged(p, 0.5), 20, 20, 0);
        w.add_cell(at(32, 11), tagged(p, 0.5), 20, 20, 0);
        w.set_cell_virus(at(32, 9), 0.48);
        w.set_cell_virus(at(32, 11), 0.52);
        w.step();
        REQUIRE(w.cells().infected[at(32, 10)] == 1);
        const double v = w.cells().virus_tag[at(32, 10)];
        CHECK((v == 0.48 || v == 0.52));
        saw_low |= v == 0.48;
        saw_high |= v == 0.52;
    }
    CHECK(saw_low);
    CHECK(saw_high);
}

TEST_CASE("drift: a passing virus shifts by at most ±0.02 and wraps; the source keeps its tag") {
    auto p = virus_params();
    p.spread_chance = 1.0;
    p.drift_chance = 1.0;
    bool wrapped = false, shifted = false;
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        evo::World w(p, seed);
        clear_ground(w);
        w.add_cell(at(32, 10), tagged(p, 0.0), 20, 20, 0);
        w.add_cell(at(32, 11), tagged(p, 0.0), 20, 20, 0);
        w.set_cell_virus(at(32, 10), 0.01);
        w.step();
        const auto& c = w.cells();
        CHECK(c.virus_tag[at(32, 10)] == 0.01);
        REQUIRE(c.infected[at(32, 11)] == 1);
        const double v = c.virus_tag[at(32, 11)];
        CHECK(v >= 0.0);
        CHECK(v < 1.0);
        CHECK(evo::tag_distance(v, 0.01) <= 0.02 + 1e-12);
        wrapped |= v > 0.9;
        shifted |= v != 0.01;
    }
    CHECK(wrapped);
    CHECK(shifted);
}

TEST_CASE("recovery: chance × resistance; resistance 0 never clears") {
    auto p = virus_params();
    p.recovery_chance = 1.0;
    evo::World w(p, 1);
    clear_ground(w);
    auto g = tagged(p, 0.5);
    g[evo::kResistance] = 1.0;
    w.add_cell(at(32, 10), g, 40, 40, 0);
    w.set_cell_virus(at(32, 10), 0.5);
    w.add_cell(at(32, 40), tagged(p, 0.5), 40, 40, 0);
    w.set_cell_virus(at(32, 40), 0.5);
    w.step();
    CHECK(w.cells().infected[at(32, 10)] == 0);
    CHECK(w.cells().virus_tag[at(32, 10)] == 0.0);
    for (int t = 0; t < 20; ++t) w.step();
    CHECK(w.cells().infected[at(32, 40)] == 1);
}

TEST_CASE("burden: an infected cell pays 0.3 more upkeep and gains 0.1 stress") {
    const auto p = virus_params();
    evo::World w(p, 1);
    clear_ground(w);
    w.add_cell(at(32, 10), tagged(p, 0.5), 20, 20, 0);
    w.set_cell_virus(at(32, 10), 0.5);
    w.add_cell(at(32, 40), tagged(p, 0.5), 20, 20, 0);
    w.step();
    const auto& c = w.cells();
    CHECK(w.minerals()[at(32, 10)] == Approx(0.2 + 0.3));
    CHECK(w.minerals()[at(32, 40)] == Approx(0.2));
    CHECK(c.stress[at(32, 10)] == Approx(0.1));
    CHECK(c.stress[at(32, 40)] == 0.0);
}

TEST_CASE("burden is judged at upkeep: a cell recovering this tick pays none, a new case pays at once") {
    auto p = virus_params();
    p.spread_chance = 1.0;
    p.recovery_chance = 1.0;
    evo::World w(p, 1);
    clear_ground(w);
    auto g = tagged(p, 0.5);
    g[evo::kResistance] = 1.0;  // always recovers
    const int src = at(32, 10);
    const int dst = at(32, 11);
    w.add_cell(src, g, 20, 20, 0);
    w.add_cell(dst, tagged(p, 0.5), 20, 20, 0);
    w.set_cell_virus(src, 0.5);
    w.step();
    const auto& c = w.cells();
    CHECK(c.infected[src] == 0);
    CHECK(c.infected[dst] == 1);  // the recovering source still spread this tick
    CHECK(w.minerals()[src] == Approx(0.2 + 0.2));  // alive + resistance, no infection
    CHECK(c.stress[src] == 0.0);
    CHECK(w.minerals()[dst] == Approx(0.2 + 0.3));
    CHECK(c.stress[dst] == Approx(0.1));
}

TEST_CASE("lifecycle: daughters are born healthy, a virus moves with its cell and dies with it") {
    auto p = virus_params();
    p.spread_chance = 1.0;
    {
        evo::World w(p, 1);
        clear_ground(w);
        const int m = at(32, 10);
        w.add_cell(m, tagged(p, 0.5), 30, 30, 10);
        w.set_cell_virus(m, 0.5);
        w.step();
        REQUIRE(w.births().size() == 1);
        CHECK(w.cells().infected[w.births()[0].site] == 0);
        CHECK(w.cells().infected[m] == 1);
    }
    {
        evo::World w(p, 1);
        clear_ground(w);
        auto g = tagged(p, 0.5);
        g[evo::kMotility] = 1.0;
        const int s = at(32, 10);
        w.add_cell(s, g, 20, 20, 0);
        w.set_cell_virus(s, 0.47);
        w.step();
        const auto& c = w.cells();
        REQUIRE(c.alive[s] == 0);
        CHECK(c.infected[s] == 0);
        int found = 0;
        for (int d = 0; d < 8; ++d) {
            const auto n = static_cast<std::size_t>(w.neighbor(s, d));
            if (c.alive[n]) {
                ++found;
                CHECK(c.infected[n] == 1);
                CHECK(c.virus_tag[n] == 0.47);
            }
        }
        CHECK(found == 1);
    }
    {
        evo::World w(p, 1);
        clear_ground(w);
        const int s = at(32, 10);
        w.add_cell(s, tagged(p, 0.5), 0.3, 0.1, 0);  // 0.4 < 0.5 upkeep: dies
        w.set_cell_virus(s, 0.5);
        w.step();
        REQUIRE(w.deaths().size() == 1);
        CHECK(w.cells().infected[s] == 0);
        CHECK(w.cells().virus_tag[s] == 0.0);
        CHECK(w.food_a()[s] == Approx(0.3 + 4.0));  // no matter in the virus
    }
}

TEST_CASE("a virus runs through a body of clones one ring per tick") {
    auto p = virus_params();
    p.spread_chance = 1.0;
    evo::World w(p, 1);
    clear_ground(w);
    for (int r = 32; r <= 34; ++r) {
        for (int col = 10; col <= 12; ++col) w.add_cell(at(r, col), tagged(p, 0.5), 20, 20, 0);
    }
    for (int r = 32; r <= 34; ++r) {
        for (int col = 10; col <= 12; ++col) {
            if (col < 12) w.add_bond(at(r, col), at(r, col + 1));
            if (r < 34) w.add_bond(at(r, col), at(r + 1, col));
        }
    }
    w.set_cell_virus(at(32, 10), 0.5);
    auto count = [&] {
        int n = 0;
        for (int r = 32; r <= 34; ++r) {
            for (int col = 10; col <= 12; ++col) n += w.cells().infected[at(r, col)];
        }
        return n;
    };
    w.step();
    CHECK(count() == 4);
    w.step();
    CHECK(count() == 9);
}

TEST_CASE("determinism with viruses: same seed gives the same hash, and viruses are hashed") {
    evo::Params p;
    p.outbreak_chance = 1e-3;
    evo::World a(p, 77), b(p, 77);
    int infected_ticks = 0;
    for (int t = 0; t < 1000; ++t) {
        a.step();
        b.step();
        int n = 0;
        for (const auto v : a.cells().infected) n += v;
        infected_ticks += n > 0;
    }
    CHECK(infected_ticks > 100);
    CHECK(a.state_hash() == b.state_hash());

    // Flipping one cell's infection changes the hash.
    for (int s = 0; s < b.site_count(); ++s) {
        if (b.cells().alive[static_cast<std::size_t>(s)]) {
            b.set_cell_virus(s, b.cells().infected[static_cast<std::size_t>(s)] ? -1.0 : 0.5);
            break;
        }
    }
    CHECK(a.state_hash() != b.state_hash());
}
