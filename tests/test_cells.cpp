// Unit tests for the M2 cell phases. Scenes are built with the helpers in lab.hpp.

#include <algorithm>
#include <set>

#include "doctest.h"
#include "lab.hpp"

using doctest::Approx;
using lab::at;
using lab::clear_ground;
using lab::eater;
using lab::still;

namespace {
evo::Params lab_params() { return lab::params(); }
}  // namespace

TEST_CASE("sense: thermal efficiency") {
    const auto p = lab_params();
    evo::World w(p, 1);
    clear_ground(w);
    auto g = still(p);
    w.add_cell(at(32, 10), g, 20, 20, 0);
    g[evo::kPreferredTemp] = 0.0;
    w.add_cell(at(32, 20), g, 20, 20, 0);
    g[evo::kPreferredTemp] = 7.5;
    w.add_cell(at(32, 30), g, 20, 20, 0);
    w.step();
    const auto& c = w.cells();
    CHECK(c.thermal_eff[at(32, 10)] == Approx(1.0));
    CHECK(c.thermal_eff[at(32, 20)] == Approx(0.0));
    CHECK(c.thermal_eff[at(32, 30)] == Approx(0.75));
}

TEST_CASE("sense: supply is the better of food and light, each /20 and capped; occupied sites are out of reach") {
    const auto p = lab_params();
    evo::World w(p, 1);
    clear_ground(w);

    auto g = still(p);
    g[evo::kDiet] = 1.0;
    g[evo::kPhotosynthesis] = 0.5;
    w.add_cell(at(32, 50), g, 20, 20, 0);
    w.set_site(at(32, 50), 10.0, 0.0, 20.0);
    w.set_site(at(32, 51), 10.0, 0.0, 0.0);

    g = still(p);
    g[evo::kDiet] = 0.0;
    w.add_cell(at(32, 80), g, 20, 20, 0);
    w.set_site(at(32, 80), 0.0, 4.0, 0.0);

    g = still(p);
    g[evo::kDiet] = 1.0;
    w.add_cell(at(32, 90), g, 20, 20, 0);
    w.add_cell(at(32, 91), g, 20, 20, 0);
    w.set_site(at(32, 91), 40.0, 0.0, 0.0);

    w.step();
    const auto& c = w.cells();
    CHECK(c.supply[at(32, 50)] == Approx(1.0));  // max(food 1, light 1 × 0.5 × 0.5)
    CHECK(c.supply[at(32, 80)] == Approx(0.2));
    CHECK(c.supply[at(32, 90)] == Approx(0.0));  // the food next door is under a cell
    CHECK(c.supply[at(32, 91)] == Approx(1.0));  // capped at 1
}

TEST_CASE("feed: own site first, then equally from the empty sites in reach") {
    const auto p = lab_params();
    evo::World w(p, 1);
    clear_ground(w);
    const int s = at(32, 10);
    w.add_cell(s, eater(p, 1.0), 20, 10, 0);
    w.set_site(s, 0.5, 0.0, 0.0);
    for (int d = 0; d < 8; ++d) w.set_site(w.neighbor(s, d), 1.0, 0.0, 0.0);

    w.step();
    CHECK(w.food_a()[s] == Approx(0.0));
    for (int d = 0; d < 8; ++d) {
        CHECK(w.food_a()[w.neighbor(s, d)] == Approx(1.0 - 1.5 / 8));
    }
    // Intake 2 A, then upkeep 0.2 + 0.2 × harvest from the larger store.
    CHECK(w.cells().store_a[s] == Approx(20.0 + 2.0 - 0.4));
    CHECK(w.cells().store_b[s] == Approx(10.0));
    CHECK(w.minerals()[s] == Approx(0.4));
}

TEST_CASE("feed: an over-asked site is split in proportion to the requests") {
    const auto p = lab_params();
    evo::World w(p, 1);
    clear_ground(w);
    w.add_cell(at(32, 10), eater(p, 1.0), 20, 30, 0);
    w.add_cell(at(32, 12), eater(p, 1.0), 20, 30, 0);
    w.set_site(at(32, 11), 0.3, 0.0, 0.0);
    const double total = w.matter().total();

    w.step();
    // Each asks 2 / 8 = 0.25 of the shared site; it holds 0.3, so each gets 0.15.
    CHECK(w.food_a()[at(32, 11)] == Approx(0.0));
    CHECK(w.cells().store_a[at(32, 10)] == Approx(20.15));
    CHECK(w.cells().store_a[at(32, 12)] == Approx(20.15));
    CHECK(w.cells().store_b[at(32, 10)] == Approx(29.6));
    CHECK(w.matter().total() == Approx(total).epsilon(1e-12));
}

TEST_CASE("feed: intake above 50 falls onto the own site as food") {
    const auto p = lab_params();
    evo::World w(p, 1);
    clear_ground(w);
    const int s = at(32, 10);
    w.add_cell(s, eater(p, 1.0), 49.5, 5, 0);
    w.set_site(s, 10.0, 0.0, 0.0);

    w.step();
    CHECK(w.food_a()[s] == Approx(10.0 - 2.0 + 1.5));
    CHECK(w.cells().store_a[s] == Approx(50.0 - 0.4));
}

TEST_CASE("upkeep: every cost item, the temperature multiplier, paid from the larger store") {
    const auto p = lab_params();
    evo::World w(p, 1);
    clear_ground(w);
    const int s = at(0, 10);  // row 0: 20 °C at season 0
    auto g = still(p);
    g[evo::kHarvest] = 0.5;
    g[evo::kAttack] = 0.2;
    g[evo::kDefense] = 0.4;
    g[evo::kPhotosynthesis] = 0.3;
    g[evo::kResistance] = 0.6;
    w.add_cell(s, g, 5, 10, 100);
    for (const int d : {0, 2, 4, 6}) w.add_cell(w.neighbor(s, d), still(p), 20, 20, 0);  // 4 neighbors

    w.step();
    const double items = 0.2 + 0.2 * 0.5 + 0.5 * 0.2 + 0.3 * 0.4 + 0.03 * 1 + 0.001 * 100 +
                         0.3 * 0.3 + 0.2 * 0.6;
    const double cost = items * (0.5 + 20.0 / 30.0);
    CHECK(w.minerals()[s] == Approx(cost));
    CHECK(w.cells().store_b[s] == Approx(10.0 - cost));
    CHECK(w.cells().store_a[s] == Approx(5.0));
    CHECK(w.cells().age[s] == 101);
}

TEST_CASE("upkeep: the larger store pays first, the other covers the rest") {
    const auto p = lab_params();
    evo::World w(p, 1);
    clear_ground(w);
    w.add_cell(at(32, 10), still(p), 0.05, 0.3, 0);  // cost 0.2
    w.add_cell(at(32, 20), still(p), 0.15, 0.1, 0);
    w.step();
    const auto& c = w.cells();
    CHECK(c.store_a[at(32, 10)] == Approx(0.05));
    CHECK(c.store_b[at(32, 10)] == Approx(0.1));
    CHECK(c.store_a[at(32, 20)] == Approx(0.0));
    CHECK(c.store_b[at(32, 20)] == Approx(0.05));
}

TEST_CASE("upkeep: a cell that cannot pay in full dies, pays nothing, and becomes food") {
    const auto p = lab_params();
    evo::World w(p, 1);
    clear_ground(w);
    const int s = at(32, 10);
    w.add_cell(s, still(p), 0.1, 0.05, 0);
    const double total = w.matter().total();

    w.step();
    CHECK(w.cell_count() == 0);
    CHECK(w.food_a()[s] == Approx(0.1 + 4.0));
    CHECK(w.food_b()[s] == Approx(0.05 + 4.0));
    CHECK(w.minerals()[s] == 0.0);
    REQUIRE(w.deaths().size() == 1);
    CHECK(w.deaths()[0].cause == evo::DeathCause::Starved);
    CHECK(w.matter().total() == Approx(total).epsilon(1e-12));
    REQUIRE(w.extinct_at().has_value());
    CHECK(*w.extinct_at() == 0);
}

TEST_CASE("move: steps toward food and pays the move cost") {
    const auto p = lab_params();
    evo::World w(p, 1);
    clear_ground(w);
    auto g = still(p);
    g[evo::kMotility] = 1.0;
    g[evo::kAppetite] = 1.0;
    g[evo::kDiet] = 1.0;
    w.add_cell(at(32, 10), g, 20, 20, 0);
    w.set_site(at(32, 11), 20.0, 0.0, 0.0);  // same row, so the upkeep multiplier stays 1

    w.step();
    CHECK_FALSE(w.cells().alive[at(32, 10)]);
    REQUIRE(w.cells().alive[at(32, 11)]);
    CHECK(w.cells().moved[at(32, 11)] == 1);
    CHECK(w.minerals()[at(32, 11)] == Approx(0.2 + 0.2));  // alive + move
}

TEST_CASE("move: contested site goes to one cell; the loser stays and pays no move cost") {
    const auto p = lab_params();
    int left_wins = 0, right_wins = 0;
    for (std::uint64_t seed = 0; seed < 40; ++seed) {
        evo::World w(p, seed);
        clear_ground(w);
        auto g = still(p);
        g[evo::kMotility] = 1.0;
        g[evo::kAppetite] = 1.0;
        g[evo::kDiet] = 1.0;
        w.add_cell(at(32, 10), g, 20, 20, 0);
        w.add_cell(at(32, 12), g, 20, 20, 0);
        w.set_site(at(32, 11), 20.0, 0.0, 0.0);

        w.step();
        REQUIRE(w.cell_count() == 2);
        REQUIRE(w.cells().alive[at(32, 11)]);
        const bool left_stayed = w.cells().alive[at(32, 10)];
        const bool right_stayed = w.cells().alive[at(32, 12)];
        REQUIRE(left_stayed != right_stayed);
        const int loser = left_stayed ? at(32, 10) : at(32, 12);
        CHECK(w.cells().moved[loser] == 0);
        CHECK(w.minerals()[loser] == Approx(0.2));
        (left_stayed ? right_wins : left_wins)++;
    }
    CHECK(left_wins > 0);
    CHECK(right_wins > 0);
}

TEST_CASE("move: sociability pulls toward kin or pushes away") {
    const auto p = lab_params();
    for (const double soc : {-1.0, 1.0}) {
        for (std::uint64_t seed = 0; seed < 30; ++seed) {
            evo::World w(p, seed);
            clear_ground(w);
            w.add_cell(at(32, 12), still(p), 20, 20, 0);  // kin: same tag
            auto g = still(p);
            g[evo::kMotility] = 1.0;
            g[evo::kAppetite] = 0.0;
            g[evo::kSociability] = soc;
            w.add_cell(at(32, 10), g, 20, 20, 0);

            w.step();
            bool next_to_kin = false;
            for (const int row : {31, 32, 33}) next_to_kin |= w.cells().alive[at(row, 11)] != 0;
            CHECK(next_to_kin == (soc > 0));
        }
    }
}

TEST_CASE("divide: a released clone, costs, daughter state, cooldown of 5 waiting ticks") {
    const auto p = lab_params();
    evo::World w(p, 4);
    clear_ground(w);
    const int m = at(32, 10);
    w.add_cell(m, still(p), 50, 50, 10);
    const std::uint64_t mother_id = w.cells().id[m];
    const double total = w.matter().total();

    w.step();
    REQUIRE(w.births().size() == 1);
    const auto& b = w.births()[0];
    CHECK(b.parent_id == mother_id);
    CHECK(b.kind == evo::BirthKind::ReleasedClone);
    const auto d = static_cast<std::size_t>(b.site);
    bool adjacent = false;
    for (int dir = 0; dir < 8; ++dir) adjacent |= w.neighbor(m, dir) == b.site;
    CHECK(adjacent);
    CHECK(w.cells().store_a[d] == 6.0);
    CHECK(w.cells().store_b[d] == 6.0);
    CHECK(w.cells().age[d] == 0);
    CHECK(w.cells().parent_id[d] == mother_id);
    CHECK(w.cells().store_a[m] == Approx(50.0 - 0.21 - 10.0));  // upkeep 0.2 + 0.001 × 10
    CHECK(w.cells().store_b[m] == Approx(40.0));
    CHECK(w.cells().cooldown[m] == 5);
    CHECK(w.matter().total() == Approx(total).epsilon(1e-12));

    std::vector<std::uint64_t> mother_births = {0};
    while (w.tick() <= 12) {
        w.step();
        for (const auto& e : w.births()) {
            if (e.parent_id == mother_id) mother_births.push_back(e.tick);
        }
    }
    CHECK(mother_births == std::vector<std::uint64_t>{0, 6, 12});
    CHECK(w.matter().total() == Approx(total).epsilon(1e-12));
}

TEST_CASE("divide: not ready below 12 of a store or below age 10") {
    const auto p = lab_params();
    evo::World w(p, 1);
    clear_ground(w);
    w.add_cell(at(32, 10), still(p), 50, 11.9, 10);
    w.add_cell(at(32, 40), still(p), 50, 50, 9);  // ages to 10 in upkeep, then divides
    w.step();
    REQUIRE(w.births().size() == 1);
    CHECK(w.births()[0].parent_id == w.cells().id[at(32, 40)]);
}

TEST_CASE("divide: two mothers, one empty site; the loser pays nothing and keeps no cooldown") {
    const auto p = lab_params();
    std::set<std::uint64_t> winners;
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        evo::World w(p, seed);
        clear_ground(w);
        const int left = at(32, 10), right = at(32, 12), gap = at(32, 11);
        w.add_cell(left, still(p), 50, 50, 10);
        w.add_cell(right, still(p), 50, 50, 10);
        for (int s = 0; s < w.site_count(); ++s) {
            if (s != gap) w.add_cell(s, still(p), 5, 5, 0);  // blockers; not ready
        }
        w.step();
        REQUIRE(w.births().size() == 1);
        CHECK(w.births()[0].site == gap);
        const auto winner = static_cast<std::size_t>(
            w.births()[0].parent_id == w.cells().id[left] ? left : right);
        const auto loser = static_cast<std::size_t>(winner == left ? right : left);
        CHECK(w.cells().cooldown[winner] == 5);
        CHECK(w.cells().cooldown[loser] == 0);
        CHECK(w.cells().store_a[loser] + w.cells().store_b[loser] > 99.0);
        winners.insert(winner);
    }
    CHECK(winners.size() == 2);
}

TEST_CASE("clear_cells: every cell's matter falls onto its site as food (End world, M7-1)") {
    const auto p = lab_params();
    evo::World w(p, 1);
    clear_ground(w);
    REQUIRE(w.add_cell(at(32, 10), still(p), 7, 3, 10));
    REQUIRE(w.add_cell(at(32, 11), still(p), 1, 2, 10));
    REQUIRE(w.add_bond(at(32, 10), at(32, 11)));
    w.set_cell_virus(at(32, 11), 0.5);
    w.clear_cells();
    CHECK(w.cell_count() == 0);
    CHECK_FALSE(w.cells().alive[at(32, 10)]);
    CHECK(w.cells().bonds[at(32, 10)] == 0);
    CHECK(w.cells().infected[at(32, 11)] == 0);
    CHECK(w.food_a()[at(32, 10)] == Approx(7 + p.body_mass_a));
    CHECK(w.food_b()[at(32, 10)] == Approx(3 + p.body_mass_b));
    CHECK(w.food_a()[at(32, 11)] == Approx(1 + p.body_mass_a));
    CHECK(w.food_b()[at(32, 11)] == Approx(2 + p.body_mass_b));
    CHECK(w.deaths().empty());
    CHECK_FALSE(w.extinct_at().has_value());
}

TEST_CASE("clear_cells: conserves matter in a running world, which then runs on empty") {
    const evo::Params p;
    evo::World w(p, 2);
    for (int t = 0; t < 500; ++t) w.step();
    REQUIRE(w.cell_count() > 0);
    const double before = w.matter().total();
    w.clear_cells();
    CHECK(w.cell_count() == 0);
    CHECK(w.matter().cells == 0.0);
    CHECK(w.matter().total() == Approx(before).epsilon(1e-12));
    for (int t = 0; t < 50; ++t) w.step();
    CHECK(w.matter().total() == Approx(before).epsilon(1e-12));
    CHECK_FALSE(w.extinct_at().has_value());  // it had no cells when stepping began
}
