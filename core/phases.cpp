// Cell phases 2-9. Each phase gathers its decisions from the current state, then commits
// them, so no cell sees another's action from the same phase (DECISIONS.md, M2).

#include <algorithm>
#include <cmath>

#include "mutation.hpp"
#include "world.hpp"

namespace evo {

namespace {

struct Intent {
    int target;
    int source;
};

// Sorts intents by target (then source) and keeps one random winner per target.
// One draw per contested target, in ascending target order.
std::vector<Intent> resolve_conflicts(std::vector<Intent> intents, Rng& rng) {
    std::sort(intents.begin(), intents.end(), [](const Intent& a, const Intent& b) {
        return a.target != b.target ? a.target < b.target : a.source < b.source;
    });
    std::vector<Intent> winners;
    winners.reserve(intents.size());
    for (std::size_t i = 0; i < intents.size();) {
        std::size_t j = i;
        while (j < intents.size() && intents[j].target == intents[i].target) ++j;
        const auto n = static_cast<std::uint32_t>(j - i);
        winners.push_back(intents[i + (n > 1 ? rng.below(n) : 0u)]);
        i = j;
    }
    return winners;
}

}  // namespace

// ---- 2. Sense ----

void World::phase_sense() {
    collect_cell_sites();
    const auto& g = cells_.genes;
    for (const int site : cell_sites_) {
        const auto s = static_cast<std::size_t>(site);

        const double dt = (site_temperature(site) - g[kPreferredTemp][s]) / params_.thermal_width;
        cells_.thermal_eff[s] = std::max(0.0, 1.0 - dt * dt);

        const double diet = g[kDiet][s];
        double food = diet * cur_.food_a[s] + (1.0 - diet) * cur_.food_b[s];
        double minerals = cur_.minerals[s];
        for (int d = 0; d < 8; ++d) {
            const auto n = static_cast<std::size_t>(neighbor(site, d));
            if (cells_.alive[n]) continue;
            food += diet * cur_.food_a[n] + (1.0 - diet) * cur_.food_b[n];
            minerals += cur_.minerals[n];
        }
        const double food_term = std::min(1.0, food / params_.supply_food_norm);
        const double light_term = std::min(
            1.0, minerals / params_.supply_mineral_norm * site_light(site) * g[kPhotosynthesis][s]);
        cells_.supply[s] = (food_term + light_term) / 2.0;

        cells_.awake[s] = 1;  // dormancy arrives in M3
    }
}

// ---- 3. Move ----

void World::phase_move() {
    collect_cell_sites();
    const auto& g = cells_.genes;
    std::vector<Intent> intents;

    for (const int site : cell_sites_) {
        const auto s = static_cast<std::size_t>(site);
        cells_.moved[s] = 0;
        if (!cells_.awake[s]) continue;  // bonded cells are skipped from M4 on
        if (!rng_.chance(g[kMotility][s])) continue;

        const double diet = g[kDiet][s];
        const double tag = g[kTag][s];
        const double tolerance = g[kTolerance][s];
        int best[8];
        int n_best = 0;
        double best_score = 0.0;
        for (int d = 0; d < 8; ++d) {
            const int cand = neighbor(site, d);
            const auto c = static_cast<std::size_t>(cand);
            if (cells_.alive[c]) continue;

            const double food = std::min(
                1.0, (diet * cur_.food_a[c] + (1.0 - diet) * cur_.food_b[c]) / params_.move_food_norm);
            int kin = 0;
            for (int e = 0; e < 8; ++e) {
                const int other = neighbor(cand, e);
                const auto o = static_cast<std::size_t>(other);
                if (other == site || !cells_.alive[o]) continue;
                if (tag_distance(tag, g[kTag][o]) <= tolerance) ++kin;
            }
            // Prey and threat terms arrive in M3.
            const double score = g[kAppetite][s] * food +
                                 g[kSociability][s] * kin / params_.neighbor_norm;

            if (n_best == 0 || score > best_score) {
                best_score = score;
                n_best = 0;
                best[n_best++] = cand;
            } else if (score == best_score) {
                best[n_best++] = cand;
            }
        }
        if (n_best == 0) continue;
        const int target =
            best[n_best > 1 ? rng_.below(static_cast<std::uint32_t>(n_best)) : 0u];
        intents.push_back({target, site});
    }

    for (const Intent& m : resolve_conflicts(std::move(intents), rng_)) {
        cells_.move(static_cast<std::size_t>(m.source), static_cast<std::size_t>(m.target));
        cells_.moved[static_cast<std::size_t>(m.target)] = 1;
    }
}

// ---- 4. Feed ----

void World::phase_feed() {
    collect_cell_sites();
    const auto& g = cells_.genes;

    struct Request {
        std::size_t cell;  // index into cell_sites_
        int site;
        double a;
        double b;
    };
    std::vector<Request> requests;
    std::vector<double> intake_a(cell_sites_.size(), 0.0);
    std::vector<double> intake_b(cell_sites_.size(), 0.0);
    std::vector<double> demand_a(static_cast<std::size_t>(n_sites_), 0.0);
    std::vector<double> demand_b(static_cast<std::size_t>(n_sites_), 0.0);

    // Gather: own site first, then the remaining demand split equally over empty sites.
    for (std::size_t i = 0; i < cell_sites_.size(); ++i) {
        const int site = cell_sites_[i];
        const auto s = static_cast<std::size_t>(site);
        if (!cells_.awake[s]) continue;

        const double diet = g[kDiet][s];
        const double base = params_.feed_rate * g[kHarvest][s] * cells_.thermal_eff[s];
        const double cap_a = base * diet * diet;
        const double cap_b = base * (1.0 - diet) * (1.0 - diet);
        intake_a[i] = std::min(cap_a, cur_.food_a[s]);
        intake_b[i] = std::min(cap_b, cur_.food_b[s]);
        const double rem_a = cap_a - intake_a[i];
        const double rem_b = cap_b - intake_b[i];
        if (rem_a <= 0.0 && rem_b <= 0.0) continue;

        int empties[8];
        int k = 0;
        for (int d = 0; d < 8; ++d) {
            const int n = neighbor(site, d);
            if (!cells_.alive[static_cast<std::size_t>(n)]) empties[k++] = n;
        }
        for (int e = 0; e < k; ++e) {
            const Request r{i, empties[e], rem_a / k, rem_b / k};
            requests.push_back(r);
            demand_a[static_cast<std::size_t>(r.site)] += r.a;
            demand_b[static_cast<std::size_t>(r.site)] += r.b;
        }
    }

    // Commit: scale over-asked sites, take the food, then fill stores.
    for (std::size_t i = 0; i < cell_sites_.size(); ++i) {
        const auto s = static_cast<std::size_t>(cell_sites_[i]);
        cur_.food_a[s] -= intake_a[i];
        cur_.food_b[s] -= intake_b[i];
    }
    for (const Request& r : requests) {
        const auto t = static_cast<std::size_t>(r.site);
        const double fa = demand_a[t] > cur_.food_a[t] ? cur_.food_a[t] / demand_a[t] : 1.0;
        const double fb = demand_b[t] > cur_.food_b[t] ? cur_.food_b[t] / demand_b[t] : 1.0;
        intake_a[r.cell] += r.a * fa;
        intake_b[r.cell] += r.b * fb;
    }
    for (const Request& r : requests) {
        const auto t = static_cast<std::size_t>(r.site);
        if (demand_a[t] > 0.0) {
            cur_.food_a[t] = std::max(0.0, cur_.food_a[t] - std::min(demand_a[t], cur_.food_a[t]));
            demand_a[t] = 0.0;
        }
        if (demand_b[t] > 0.0) {
            cur_.food_b[t] = std::max(0.0, cur_.food_b[t] - std::min(demand_b[t], cur_.food_b[t]));
            demand_b[t] = 0.0;
        }
    }
    for (std::size_t i = 0; i < cell_sites_.size(); ++i) {
        add_to_store(static_cast<std::size_t>(cell_sites_[i]), intake_a[i], intake_b[i]);
    }
}

// ---- 8. Upkeep and death ----

void World::phase_upkeep() {
    collect_cell_sites();
    const auto& g = cells_.genes;
    std::vector<double> cost(cell_sites_.size(), 0.0);

    for (std::size_t i = 0; i < cell_sites_.size(); ++i) {
        const int site = cell_sites_[i];
        const auto s = static_cast<std::size_t>(site);
        int crowd = 0;
        for (int d = 0; d < 8; ++d) {
            if (cells_.alive[static_cast<std::size_t>(neighbor(site, d))]) ++crowd;
        }
        const int excess = std::max(0, crowd - params_.crowding_free);
        const double total = params_.cost_alive +
                             params_.cost_harvest * g[kHarvest][s] +
                             params_.cost_attack * g[kAttack][s] +
                             params_.cost_defense * g[kDefense][s] +
                             params_.cost_crowding * excess +
                             params_.cost_aging * cells_.age[s] +
                             params_.cost_move * cells_.moved[s] +
                             params_.cost_photosynthesis * g[kPhotosynthesis][s] +
                             params_.cost_resistance * g[kResistance][s];
        // Infection cost arrives in M5, the dormancy discount in M3.
        cost[i] = total * (params_.upkeep_temp_base +
                           site_temperature(site) / params_.upkeep_temp_scale);
    }

    for (std::size_t i = 0; i < cell_sites_.size(); ++i) {
        const auto s = static_cast<std::size_t>(cell_sites_[i]);
        const double a = cells_.store_a[s];
        const double b = cells_.store_b[s];
        const double c = cost[i];
        if (a + b < c) {
            kill_cell(s, DeathCause::Starved);
            continue;
        }
        double pay_a, pay_b;
        if (a >= b) {
            pay_a = std::min(c, a);
            pay_b = std::min(b, c - pay_a);
        } else {
            pay_b = std::min(c, b);
            pay_a = std::min(a, c - pay_b);
        }
        cells_.store_a[s] = a - pay_a;
        cells_.store_b[s] = b - pay_b;
        cur_.minerals[s] += pay_a + pay_b;
        ++cells_.age[s];
    }
}

// ---- 9. Divide ----

void World::phase_divide() {
    collect_cell_sites();
    std::vector<Intent> intents;

    for (const int site : cell_sites_) {
        const auto s = static_cast<std::size_t>(site);
        if (cells_.cooldown[s] > 0) {
            --cells_.cooldown[s];
            continue;
        }
        if (!cells_.awake[s] || cells_.age[s] < params_.divide_min_age ||
            cells_.store_a[s] < params_.divide_min_store ||
            cells_.store_b[s] < params_.divide_min_store) {
            continue;
        }
        int empties[8];
        int k = 0;
        for (int d = 0; d < 8; ++d) {
            const int n = neighbor(site, d);
            if (!cells_.alive[static_cast<std::size_t>(n)]) empties[k++] = n;
        }
        if (k == 0) continue;
        intents.push_back({empties[k > 1 ? rng_.below(static_cast<std::uint32_t>(k)) : 0u], site});
    }

    std::vector<Intent> winners = resolve_conflicts(std::move(intents), rng_);
    std::sort(winners.begin(), winners.end(),
              [](const Intent& a, const Intent& b) { return a.source < b.source; });

    // Adhesion and mating arrive in M4; every daughter is a released clone.
    for (const Intent& w : winners) {
        const auto m = static_cast<std::size_t>(w.source);
        const auto d = static_cast<std::size_t>(w.target);
        cells_.store_a[m] -= params_.clone_cost;
        cells_.store_b[m] -= params_.clone_cost;
        cells_.cooldown[m] = params_.divide_cooldown;

        Genome genome = cells_.genome(m);
        const double p = cells_.genes[kMutability][m] * cells_.stress[m];
        mutate(genome, mutation_chance(p, params_), rng_, params_);

        cells_.clear(d);
        cells_.alive[d] = 1;
        cells_.id[d] = next_id_++;
        cells_.parent_id[d] = cells_.id[m];
        cells_.store_a[d] = params_.daughter_store;
        cells_.store_b[d] = params_.daughter_store;
        cells_.set_genome(d, genome);
        ++cell_count_;
        births_.push_back({tick_, cells_.id[d], cells_.id[m], 0, w.target, genome,
                           BirthKind::ReleasedClone});
    }
}

}  // namespace evo
