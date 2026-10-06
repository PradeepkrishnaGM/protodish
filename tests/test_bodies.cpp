// Unit tests for M4: bonds, bodies, roles, role split, Share and mating.

#include <cstdlib>

#include "doctest.h"
#include "lab.hpp"

using doctest::Approx;
using lab::at;
using lab::clear_ground;
using lab::still;

namespace {

int bond_count(const evo::World& w, int s) {
    int k = 0;
    for (int d = 0; d < 8; ++d) k += (w.cells().bonds[static_cast<std::size_t>(s)] >> d) & 1;
    return k;
}

// Fills every empty site with non-ready, non-feeding blockers.
void fill_with_blockers(evo::World& w, const evo::Params& p, int except) {
    for (int s = 0; s < w.site_count(); ++s) {
        if (s != except) w.add_cell(s, still(p), 5, 5, 0);
    }
}

evo::Genome mother_genome(const evo::Params& p) {
    evo::Genome g = still(p);
    g[evo::kAdhesion] = 0.0;
    return g;
}

}  // namespace

// ---- Bonds at division ----

TEST_CASE("divide: adhesion 1 attaches the daughter, adhesion 0 releases her") {
    const auto p = lab::params();
    for (const double adhesion : {0.0, 1.0}) {
        CAPTURE(adhesion);
        evo::World w(p, 3);
        clear_ground(w);
        const int m = at(32, 10);
        auto g = mother_genome(p);
        g[evo::kAdhesion] = adhesion;
        w.add_cell(m, g, 50, 50, 10);
        w.step();
        REQUIRE(w.births().size() == 1);
        const int d = w.births()[0].site;
        if (adhesion > 0) {
            CHECK(w.births()[0].kind == evo::BirthKind::AttachedClone);
            CHECK(w.bonded(m, d));
            CHECK(w.bonded(d, m));
        } else {
            CHECK(w.births()[0].kind == evo::BirthKind::ReleasedClone);
            CHECK(w.cells().bonds[static_cast<std::size_t>(d)] == 0);
            CHECK(w.cells().bonds[static_cast<std::size_t>(m)] == 0);
        }
    }
}

TEST_CASE("divide: an attached daughter bonds to her mother and the mother's bonded neighbors") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    const int m = at(32, 10), b = at(31, 10), gap = at(32, 11);
    auto g = mother_genome(p);
    g[evo::kAdhesion] = 1.0;
    w.add_cell(m, g, 50, 50, 10);
    w.add_cell(b, still(p), 5, 5, 0);
    REQUIRE(w.add_bond(m, b));
    fill_with_blockers(w, p, gap);

    w.step();
    REQUIRE(w.births().size() == 1);
    REQUIRE(w.births()[0].site == gap);
    CHECK(w.bonded(gap, m));
    CHECK(w.bonded(gap, b));                // b is bonded to the mother and next to the gap
    CHECK_FALSE(w.bonded(gap, at(31, 11)));  // next to the gap, but not bonded to the mother
    CHECK(bond_count(w, gap) == 2);
    CHECK(bond_count(w, m) == 2);
}

// ---- Anchoring and prey ----

TEST_CASE("bonded cells do not move and are never prey to each other") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    const int x = at(32, 10), v = at(32, 11);
    auto gx = still(p);
    gx[evo::kMotility] = 1.0;
    gx[evo::kTag] = 0.0;
    gx[evo::kTolerance] = 0.0;
    gx[evo::kAttack] = 0.8;
    auto gv = gx;
    gv[evo::kTag] = 0.5;
    gv[evo::kAttack] = 0.0;
    w.add_cell(x, gx, 20, 20, 0);
    w.add_cell(v, gv, 20, 20, 0);
    REQUIRE(w.add_bond(x, v));

    w.step();
    CHECK(w.cells().alive[x]);
    CHECK(w.cells().alive[v]);
    CHECK_FALSE(w.treats_as_prey(x, v));
    CHECK(w.cells().drained[v] == 0);
}

// ---- Roles and role split ----

TEST_CASE("role: the center of a fully bonded 3x3 block is inner; role split adjusts genes and upkeep") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    auto g = still(p);
    g[evo::kRoleSplit] = 0.5;
    g[evo::kHarvest] = 0.8;
    g[evo::kAttack] = 0.4;
    g[evo::kDefense] = 0.2;
    for (int r = 31; r <= 33; ++r) {
        for (int c = 10; c <= 12; ++c) w.add_cell(at(r, c), g, 20, 20, 0);
    }
    for (int r = 31; r <= 33; ++r) {
        for (int c = 10; c <= 12; ++c) {
            for (int r2 = 31; r2 <= 33; ++r2) {
                for (int c2 = 10; c2 <= 12; ++c2) {
                    if (std::abs(r - r2) <= 1 && std::abs(c - c2) <= 1 && (r != r2 || c != c2)) {
                        w.add_bond(at(r, c), at(r2, c2));
                    }
                }
            }
        }
    }
    const int center = at(32, 11), edge = at(32, 10);
    REQUIRE(w.is_inner(center));
    REQUIRE_FALSE(w.is_inner(edge));

    w.step();
    const auto& c = w.cells();
    CHECK(c.eff_harvest[center] == Approx(0.8 * 1.5));  // not clamped (DECISIONS M4-1)
    CHECK(c.eff_attack[center] == Approx(0.4 * 0.5));
    CHECK(c.eff_defense[center] == Approx(0.2 * 0.5));
    CHECK(c.eff_harvest[edge] == Approx(0.8 * 0.5));
    CHECK(c.eff_attack[edge] == Approx(0.4 * 1.5));
    CHECK(c.eff_defense[edge] == Approx(0.2 * 1.5));
    // Center upkeep: alive + adjusted harvest, attack, defense + crowding (8 − 3) × 0.03.
    const double cost = 0.2 + 0.2 * 1.2 + 0.5 * 0.2 + 0.3 * 0.1 + 0.03 * 5;
    CHECK(w.minerals()[center] == Approx(cost));
}

TEST_CASE("role split also applies to free cells, which are outer") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    auto g = still(p);
    g[evo::kRoleSplit] = -0.5;
    g[evo::kHarvest] = 0.6;
    w.add_cell(at(32, 10), g, 20, 20, 0);
    w.step();
    CHECK(w.cells().eff_harvest[at(32, 10)] == Approx(0.6 * 1.5));
}

// ---- Share ----

TEST_CASE("share: surplus above 10 split evenly among bonded cells; dormant cells receive only") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    const int l = at(32, 10), c = at(32, 11), r = at(32, 12);
    auto giver = still(p);
    giver[evo::kShare] = 1.0;
    auto sleeper = giver;
    sleeper[evo::kDormancy] = 1.0;  // nothing in reach: dormant
    w.add_cell(l, still(p), 48, 5, 0);
    w.add_cell(c, giver, 20, 10, 0);
    w.add_cell(r, sleeper, 40, 40, 0);
    REQUIRE(w.add_bond(l, c));
    REQUIRE(w.add_bond(c, r));
    const double total = w.matter().total();

    w.step();
    const auto& s = w.cells();
    REQUIRE(s.awake[r] == 0);
    // c gives 1 × (20 − 10) = 10 A, 5 to each side; B is not above 10.
    CHECK(s.store_a[c] == Approx(10.0 - 0.2));
    CHECK(s.store_b[c] == Approx(10.0));
    CHECK(s.store_a[l] == Approx(50.0 - 0.2));  // 48 + 5 overflows by 3
    CHECK(w.food_a()[l] == Approx(3.0));
    CHECK(s.store_a[r] == Approx(45.0 - 0.02));  // received 5, gave nothing, dormant upkeep
    CHECK(s.store_b[r] == Approx(40.0));
    CHECK(w.matter().total() == Approx(total).epsilon(1e-12));
}

// ---- Death splits bodies ----

TEST_CASE("death breaks bonds and splits a line body into two bodies") {
    const auto p = lab::params();
    evo::World w(p, 1);
    clear_ground(w);
    for (int col = 10; col <= 14; ++col) {
        w.add_cell(at(32, col), still(p), col == 12 ? 0.05 : 20.0, col == 12 ? 0.05 : 20.0, 0);
    }
    for (int col = 10; col < 14; ++col) REQUIRE(w.add_bond(at(32, col), at(32, col + 1)));
    {
        const auto labels = w.body_labels();
        for (int col = 11; col <= 14; ++col) CHECK(labels[at(32, col)] == labels[at(32, 10)]);
    }

    w.step();
    REQUIRE_FALSE(w.cells().alive[at(32, 12)]);
    CHECK_FALSE(w.bonded(at(32, 11), at(32, 12)));
    CHECK(bond_count(w, at(32, 11)) == 1);
    CHECK(bond_count(w, at(32, 13)) == 1);
    const auto labels = w.body_labels();
    CHECK(labels[at(32, 12)] == -1);
    CHECK(labels[at(32, 10)] == labels[at(32, 11)]);
    CHECK(labels[at(32, 13)] == labels[at(32, 14)]);
    CHECK(labels[at(32, 10)] != labels[at(32, 13)]);
}

// ---- Mating ----

namespace {

struct MatingScene {
    evo::Genome m;
    evo::Genome q;
};

MatingScene mating_genomes(const evo::Params& p) {
    MatingScene s{mother_genome(p), mother_genome(p)};
    s.m[evo::kMating] = 1.0;
    s.q[evo::kMating] = 1.0;
    s.q[evo::kTag] = 0.52;  // within both tolerances (0.1)
    for (const auto gene : {evo::kAppetite, evo::kCaution, evo::kBoldness, evo::kShare,
                            evo::kResistance, evo::kSociability}) {
        s.m[gene] = 0.1;
        s.q[gene] = 0.9;
    }
    return s;
}

}  // namespace

TEST_CASE("mating: a ready kin neighbor of the site becomes the second parent") {
    const auto p = lab::params();
    const auto genomes = mating_genomes(p);
    int from_m = 0, from_q = 0;
    for (std::uint64_t seed = 0; seed < 8; ++seed) {
        CAPTURE(seed);
        evo::World w(p, seed);
        clear_ground(w);
        const int left = at(32, 10), right = at(32, 12), gap = at(32, 11);
        w.add_cell(left, genomes.m, 50, 50, 10);
        w.add_cell(right, genomes.q, 50, 50, 10);
        fill_with_blockers(w, p, gap);  // both want the gap; the loser is a ready partner
        const auto id_left = w.cells().id[left], id_right = w.cells().id[right];
        const double total = w.matter().total();

        w.step();
        REQUIRE(w.births().size() == 1);
        const auto& b = w.births()[0];
        CHECK(b.kind == evo::BirthKind::Mating);
        CHECK(b.site == gap);
        CHECK(b.parent2_id != 0);
        CHECK(((b.parent_id == id_left && b.parent2_id == id_right) ||
               (b.parent_id == id_right && b.parent2_id == id_left)));
        CHECK(w.cells().parent2_id[gap] == b.parent2_id);
        for (const int parent : {left, right}) {
            const auto ps = static_cast<std::size_t>(parent);
            CHECK(w.cells().cooldown[ps] == 5);
            // Each pays 5 A and 5 B. Upkeep: alive 0.2, aging 0.001 × 10, crowding
            // 0.03 × (7 − 3), resistance 0.2 × gene.
            const double upkeep = 0.2 + 0.01 + 0.12 + 0.2 * w.cells().genes[evo::kResistance][ps];
            CHECK(w.cells().store_a[ps] + w.cells().store_b[ps] == Approx(100.0 - 10.0 - upkeep));
        }
        CHECK(w.matter().total() == Approx(total).epsilon(1e-12));
        for (const auto gene : {evo::kAppetite, evo::kCaution, evo::kBoldness, evo::kShare,
                                evo::kResistance, evo::kSociability}) {
            if (b.genome[gene] == genomes.m[gene]) ++from_m;
            if (b.genome[gene] == genomes.q[gene]) ++from_q;
        }
    }
    CHECK(from_m > 5);
    CHECK(from_q > 5);
}

TEST_CASE("mating falls back to a clone without a mutual-kin, ready partner") {
    const auto p = lab::params();
    auto genomes = mating_genomes(p);
    double partner_store_b = 50.0;

    SUBCASE("partner does not regard the mother as kin") { genomes.q[evo::kTolerance] = 0.0; }
    SUBCASE("mating gene 0") {
        genomes.m[evo::kMating] = 0.0;
        genomes.q[evo::kMating] = 0.0;
    }
    SUBCASE("partner not ready") { partner_store_b = 11.0; }

    evo::World w(p, 2);
    clear_ground(w);
    const int left = at(32, 10), right = at(32, 12), gap = at(32, 11);
    w.add_cell(left, genomes.m, 50, 50, 10);
    w.add_cell(right, genomes.q, 50, partner_store_b, 10);
    fill_with_blockers(w, p, gap);
    w.step();
    REQUIRE(w.births().size() == 1);
    CHECK(w.births()[0].kind == evo::BirthKind::ReleasedClone);
    CHECK(w.births()[0].parent2_id == 0);
}

TEST_CASE("mating: a partner bonded to the mother is not eligible") {
    const auto p = lab::params();
    const auto genomes = mating_genomes(p);
    evo::World w(p, 2);
    clear_ground(w);
    const int m = at(32, 10), q = at(31, 11), gap = at(32, 11);
    w.add_cell(m, genomes.m, 50, 50, 10);
    w.add_cell(q, genomes.q, 50, 50, 10);
    REQUIRE(w.add_bond(m, q));
    fill_with_blockers(w, p, gap);
    w.step();
    REQUIRE(w.births().size() == 1);
    CHECK(w.births()[0].kind == evo::BirthKind::ReleasedClone);
}
