// Click-to-inspect tracker (M7e).

#include <string>

#include "doctest.h"
#include "inspect.hpp"
#include "lab.hpp"

using lab::at;

TEST_CASE("inspect: selecting an empty site selects nothing") {
    const evo::Params p = lab::params();
    evo::World w(p, 1);
    evo::CellTracker t;
    t.select(w, at(32, 5));
    CHECK(t.state() == evo::CellTracker::State::None);
    t.select(w, -1);
    CHECK(t.state() == evo::CellTracker::State::None);
}

TEST_CASE("inspect: the tracker follows a moving cell by ID") {
    const evo::Params p = lab::params();
    evo::World w(p, 3);
    evo::Genome g = lab::still(p);
    g[evo::kMotility] = 1.0;  // moves every tick
    REQUIRE(w.add_cell(at(32, 5), g, 40, 40, 10));
    evo::CellTracker t;
    t.select(w, at(32, 5));
    const auto id = t.id();
    int moves = 0;
    for (int k = 0; k < 20; ++k) {
        const int before = t.site();
        w.step();
        t.update(w);
        REQUIRE(t.state() == evo::CellTracker::State::Alive);
        CHECK(w.cells().id[static_cast<std::size_t>(t.site())] == id);
        moves += t.site() != before ? 1 : 0;
    }
    CHECK(moves > 10);
}

TEST_CASE("inspect: the tracker records the tick and cause of death") {
    const evo::Params p = lab::params();
    evo::World w(p, 1);
    lab::clear_ground(w);
    REQUIRE(w.add_cell(at(32, 5), lab::still(p), 0.3, 0.0, 10));  // cannot pay 2 ticks of upkeep
    evo::CellTracker t;
    t.select(w, at(32, 5));
    std::uint64_t died = 0;
    for (int k = 0; k < 5 && t.state() == evo::CellTracker::State::Alive; ++k) {
        died = w.tick();
        w.step();
        t.update(w);
    }
    REQUIRE(t.state() == evo::CellTracker::State::Dead);
    CHECK(t.death_tick() == died);
    CHECK(t.death_cause() == evo::DeathCause::Starved);
    CHECK(t.site() == at(32, 5));
    CHECK(std::string(evo::death_cause_name(t.death_cause())) == "starved");
}

TEST_CASE("inspect: End world marks the selected cell removed") {
    const evo::Params p = lab::params();
    evo::World w(p, 1);
    REQUIRE(w.add_cell(at(32, 5), lab::still(p), 10, 10, 10));
    evo::CellTracker t;
    t.select(w, at(32, 5));
    w.clear_cells();
    t.removed(w);
    CHECK(t.state() == evo::CellTracker::State::Removed);
    CHECK(t.death_tick() == w.tick());
}

TEST_CASE("inspect: body size follows bonds") {
    const evo::Params p = lab::params();
    evo::World w(p, 1);
    for (const int col : {5, 6, 7, 9}) REQUIRE(w.add_cell(at(32, col), lab::still(p), 10, 10, 10));
    REQUIRE(w.add_bond(at(32, 5), at(32, 6)));
    REQUIRE(w.add_bond(at(32, 6), at(32, 7)));
    CHECK(evo::body_size(w, at(32, 5)) == 3);
    CHECK(evo::body_size(w, at(32, 7)) == 3);
    CHECK(evo::body_size(w, at(32, 9)) == 1);
    CHECK(evo::body_size(w, at(32, 20)) == 0);
}
