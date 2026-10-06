// Census and lineage log (M6).

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "doctest.h"
#include "lab.hpp"
#include "records.hpp"

using lab::at;

TEST_CASE("tag clusters: gaps wider than 0.1 split the circle") {
    auto count = [](std::vector<double> tags, int min_size = 1) {
        return evo::count_tag_clusters(tags, 0.1, min_size);
    };
    CHECK(count({}).all == 0);
    CHECK(count({0.3}).all == 1);
    CHECK(count({0.0, 0.05, 0.95}).all == 1);    // joined across the wrap
    CHECK(count({0.1, 0.25}).all == 2);          // gap 0.15 one way, 0.85 the other
    CHECK(count({0.1, 0.2}).all == 1);           // a gap of exactly 0.1 does not split
    CHECK(count({0.0, 0.08, 0.16, 0.24, 0.32, 0.40, 0.48, 0.56, 0.64, 0.72, 0.80, 0.88}).all == 1);
    CHECK(count({0.1, 0.3, 0.5, 0.7}).all == 4);
    CHECK(count({0.95, 0.02, 0.5}).all == 2);    // 0.95 and 0.02 are 0.07 apart

    const evo::TagClusters c = count({0.1, 0.11, 0.12, 0.5, 0.8, 0.81}, 3);
    CHECK(c.all == 3);
    CHECK(c.large == 1);  // only the cluster around 0.1 has 3 cells
}

TEST_CASE("census: body buckets") {
    CHECK(evo::body_bucket(2) == 0);
    CHECK(evo::body_bucket(3) == 1);
    CHECK(evo::body_bucket(4) == 1);
    CHECK(evo::body_bucket(5) == 2);
    CHECK(evo::body_bucket(16) == 3);
    CHECK(evo::body_bucket(17) == 4);
    CHECK(evo::body_bucket(32) == 4);
    CHECK(evo::body_bucket(33) == 5);
    CHECK(evo::body_bucket(5000) == 5);
}

TEST_CASE("census: counts cells, bodies, roles, producers and clusters") {
    evo::Params p = lab::params();
    p.cluster_min_size = 9;
    evo::World w(p, 1);
    evo::Genome g = lab::still(p);

    // A 3 x 3 block bonded all round: 9 cells, one inner (the centre has 8 bonded neighbors).
    std::vector<int> block;
    for (int r = 10; r < 13; ++r) {
        for (int col = 10; col < 13; ++col) {
            REQUIRE(w.add_cell(at(r, col), g, 20, 20, 10));
            block.push_back(at(r, col));
        }
    }
    for (std::size_t i = 0; i < block.size(); ++i) {
        for (std::size_t j = i + 1; j < block.size(); ++j) w.add_bond(block[i], block[j]);
    }
    // A body of 2, three free cells, one of them a producer with a distant tag, one infected.
    REQUIRE(w.add_cell(at(40, 40), g, 20, 20, 10));
    REQUIRE(w.add_cell(at(40, 41), g, 20, 20, 10));
    REQUIRE(w.add_bond(at(40, 40), at(40, 41)));
    evo::Genome producer = g;
    producer[evo::kPhotosynthesis] = 0.9;
    producer[evo::kHarvest] = 0.1;
    producer[evo::kTag] = 0.0;
    REQUIRE(w.add_cell(at(70, 70), producer, 20, 20, 10));
    REQUIRE(w.add_cell(at(90, 90), g, 20, 20, 10));
    REQUIRE(w.add_cell(at(100, 5), g, 20, 20, 10));
    w.set_cell_virus(at(90, 90), 0.5);

    const evo::Census c = evo::take_census(w);
    CHECK(c.cells == 14);
    CHECK(c.free_cells == 3);
    CHECK(c.body_cells == 11);
    CHECK(c.bodies == 2);
    CHECK(c.bodies_by_size[0] == 1);  // 2 cells
    CHECK(c.bodies_by_size[3] == 1);  // 9 cells
    CHECK(c.largest_body == 9);
    CHECK(c.inner_cells == 1);
    CHECK(c.infected == 1);
    CHECK(c.dormant == 0);  // nothing has been sensed before the first tick
    CHECK(c.producers == 1);
    CHECK(c.consumers == 13);
    CHECK(c.clusters.all == 2);    // tag 0.5 (13 cells) and tag 0.0 (1 cell)
    CHECK(c.clusters.large == 1);  // only the first has at least 9 cells
    CHECK(c.gene_mean[evo::kTag] == doctest::Approx(0.5 * 13 / 14));
    CHECK(c.hash == w.state_hash());
}

TEST_CASE("census: newborns are not counted as dormant") {
    evo::Params p = lab::params();
    evo::World w(p, 3);
    evo::Genome g = lab::still(p);
    REQUIRE(w.add_cell(at(32, 32), g, 30, 30, 10));
    w.step();
    REQUIRE(w.births().size() == 1);
    CHECK(evo::take_census(w).dormant == 0);
}

TEST_CASE("census: interval counts add up births and deaths") {
    evo::Params p;
    evo::World w(p, 11);
    evo::IntervalCounts n;
    std::uint64_t births = 0, deaths = 0;
    int peak = 0;
    for (int t = 0; t < 300; ++t) {
        w.step();
        n.add_tick(w);
        births += w.births().size();
        deaths += w.deaths().size();
        peak = std::max(peak, w.cell_count());
    }
    CHECK(n.births[0] + n.births[1] + n.births[2] == births);
    CHECK(n.deaths[0] + n.deaths[1] + n.deaths[2] == deaths);
    CHECK(n.peak_cells == peak);
    CHECK(births > 0);
    n.reset();
    CHECK(n.peak_cells == 0);
    CHECK(n.births[1] == 0);
}

namespace {

std::uint32_t u32_at(const std::vector<unsigned char>& b, std::size_t i) {
    return static_cast<std::uint32_t>(b[i]) | static_cast<std::uint32_t>(b[i + 1]) << 8 |
           static_cast<std::uint32_t>(b[i + 2]) << 16 | static_cast<std::uint32_t>(b[i + 3]) << 24;
}

}  // namespace

TEST_CASE("lineage log: every death and parent refers to a logged cell") {
    const auto path = std::filesystem::temp_directory_path() / "evo_test_lineage.bin";
    evo::Params p;
    evo::World w(p, 21);
    std::uint64_t births = 0, deaths = 0;
    {
        evo::LineageWriter lw;
        REQUIRE(lw.open(path.string(), w).empty());
        for (int t = 0; t < 600; ++t) {
            w.step();
            lw.add_tick(w);
            births += w.births().size();
            deaths += w.deaths().size();
        }
        REQUIRE(lw.close().empty());
    }
    std::ifstream in(path, std::ios::binary);
    const std::vector<unsigned char> b((std::istreambuf_iterator<char>(in)), {});
    std::filesystem::remove(path);

    REQUIRE(b.size() >= evo::kLineageHeaderSize);
    CHECK(std::string(b.begin(), b.begin() + 8) == "EVOLIN01");
    CHECK(u32_at(b, 8) == 1);
    CHECK(u32_at(b, 12) == evo::kGeneCount);
    CHECK(u32_at(b, 16) == evo::kLineageBirthSize);
    CHECK(u32_at(b, 20) == evo::kLineageDeathSize);
    CHECK(u32_at(b, 24) == 21);  // seed, low half

    std::set<std::uint32_t> known;
    std::uint64_t n_ancestors = 0, n_births = 0, n_deaths = 0;
    bool parents_ok = true, deaths_ok = true;
    std::size_t i = evo::kLineageHeaderSize;
    while (i < b.size()) {
        if (b[i] == 1) {
            REQUIRE(i + evo::kLineageBirthSize <= b.size());
            const std::uint32_t id = u32_at(b, i + 8);
            const std::uint32_t parent = u32_at(b, i + 12);
            const std::uint32_t parent2 = u32_at(b, i + 16);
            if (b[i + 1] == evo::kLineageAncestor) {
                ++n_ancestors;
            } else {
                ++n_births;
                parents_ok = parents_ok && known.count(parent) && (parent2 == 0 || known.count(parent2));
            }
            known.insert(id);
            i += evo::kLineageBirthSize;
        } else {
            REQUIRE(b[i] == 2);
            REQUIRE(i + evo::kLineageDeathSize <= b.size());
            deaths_ok = deaths_ok && known.count(u32_at(b, i + 8));
            ++n_deaths;
            i += evo::kLineageDeathSize;
        }
    }
    CHECK(i == b.size());
    CHECK(n_ancestors == 50);
    CHECK(n_births == births);
    CHECK(n_deaths == deaths);
    CHECK(parents_ok);
    CHECK(deaths_ok);
}

TEST_CASE("census: intake-based producers and consumers") {
    evo::Params p = lab::params();
    evo::World w(p, 5);
    evo::Genome producer = lab::still(p);
    producer[evo::kPhotosynthesis] = 1.0;  // makes food from minerals, eats nothing
    REQUIRE(w.add_cell(at(32, 10), producer, 11, 11, 10));
    REQUIRE(w.add_cell(at(32, 40), lab::eater(p, 0.5), 11, 11, 10));
    REQUIRE(w.add_cell(at(32, 70), lab::still(p), 11, 11, 10));  // takes in nothing
    w.step();
    const evo::Census c = evo::take_census(w);
    CHECK(c.producers_intake == 1);
    CHECK(c.consumers_intake == 1);
    REQUIRE(c.cells == 3);  // stores below 12: nobody divides
    CHECK(c.no_intake == 1);
    CHECK(c.producers == 1);  // the gene test agrees here
}

TEST_CASE("option D0: ancestors split into evenly spaced tag groups") {
    evo::Params p;
    p.initial_tag_groups = 3;
    evo::World w(p, 4);
    std::vector<double> tags;
    for (int s = 0; s < w.site_count(); ++s) {
        if (w.cells().alive[static_cast<std::size_t>(s)]) tags.push_back(w.cells().genes[evo::kTag][static_cast<std::size_t>(s)]);
    }
    REQUIRE(tags.size() == 50);
    int n0 = 0, n1 = 0, n2 = 0;
    for (const double t : tags) {
        if (std::fabs(t - 0.5) < 1e-12) ++n0;
        else if (std::fabs(t - (0.5 + 1.0 / 3.0)) < 1e-12) ++n1;
        else if (std::fabs(t - (0.5 + 2.0 / 3.0 - 1.0)) < 1e-12) ++n2;
    }
    CHECK(n0 == 17);
    CHECK(n1 == 17);
    CHECK(n2 == 16);
    CHECK(evo::take_census(w).clusters.all == 3);

    // The placement draws are the same as with one group: same sites, same RNG state.
    evo::Params one;
    evo::World u(one, 4);
    CHECK(u.cells().alive == w.cells().alive);
}

TEST_CASE("option D3b: generalist cost adds k x harvest x photosynthesis to upkeep") {
    auto stores_after_one_tick = [](double k) {
        evo::Params p = lab::params();
        p.cost_generalist = k;
        evo::World w(p, 2);
        evo::Genome g = lab::still(p);
        g[evo::kHarvest] = 0.6;
        g[evo::kPhotosynthesis] = 0.5;
        REQUIRE(w.add_cell(at(32, 32), g, 11, 11, 10));  // row 32: upkeep multiplier 1
        w.step();
        return w.cells().store_a[static_cast<std::size_t>(at(32, 32))] +
               w.cells().store_b[static_cast<std::size_t>(at(32, 32))];
    };
    CHECK(stores_after_one_tick(0.0) - stores_after_one_tick(0.4) == doctest::Approx(0.4 * 0.6 * 0.5));
}
