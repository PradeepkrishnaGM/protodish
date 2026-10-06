// The project's required invariants: matter conservation and determinism.
// Long runs are in the doctest suite "slow" (ctest label "slow"); skip them with ctest -LE slow.

#include <cmath>

#include "doctest.h"
#include "genome.hpp"
#include "world.hpp"

namespace {
constexpr double kRelTol = 1e-9;
}

TEST_CASE("invariant: matter is conserved every tick" * doctest::test_suite("slow")) {
    evo::Params p;
    evo::World w(p, 2026);
    const double expected = 16384.0 * (4.0 + 4.0 + 2.0) + 50.0 * (15.0 + 15.0 + 4.0 + 4.0);
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

TEST_CASE("invariant: no site ever goes negative" * doctest::test_suite("slow")) {
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

namespace {
void require_ranges(const evo::World& w) {
    const auto& c = w.cells();
    const double store_max = w.params().store_max;
    int alive = 0;
    bool ok = true;
    for (std::size_t s = 0; s < c.alive.size(); ++s) {
        if (!c.alive[s]) continue;
        ++alive;
        ok &= c.store_a[s] >= 0.0 && c.store_a[s] <= store_max;
        ok &= c.store_b[s] >= 0.0 && c.store_b[s] <= store_max;
        ok &= c.stress[s] >= 0.0 && c.stress[s] <= 1.0;
        for (std::size_t i = 0; i < evo::kGeneCount; ++i) {
            const auto& info = evo::kGeneInfo[i];
            const double v = c.genes[i][s];
            ok &= v >= info.min && (info.circular ? v < info.max : v <= info.max);
        }
        if (!ok) {
            FAIL_CHECK("out of range at site " << s << ", tick " << w.tick());
            break;
        }
    }
    REQUIRE(ok);
    REQUIRE(alive == w.cell_count());

    // Bonds: symmetric, and only between living cells.
    for (int s = 0; s < w.site_count(); ++s) {
        const unsigned mask = c.bonds[static_cast<std::size_t>(s)];
        if (!c.alive[static_cast<std::size_t>(s)]) {
            ok &= mask == 0;
            continue;
        }
        for (int d = 0; d < 8; ++d) {
            const auto t = static_cast<std::size_t>(w.neighbor(s, d));
            const bool here = (mask >> d) & 1u;
            const bool there = (c.bonds[t] >> ((d + 4) % 8)) & 1u;
            ok &= here == there;
            ok &= !here || c.alive[t];
        }
        if (!ok) {
            FAIL_CHECK("bond inconsistency at site " << s << ", tick " << w.tick());
            break;
        }
    }
    REQUIRE(ok);
}
}  // namespace

TEST_CASE("invariant: 50 ancestors run 10,000 ticks; matter and ranges hold every tick" * doctest::test_suite("slow")) {
    for (const std::uint64_t seed : {1ULL, 2ULL, 3ULL}) {
        CAPTURE(seed);
        evo::Params p;
        evo::World w(p, seed);
        const double start = w.matter().total();
        int peak = 0;
        std::uint64_t births = 0;
        for (int t = 0; t < 10000; ++t) {
            w.step();
            births += w.births().size();
            peak = std::max(peak, w.cell_count());
            REQUIRE(std::fabs(w.matter().total() - start) <= kRelTol * start);
            require_ranges(w);
        }
        MESSAGE("seed " << seed << ": " << w.cell_count() << " cells at 10k, peak " << peak
                        << ", births " << births);
    }
}

TEST_CASE("invariant: conflict scenario; matter and ranges hold while every M3 rule fires" * doctest::test_suite("slow")) {
    evo::Params p;
    p.initial_cells = 400;
    p.mutation_base = 0.2;  // spread tags fast so cells stop being kin
    p.ancestor[evo::kTolerance] = 0.02;
    p.ancestor[evo::kAttack] = 0.3;
    p.ancestor[evo::kDefense] = 0.1;
    p.ancestor[evo::kPhotosynthesis] = 0.4;
    p.ancestor[evo::kDormancy] = 0.3;
    p.ancestor[evo::kMutability] = 0.5;
    p.ancestor[evo::kBoldness] = 0.3;
    p.ancestor[evo::kCaution] = 0.3;

    evo::World w(p, 9);
    const double start = w.matter().total();
    int drained_ticks = 0, dormant_ticks = 0, producing_ticks = 0, stressed_ticks = 0;
    for (int t = 0; t < 3000 && w.cell_count() > 0; ++t) {
        w.step();
        REQUIRE(std::fabs(w.matter().total() - start) <= kRelTol * start);
        require_ranges(w);
        const auto& c = w.cells();
        bool drained = false, dormant = false, producing = false, stressed = false;
        for (std::size_t s = 0; s < c.alive.size(); ++s) {
            if (!c.alive[s]) continue;
            drained |= c.drained[s] != 0;
            dormant |= c.awake[s] == 0;
            producing |= c.awake[s] && c.genes[evo::kPhotosynthesis][s] > 0 && c.gross_intake[s] > 0;
            stressed |= c.stress[s] > 0.3;
        }
        drained_ticks += drained;
        dormant_ticks += dormant;
        producing_ticks += producing;
        stressed_ticks += stressed;
    }
    MESSAGE("ticks with drains " << drained_ticks << ", dormancy " << dormant_ticks
                                 << ", producers feeding " << producing_ticks << ", stress > 0.3 "
                                 << stressed_ticks << "; cells at end " << w.cell_count());
    CHECK(drained_ticks > 100);
    CHECK(dormant_ticks > 100);
    CHECK(producing_ticks > 100);
    CHECK(stressed_ticks > 100);
}

TEST_CASE("invariant: body scenario; bonds stay consistent while bodies grow, share, split and mate" *
          doctest::test_suite("slow")) {
    // A mild world (constant 15 °C and light, more sparks, no attack) so that bodies
    // live long enough to grow, share, split and mate. The point is coverage, not balance.
    evo::Params p;
    p.initial_cells = 300;
    p.temp_season_amp = 0.0;
    p.temp_latitude_amp = 0.0;
    p.light_season_amp = 0.0;
    p.light_latitude_amp = 0.0;
    p.spark_base = 1500.0;
    p.spark_season_amp = 0.0;
    p.ancestor[evo::kAdhesion] = 0.6;
    p.ancestor[evo::kShare] = 0.5;
    p.ancestor[evo::kRoleSplit] = 0.3;
    p.ancestor[evo::kMating] = 1.0;
    p.ancestor[evo::kPhotosynthesis] = 0.3;

    evo::World w(p, 4);
    const double start = w.matter().total();
    int attached = 0, matings = 0, bonded_deaths = 0, max_body = 0, inner_samples = 0;
    for (int t = 0; t < 3000 && w.cell_count() > 0; ++t) {
        // Count deaths of bonded cells: bonds are about to break.
        std::vector<std::uint8_t> was_bonded(w.cells().bonds);
        w.step();
        REQUIRE(std::fabs(w.matter().total() - start) <= kRelTol * start);
        require_ranges(w);
        for (const auto& b : w.births()) {
            attached += b.kind == evo::BirthKind::AttachedClone;
            matings += b.kind == evo::BirthKind::Mating;
        }
        for (const auto& d : w.deaths()) bonded_deaths += was_bonded[static_cast<std::size_t>(d.site)] != 0;
        if (t % 100 == 0) {
            const auto labels = w.body_labels();
            std::vector<int> size(labels.size(), 0);
            for (const int l : labels) {
                if (l >= 0) max_body = std::max(max_body, ++size[static_cast<std::size_t>(l)]);
            }
            for (int s = 0; s < w.site_count(); ++s) inner_samples += w.cells().alive[s] && w.is_inner(s);
        }
    }
    MESSAGE("attached births " << attached << ", matings " << matings << ", deaths of bonded cells "
                               << bonded_deaths << ", largest body " << max_body
                               << ", inner cells seen in samples " << inner_samples << "; cells at end "
                               << w.cell_count());
    // Inner cells are rare in free-running worlds (bodies stay at about 10 cells), so the
    // inner role and role split are covered by unit tests in test_bodies.cpp instead.
    CHECK(attached > 1000);
    CHECK(matings > 0);
    CHECK(bonded_deaths > 1000);
    CHECK(max_body >= 6);
}
