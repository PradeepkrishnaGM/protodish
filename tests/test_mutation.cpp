#include <cmath>

#include "doctest.h"
#include "mutation.hpp"

using doctest::Approx;

TEST_CASE("mutation chance: 5% calm, 25% fully stressed with mutability 1") {
    evo::Params p;
    CHECK(evo::mutation_chance(0.0, p) == Approx(0.05));
    CHECK(evo::mutation_chance(1.0, p) == Approx(0.25));
    CHECK(evo::mutation_chance(0.5, p) == Approx(0.15));
}

TEST_CASE("mutation: chance 0 changes nothing") {
    evo::Params p;
    evo::Rng rng(1);
    evo::Genome g = p.ancestor;
    for (int i = 0; i < 1000; ++i) evo::mutate(g, 0.0, rng, p);
    CHECK(g == p.ancestor);
}

TEST_CASE("mutation: genes stay in range from the edges (1M mutations), tag wraps") {
    evo::Params p;
    evo::Rng rng(2);
    for (int edge = 0; edge < 2; ++edge) {
        evo::Genome g{};
        for (std::size_t i = 0; i < evo::kGeneCount; ++i) {
            const auto& info = evo::kGeneInfo[i];
            g[i] = edge == 0 ? info.min : info.max;
        }
        g[evo::kTag] = edge == 0 ? 0.0 : std::nextafter(1.0, 0.0);
        for (int n = 0; n < 25000; ++n) {
            const evo::Genome before = g;
            evo::mutate(g, 1.0, rng, p);  // every gene mutates
            for (std::size_t i = 0; i < evo::kGeneCount; ++i) {
                const auto& info = evo::kGeneInfo[i];
                REQUIRE(g[i] >= info.min);
                if (info.circular) {
                    REQUIRE(g[i] < info.max);
                    REQUIRE(evo::tag_distance(before[i], g[i]) <= p.tag_mutation_step + 1e-12);
                } else {
                    REQUIRE(g[i] <= info.max);
                    const double step = p.mutation_step * (info.max - info.min);
                    REQUIRE(std::fabs(g[i] - before[i]) <= step + 1e-12);
                }
            }
            if (n % 50 == 0) {  // keep pushing against the edges
                for (std::size_t i = 0; i < evo::kGeneCount; ++i) {
                    const auto& info = evo::kGeneInfo[i];
                    if (!info.circular) g[i] = edge == 0 ? info.min : info.max;
                }
            }
        }
    }
}

TEST_CASE("mutation: a gene at its edge reflects back within one step") {
    evo::Params p;
    for (std::uint64_t seed = 0; seed < 1000; ++seed) {
        evo::Rng rng(seed);
        evo::Genome g = p.ancestor;
        g[evo::kHarvest] = 1.0;
        evo::mutate(g, 1.0, rng, p);
        CHECK(g[evo::kHarvest] <= 1.0);
        CHECK(g[evo::kHarvest] >= 0.9);
    }
}

TEST_CASE("mutation: about 5% of genes mutate at the base chance") {
    evo::Params p;
    evo::Rng rng(3);
    int changed = 0;
    const int trials = 20000;
    for (int t = 0; t < trials; ++t) {
        evo::Genome g = p.ancestor;
        evo::mutate(g, 0.05, rng, p);
        for (std::size_t i = 0; i < evo::kGeneCount; ++i) changed += g[i] != p.ancestor[i];
    }
    const double rate = static_cast<double>(changed) / (trials * evo::kGeneCount);
    CHECK(rate == Approx(0.05).epsilon(0.05));
}
