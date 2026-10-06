// Unit tests for M3: dormancy, kin and prey, photosynthesis, leak, attack and stress.

#include <cmath>

#include "doctest.h"
#include "lab.hpp"

using doctest::Approx;
using lab::at;
using lab::clear_ground;
using lab::eater;
using lab::still;

namespace {

// An immobile, non-feeding cell with a given tag that regards no one else as kin.
evo::Genome loner(const evo::Params& p, double tag) {
    evo::Genome g = still(p);
    g[evo::kTag] = tag;
    g[evo::kTolerance] = 0.0;
    return g;
}

}  // namespace

// ---- Dormancy ----

TEST_CASE("dormancy: low supply makes a cell dormant; it is frozen and pays one tenth") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    const int s = at(32, 10);
    auto g = eater(p, 1.0);
    g[evo::kMotility] = 1.0;
    g[evo::kAppetite] = 1.0;
    g[evo::kDormancy] = 0.5;
    w.add_cell(s, g, 20, 20, 20);
    w.set_site(s, 9.0, 0.0, 0.0);  // supply 9 / 20 = 0.45 < 0.5
    w.set_cell_cooldown(s, 3);
    w.set_cell_stress(s, 0.5);

    w.step();
    const auto& c = w.cells();
    REQUIRE(c.alive[s]);  // did not move
    CHECK(c.awake[s] == 0);
    CHECK(w.food_a()[s] == 9.0);  // did not feed
    CHECK(c.age[s] == 20);        // did not age
    CHECK(c.cooldown[s] == 3);    // cooldown frozen
    CHECK(c.stress[s] == 0.5);    // stress frozen
    CHECK(w.births().empty());    // ready by stores and age, but dormant
    CHECK(w.minerals()[s] == Approx((0.2 + 0.2 + 0.001 * 20) * 0.1));
}

TEST_CASE("dormancy: supply at the gene is awake; thermal efficiency below it is dormant") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    auto g = still(p);
    g[evo::kDiet] = 1.0;
    g[evo::kDormancy] = 0.5;
    w.add_cell(at(32, 10), g, 20, 20, 0);
    w.set_site(at(32, 10), 10.0, 0.0, 0.0);  // supply exactly 0.5: not below
    g[evo::kPreferredTemp] = 0.0;              // efficiency 0 at 15 °C
    g[evo::kDormancy] = 0.1;
    w.add_cell(at(32, 40), g, 20, 20, 0);
    w.set_site(at(32, 40), 40.0, 0.0, 0.0);  // plenty of food
    w.step();
    CHECK(w.cells().awake[at(32, 10)] == 1);
    CHECK(w.cells().awake[at(32, 40)] == 0);
}

TEST_CASE("dormancy gene 0 never sleeps, even with no food and no efficiency") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    auto g = still(p);
    g[evo::kPreferredTemp] = 0.0;
    w.add_cell(at(32, 10), g, 20, 20, 0);
    w.step();
    CHECK(w.cells().awake[at(32, 10)] == 1);
}

TEST_CASE("supply: light term for a producer") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    auto g = still(p);
    g[evo::kPhotosynthesis] = 0.8;
    w.add_cell(at(32, 10), g, 20, 20, 0);
    w.set_site(at(32, 10), 0.0, 0.0, 10.0);
    w.set_site(at(32, 11), 0.0, 0.0, 10.0);
    w.step();
    CHECK(w.cells().supply[at(32, 10)] == Approx(20.0 / 20.0 * 0.5 * 0.8));
}

// ---- Kin, prey and threats ----

TEST_CASE("kin is one-way; prey needs non-kin and attack above effective defense") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    const int a = at(32, 10), b = at(32, 11), dormant = at(32, 20), awake = at(32, 30);
    auto ga = still(p);
    ga[evo::kTag] = 0.5;
    ga[evo::kTolerance] = 0.2;
    ga[evo::kAttack] = 0.5;
    auto gb = still(p);
    gb[evo::kTag] = 0.6;
    gb[evo::kTolerance] = 0.05;
    gb[evo::kAttack] = 0.5;
    w.add_cell(a, ga, 20, 20, 0);
    w.add_cell(b, gb, 20, 20, 0);

    auto gv = loner(p, 0.0);
    gv[evo::kDefense] = 0.3;
    gv[evo::kDormancy] = 1.0;  // no food in reach: dormant
    w.add_cell(dormant, gv, 20, 20, 0);
    gv[evo::kDormancy] = 0.0;
    w.add_cell(awake, gv, 20, 20, 0);

    w.step();
    CHECK(w.is_kin(a, b));
    CHECK_FALSE(w.is_kin(b, a));
    CHECK_FALSE(w.treats_as_prey(a, b));  // kin
    CHECK(w.treats_as_prey(b, a));        // b does not regard a as kin
    // Defense 0.3 doubles to 0.6 while dormant: 0.5 no longer beats it.
    CHECK(w.cells().awake[dormant] == 0);
    CHECK_FALSE(w.treats_as_prey(a, dormant));
    CHECK(w.treats_as_prey(a, awake));
}

TEST_CASE("move: boldness pulls toward prey, caution pushes from awake threats only") {
    const auto p = lab::params();
    auto mover = loner(p, 0.0);
    mover[evo::kMotility] = 1.0;
    mover[evo::kAppetite] = 0.0;

    auto next_to = [](const evo::World& w) {
        bool hit = false;
        for (const int row : {31, 32, 33}) hit |= w.cells().alive[at(row, 11)] != 0;
        return hit;
    };

    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        CAPTURE(seed);
        {  // prey at (32, 12): a bold attacker steps next to it
            evo::World w(p, seed);
            clear_ground(w);
            auto g = mover;
            g[evo::kAttack] = 0.5;
            g[evo::kBoldness] = 1.0;
            w.add_cell(at(32, 10), g, 20, 20, 0);
            w.add_cell(at(32, 12), loner(p, 0.5), 20, 20, 0);
            w.step();
            CHECK(next_to(w));
        }
        {  // threat at (32, 12): a cautious cell keeps away
            evo::World w(p, seed);
            clear_ground(w);
            auto g = mover;
            g[evo::kCaution] = 1.0;
            w.add_cell(at(32, 10), g, 20, 20, 0);
            auto t = loner(p, 0.5);
            t[evo::kAttack] = 0.5;
            w.add_cell(at(32, 12), t, 20, 20, 0);
            w.step();
            CHECK_FALSE(next_to(w));
        }
    }

    int beside_dormant = 0;  // a dormant threat is ignored: the walk is random
    for (std::uint64_t seed = 0; seed < 40; ++seed) {
        evo::World w(p, seed);
        clear_ground(w);
        auto g = mover;
        g[evo::kCaution] = 1.0;
        w.add_cell(at(32, 10), g, 20, 20, 0);
        auto t = loner(p, 0.5);
        t[evo::kAttack] = 0.5;
        t[evo::kDormancy] = 1.0;
        w.add_cell(at(32, 12), t, 20, 20, 0);
        w.step();
        REQUIRE(w.cells().awake[at(32, 12)] == 0);
        beside_dormant += next_to(w);
    }
    CHECK(beside_dormant > 0);
}

// ---- Photosynthesis and leak ----

TEST_CASE("photosynthesis: makes food from minerals, own site first, split by diet") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    const int s = at(32, 10);
    auto g = still(p);
    g[evo::kPhotosynthesis] = 1.0;
    g[evo::kDiet] = 0.75;
    w.add_cell(s, g, 20, 30, 0);
    w.set_site(s, 0.0, 0.0, 0.4);
    for (int d = 0; d < 8; ++d) w.set_site(w.neighbor(s, d), 0.0, 0.0, 1.0);

    w.step();
    // Capacity 2 × 1 × light 0.5 × eff 1 = 1: 0.4 from own site, 0.075 from each neighbor.
    const auto& c = w.cells();
    CHECK(c.feed_capacity[s] == Approx(1.0));
    CHECK(c.gross_intake[s] == Approx(1.0));
    CHECK(c.store_a[s] == Approx(20.75));
    CHECK(c.store_b[s] == Approx(30.25 - 0.5));  // upkeep 0.2 + 0.3 × photosynthesis
    CHECK(w.minerals()[s] == Approx(0.5));       // emptied, then the upkeep waste
    for (int d = 0; d < 8; ++d) CHECK(w.minerals()[w.neighbor(s, d)] == Approx(0.925));
}

TEST_CASE("leak: 20% of gross intake goes evenly to neighbors, dormant ones included") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    const int f = at(32, 10), n1 = at(32, 11), n2 = at(31, 10);
    w.add_cell(f, eater(p, 1.0), 5, 30, 0);
    w.set_site(f, 10.0, 0.0, 0.0);
    w.add_cell(n1, still(p), 5, 30, 0);
    auto sleeper = still(p);
    sleeper[evo::kDormancy] = 1.0;
    w.add_cell(n2, sleeper, 5, 30, 0);
    const double total = w.matter().total();

    w.step();
    const auto& c = w.cells();
    REQUIRE(c.awake[n2] == 0);
    CHECK(c.store_a[f] == Approx(5.0 + 1.6));
    CHECK(c.store_a[n1] == Approx(5.2));
    CHECK(c.store_a[n2] == Approx(5.2));
    CHECK(c.store_b[n2] == Approx(30.0 - 0.02));  // dormant upkeep: 0.2 × 0.1
    CHECK(w.matter().total() == Approx(total).epsilon(1e-12));
}

// ---- Attack ----

TEST_CASE("attack: drain amount, proportional take, half kept, half as scraps") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    const int x = at(32, 10), v = at(32, 11);
    auto gx = loner(p, 0.0);
    gx[evo::kAttack] = 0.8;
    w.add_cell(x, gx, 10, 10, 0);
    auto gv = loner(p, 0.5);
    gv[evo::kDefense] = 0.3;
    w.add_cell(v, gv, 10, 30, 0);
    const double total = w.matter().total();

    w.step();
    // Drain 3 × (0.8 − 0.3) × 1 = 1.5, taken 1 : 3 from A and B.
    const auto& c = w.cells();
    CHECK(c.store_a[v] == Approx(10.0 - 0.375));
    CHECK(c.store_b[v] == Approx(30.0 - 1.125 - (0.2 + 0.3 * 0.3)));
    CHECK(c.store_a[x] == Approx(10.0 + 0.1875));
    CHECK(c.store_b[x] == Approx(10.0 + 0.5625 - (0.2 + 0.5 * 0.8)));
    CHECK(w.food_a()[v] == Approx(0.1875));
    CHECK(w.food_b()[v] == Approx(0.5625));
    CHECK(c.drained[v] == 1);
    CHECK(c.drained[x] == 0);
    CHECK(c.stress[v] == Approx(0.2));
    CHECK(c.stress[x] == 0.0);
    CHECK(w.matter().total() == Approx(total).epsilon(1e-12));
}

TEST_CASE("attack: satiation caps the drain at twice the room; full stores do not attack") {
    const auto p = lab::params();
    auto gx = loner(p, 0.0);
    gx[evo::kAttack] = 0.8;
    auto gv = loner(p, 0.5);
    gv[evo::kDefense] = 0.3;

    for (const auto& [store, expected] :
         {std::pair{49.5, 1.5}, std::pair{49.8, 0.8}, std::pair{50.0, 0.0}}) {
        CAPTURE(store);
        evo::World w(p, 1);
        clear_ground(w);
        w.add_cell(at(32, 10), gx, store, store, 0);
        w.add_cell(at(32, 11), gv, 20, 20, 0);
        w.step();
        const double lost = 40.0 - (w.cells().store_a[at(32, 11)] + w.cells().store_b[at(32, 11)]) -
                            (0.2 + 0.3 * 0.3);
        CHECK(lost == Approx(expected));
        CHECK(w.cells().drained[at(32, 11)] == (expected > 0.0 ? 1 : 0));
    }
}

TEST_CASE("attack: several attackers are scaled to what the victim holds; it dies drained") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    const int v = at(32, 11);
    auto gx = loner(p, 0.0);
    gx[evo::kAttack] = 0.8;
    w.add_cell(at(32, 10), gx, 10, 10, 0);
    w.add_cell(at(32, 12), gx, 10, 10, 0);
    auto gv = loner(p, 0.5);
    gv[evo::kDefense] = 0.3;
    w.add_cell(v, gv, 0.25, 0.25, 0);
    const double total = w.matter().total();

    w.step();
    CHECK_FALSE(w.cells().alive[v]);
    REQUIRE(w.deaths().size() == 1);
    CHECK(w.deaths()[0].cause == evo::DeathCause::Drained);
    // Each attacker took 0.25 (half A, half B) and kept half; scraps 0.125 of each + body 4.
    CHECK(w.food_a()[v] == Approx(0.125 + 4.0));
    CHECK(w.food_b()[v] == Approx(0.125 + 4.0));
    CHECK(w.matter().total() == Approx(total).epsilon(1e-12));
}

// ---- Stress ----

TEST_CASE("stress: poor intake raises it, it fades by 0.9, and it is capped at 1") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    const int s = at(32, 10);
    auto g = p.ancestor;  // capacity 0.4 + 0.4, nothing to eat
    g[evo::kMotility] = 0.0;
    w.add_cell(s, g, 20, 20, 0);
    w.step();
    CHECK(w.cells().stress[s] == Approx(0.05));
    w.step();
    CHECK(w.cells().stress[s] == Approx(0.05 * 0.9 + 0.05));

    // A drained, underfed cell near the top is capped at 1.
    evo::World w2(p, 1);
    clear_ground(w2);
    auto gx = loner(p, 0.0);
    gx[evo::kAttack] = 0.8;
    w2.add_cell(at(32, 10), gx, 10, 10, 0);
    auto gv = loner(p, 0.5);
    gv[evo::kHarvest] = 0.5;
    w2.add_cell(at(32, 11), gv, 30, 30, 0);
    w2.set_cell_stress(at(32, 11), 0.95);
    w2.step();
    CHECK(w2.cells().stress[at(32, 11)] == 1.0);
}

TEST_CASE("stress-driven mutation: stressed, mutable mothers mutate more") {
    const auto p = lab::params();
    evo::World w(p, 5);
    clear_ground(w);
    auto g = still(p);
    g[evo::kMutability] = 1.0;
    std::vector<int> mothers;
    for (int row = 2; row < 126; row += 4) {
        for (int col = 2; col < 126; col += 4) {
            w.add_cell(at(row, col), g, 50, 50, 10);
            w.set_cell_stress(at(row, col), 1.0);
        }
    }
    w.step();
    // Stress after upkeep is 0.9 (fade, no other rise), so the chance is 5% × (1 + 4 × 0.9).
    int changed = 0, total = 0;
    for (const auto& b : w.births()) {
        for (std::size_t i = 0; i < evo::kGeneCount; ++i) changed += b.genome[i] != g[i];
        total += static_cast<int>(evo::kGeneCount);
    }
    REQUIRE(total > 10000);
    CHECK(static_cast<double>(changed) / total == Approx(0.05 * (1 + 4 * 0.9)).epsilon(0.08));
}
