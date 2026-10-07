// Population history for the app's graph (M7d).

#include <algorithm>

#include "doctest.h"
#include "history.hpp"
#include "lab.hpp"

namespace {

evo::PopulationCounts counts(int cells) { return {cells, cells / 2, cells / 3}; }

}  // namespace

TEST_CASE("history: one sample per tick until full") {
    evo::PopulationHistory h(100);
    for (std::uint64_t t = 0; t < 50; ++t) h.add(t, counts(static_cast<int>(t)));
    CHECK(h.size() == 50);
    CHECK(h.ticks_per_sample() == 1);
    const auto s = h.downsample(1000);
    REQUIRE(s.cells.size() == 50);
    CHECK(s.tick[7] == 7);
    CHECK(s.cells[7] == 7);
    CHECK(s.producers[7] == 3);
    CHECK(s.infected[9] == 3);
}

TEST_CASE("history: downsampling keeps the peak of each bucket") {
    evo::PopulationHistory h;
    for (std::uint64_t t = 0; t < 1000; ++t) h.add(t, counts(t == 437 ? 5000 : 10));
    const auto s = h.downsample(10);
    REQUIRE(s.cells.size() == 10);
    CHECK(s.tick[4] == 400);
    CHECK(s.cells[4] == 5000);  // a one-tick spike survives
    CHECK(s.cells[3] == 10);
    CHECK(*std::max_element(s.infected.begin(), s.infected.end()) == 5000 / 3);
    CHECK(h.downsample(3).cells.size() == 3);  // 334 samples per point: 334, 334, 332
}

TEST_CASE("history: memory stays bounded; merged samples keep peaks and tick labels") {
    for (const std::size_t cap : {std::size_t{8}, std::size_t{9}}) {
        evo::PopulationHistory h(cap);
        const std::uint64_t n = 1000;
        for (std::uint64_t t = 0; t < n; ++t) h.add(t, counts(t == 613 ? 9999 : static_cast<int>(t % 7)));
        CHECK(h.size() <= cap);
        const auto stride = h.ticks_per_sample();
        CHECK(stride * cap >= n);  // every tick is still covered
        const auto s = h.downsample(cap);
        CHECK(s.tick.front() == 0);
        for (std::size_t k = 0; k < s.tick.size(); ++k) CHECK(s.tick[k] == k * stride);
        CHECK(*std::max_element(s.cells.begin(), s.cells.end()) == 9999);
        CHECK(s.cells[613 / stride] == 9999);
    }
}

TEST_CASE("history: counts come from the world") {
    const evo::Params p = lab::params();
    evo::World w(p, 1);
    evo::Genome g = lab::still(p);
    g[evo::kPhotosynthesis] = 0.5;  // above harvest 0: a producer
    REQUIRE(w.add_cell(lab::at(32, 5), g, 10, 10, 10));
    REQUIRE(w.add_cell(lab::at(32, 9), lab::still(p), 10, 10, 10));
    REQUIRE(w.add_cell(lab::at(32, 13), lab::still(p), 10, 10, 10));
    w.set_cell_virus(lab::at(32, 13), 0.5);
    const evo::PopulationCounts c = evo::count_population(w);
    CHECK(c.cells == 3);
    CHECK(c.producers == 1);
    CHECK(c.infected == 1);
}

TEST_CASE("history: a window from a given tick") {
    evo::PopulationHistory h;
    for (std::uint64_t t = 0; t < 100; ++t) h.add(t, counts(t < 50 ? 900 : static_cast<int>(t)));
    const auto s = h.downsample(1000, 60);
    REQUIRE(s.tick.size() == 40);
    CHECK(s.tick.front() == 60);
    CHECK(s.cells.front() == 60);
    CHECK(h.downsample(5, 60).cells.size() == 5);
    CHECK(h.downsample(5, 60).cells[0] == 67);  // the peak of ticks 60-67, not the earlier 900s
    CHECK(h.downsample(10, 500).tick.empty());
    CHECK(h.downsample(1000, 0).tick.size() == 100);
}
