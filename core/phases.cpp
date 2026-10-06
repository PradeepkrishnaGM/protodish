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

// One cell's share of a pool on one site, scaled later if the site is over-asked.
struct Request {
    std::size_t cell;  // index into the phase's cell list
    int site;
    double amount;
};

// Takes from the own site first, then asks the empty sites equally for the rest.
// Returns the amount taken from the own site. `pool` is the per-site quantity.
double gather_from_reach(std::size_t cell, int site, double capacity, const std::vector<double>& pool,
                         const int* empties, int k, std::vector<Request>& requests,
                         std::vector<double>& demand) {
    const double own = std::min(capacity, pool[static_cast<std::size_t>(site)]);
    const double rest = capacity - own;
    if (rest > 0.0 && k > 0) {
        for (int e = 0; e < k; ++e) {
            requests.push_back({cell, empties[e], rest / k});
            demand[static_cast<std::size_t>(empties[e])] += rest / k;
        }
    }
    return own;
}

// Grants requests (scaled per site when over-asked), adds them to `intake`, and removes
// what was granted from the pool.
void commit_requests(const std::vector<Request>& requests, std::vector<double>& demand,
                     std::vector<double>& pool, std::vector<double>& intake) {
    for (const Request& r : requests) {
        const auto t = static_cast<std::size_t>(r.site);
        const double f = demand[t] > pool[t] ? pool[t] / demand[t] : 1.0;
        intake[r.cell] += r.amount * f;
    }
    for (const Request& r : requests) {
        const auto t = static_cast<std::size_t>(r.site);
        if (demand[t] > 0.0) {
            pool[t] = std::max(0.0, pool[t] - std::min(demand[t], pool[t]));
            demand[t] = 0.0;
        }
    }
}

}  // namespace

// ---- Relations ----

bool World::is_kin(int a, int b) const {
    const auto& g = cells_.genes;
    return tag_distance(g[kTag][static_cast<std::size_t>(a)], g[kTag][static_cast<std::size_t>(b)]) <=
           g[kTolerance][static_cast<std::size_t>(a)];
}

double World::effective_defense(int s) const {
    const auto i = static_cast<std::size_t>(s);
    const double d = cells_.genes[kDefense][i];
    return cells_.awake[i] ? d : d * params_.dormant_defense_multiple;
}

bool World::treats_as_prey(int a, int b) const {
    // Bonded cells are never prey (M4).
    return !is_kin(a, b) && cells_.genes[kAttack][static_cast<std::size_t>(a)] > effective_defense(b);
}

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
        cells_.supply[s] = std::max(food_term, light_term);  // DECISIONS M3-1

        const double dormancy = g[kDormancy][s];
        cells_.awake[s] = !(cells_.thermal_eff[s] < dormancy || cells_.supply[s] < dormancy);

        cells_.drained[s] = 0;
        cells_.gross_intake[s] = 0.0;
        cells_.feed_capacity[s] = 0.0;
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
        int best[8];
        int n_best = 0;
        double best_score = 0.0;
        for (int d = 0; d < 8; ++d) {
            const int cand = neighbor(site, d);
            const auto c = static_cast<std::size_t>(cand);
            if (cells_.alive[c]) continue;

            const double food = std::min(
                1.0, (diet * cur_.food_a[c] + (1.0 - diet) * cur_.food_b[c]) / params_.move_food_norm);
            int kin = 0, prey = 0, threats = 0;
            for (int e = 0; e < 8; ++e) {
                const int other = neighbor(cand, e);
                if (other == site || !cells_.alive[static_cast<std::size_t>(other)]) continue;
                if (is_kin(site, other)) ++kin;
                if (treats_as_prey(site, other)) ++prey;
                if (cells_.awake[static_cast<std::size_t>(other)] && treats_as_prey(other, site)) {
                    ++threats;  // DECISIONS M3-3
                }
            }
            const double score = g[kAppetite][s] * food +
                                 (g[kBoldness][s] * prey - g[kCaution][s] * threats +
                                  g[kSociability][s] * kin) / params_.neighbor_norm;

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

// ---- 4. Feed, photosynthesis and leak ----

void World::phase_feed() {
    collect_cell_sites();
    const auto& g = cells_.genes;
    const std::size_t n_cells = cell_sites_.size();
    const auto n = static_cast<std::size_t>(n_sites_);

    std::vector<Request> req_a, req_b, req_m;
    std::vector<double> got_a(n_cells, 0.0), got_b(n_cells, 0.0), made(n_cells, 0.0);
    std::vector<double> demand_a(n, 0.0), demand_b(n, 0.0), demand_m(n, 0.0);

    // Gather.
    for (std::size_t i = 0; i < n_cells; ++i) {
        const int site = cell_sites_[i];
        const auto s = static_cast<std::size_t>(site);
        if (!cells_.awake[s]) continue;

        const double diet = g[kDiet][s];
        const double eff = cells_.thermal_eff[s];
        const double base = params_.feed_rate * g[kHarvest][s] * eff;
        const double cap_a = base * diet * diet;
        const double cap_b = base * (1.0 - diet) * (1.0 - diet);
        const double cap_m = params_.photo_rate * g[kPhotosynthesis][s] * site_light(site) * eff;
        cells_.feed_capacity[s] = cap_a + cap_b + cap_m;

        int empties[8];
        int k = 0;
        for (int d = 0; d < 8; ++d) {
            const int nb = neighbor(site, d);
            if (!cells_.alive[static_cast<std::size_t>(nb)]) empties[k++] = nb;
        }
        got_a[i] = gather_from_reach(i, site, cap_a, cur_.food_a, empties, k, req_a, demand_a);
        got_b[i] = gather_from_reach(i, site, cap_b, cur_.food_b, empties, k, req_b, demand_b);
        made[i] = gather_from_reach(i, site, cap_m, cur_.minerals, empties, k, req_m, demand_m);
    }

    // Commit: own sites (asked only by their occupant), then the empty sites in reach.
    for (std::size_t i = 0; i < n_cells; ++i) {
        const auto s = static_cast<std::size_t>(cell_sites_[i]);
        cur_.food_a[s] -= got_a[i];
        cur_.food_b[s] -= got_b[i];
        cur_.minerals[s] -= made[i];
    }
    commit_requests(req_a, demand_a, cur_.food_a, got_a);
    commit_requests(req_b, demand_b, cur_.food_b, got_b);
    commit_requests(req_m, demand_m, cur_.minerals, made);

    // Leak: keep (1 − leak) of gross intake, pass the rest evenly to occupied neighbors.
    std::vector<double> add_a(n_cells, 0.0), add_b(n_cells, 0.0);
    std::vector<std::size_t> index_of(n, 0);
    for (std::size_t i = 0; i < n_cells; ++i) index_of[static_cast<std::size_t>(cell_sites_[i])] = i;

    for (std::size_t i = 0; i < n_cells; ++i) {
        const int site = cell_sites_[i];
        const auto s = static_cast<std::size_t>(site);
        const double diet = g[kDiet][s];
        const double gross_a = got_a[i] + made[i] * diet;
        const double gross_b = got_b[i] + made[i] * (1.0 - diet);
        cells_.gross_intake[s] = gross_a + gross_b;

        int occupied[8];
        int k = 0;
        for (int d = 0; d < 8; ++d) {
            const int nb = neighbor(site, d);
            if (cells_.alive[static_cast<std::size_t>(nb)]) occupied[k++] = nb;
        }
        if (k == 0) {
            add_a[i] += gross_a;
            add_b[i] += gross_b;
            continue;
        }
        const double leak_a = gross_a * params_.leak_fraction;
        const double leak_b = gross_b * params_.leak_fraction;
        add_a[i] += gross_a - leak_a;
        add_b[i] += gross_b - leak_b;
        for (int e = 0; e < k; ++e) {
            const std::size_t j = index_of[static_cast<std::size_t>(occupied[e])];
            add_a[j] += leak_a / k;
            add_b[j] += leak_b / k;
        }
    }
    for (std::size_t i = 0; i < n_cells; ++i) {
        add_to_store(static_cast<std::size_t>(cell_sites_[i]), add_a[i], add_b[i]);
    }
}

// ---- 5. Attack ----

void World::phase_attack() {
    collect_cell_sites();
    const auto& g = cells_.genes;
    const double max = params_.store_max;

    struct Drain {
        int attacker;
        int victim;
        double amount;
    };
    std::vector<Drain> drains;
    std::vector<double> demand(static_cast<std::size_t>(n_sites_), 0.0);

    // Gather: each awake attacker's drains, capped by satiation.
    for (const int site : cell_sites_) {
        const auto s = static_cast<std::size_t>(site);
        if (!cells_.awake[s]) continue;
        const double room = (max - cells_.store_a[s]) + (max - cells_.store_b[s]);
        if (room <= 0.0) continue;  // full stores: no attack

        const std::size_t first = drains.size();
        double wanted = 0.0;
        for (int d = 0; d < 8; ++d) {
            const int v = neighbor(site, d);
            if (!cells_.alive[static_cast<std::size_t>(v)] || !treats_as_prey(site, v)) continue;
            const double want = params_.drain_factor * (g[kAttack][s] - effective_defense(v)) *
                                cells_.thermal_eff[s];
            if (want <= 0.0) continue;
            drains.push_back({site, v, want});
            wanted += want;
        }
        const double cap = params_.satiation_multiple * room;
        const double scale = wanted > cap ? cap / wanted : 1.0;
        for (std::size_t i = first; i < drains.size(); ++i) {
            drains[i].amount *= scale;
            demand[static_cast<std::size_t>(drains[i].victim)] += drains[i].amount;
        }
    }
    if (drains.empty()) return;

    // Victim scaling, using the victims' stores at the start of the phase.
    struct Taken {
        double a;
        double b;
    };
    std::vector<Taken> taken(drains.size());
    for (std::size_t i = 0; i < drains.size(); ++i) {
        const auto v = static_cast<std::size_t>(drains[i].victim);
        const double holds = cells_.store_a[v] + cells_.store_b[v];
        if (holds <= 0.0) {
            taken[i] = {0.0, 0.0};
            continue;
        }
        const double f = demand[v] > holds ? holds / demand[v] : 1.0;
        const double amount = drains[i].amount * f;
        taken[i] = {amount * cells_.store_a[v] / holds, amount * cells_.store_b[v] / holds};
    }

    // Commit: subtract from every victim first, then add gains and scraps.
    for (std::size_t i = 0; i < drains.size(); ++i) {
        const auto v = static_cast<std::size_t>(drains[i].victim);
        if (taken[i].a + taken[i].b <= 0.0) continue;
        cells_.store_a[v] = std::max(0.0, cells_.store_a[v] - taken[i].a);
        cells_.store_b[v] = std::max(0.0, cells_.store_b[v] - taken[i].b);
        cells_.drained[v] = 1;
    }
    for (std::size_t i = 0; i < drains.size(); ++i) {
        const auto a = static_cast<std::size_t>(drains[i].attacker);
        const auto v = static_cast<std::size_t>(drains[i].victim);
        const double keep_a = taken[i].a * params_.attacker_keep;
        const double keep_b = taken[i].b * params_.attacker_keep;
        add_to_store(a, keep_a, keep_b);
        cur_.food_a[v] += taken[i].a - keep_a;  // scraps fall on the victim's site
        cur_.food_b[v] += taken[i].b - keep_b;
    }
}

// ---- 8. Upkeep, stress and death ----

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
        // Infection cost arrives in M5.
        cost[i] = total * (params_.upkeep_temp_base +
                           site_temperature(site) / params_.upkeep_temp_scale) *
                  (cells_.awake[s] ? 1.0 : params_.dormant_upkeep_factor);
    }

    for (std::size_t i = 0; i < cell_sites_.size(); ++i) {
        const auto s = static_cast<std::size_t>(cell_sites_[i]);
        const double a = cells_.store_a[s];
        const double b = cells_.store_b[s];
        const double c = cost[i];
        if (a + b < c) {
            kill_cell(s, cells_.drained[s] ? DeathCause::Drained : DeathCause::Starved);
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

        if (!cells_.awake[s]) continue;  // dormant: no aging, stress frozen (DECISIONS M3-7)
        ++cells_.age[s];
        double stress = cells_.stress[s] * params_.stress_fade;
        if (cells_.drained[s]) stress += params_.stress_drained;
        // Infection stress arrives in M5.
        if (cells_.feed_capacity[s] > 0.0 &&
            cells_.gross_intake[s] < params_.poor_intake * cells_.feed_capacity[s]) {
            stress += params_.stress_poor;
        }
        cells_.stress[s] = std::min(kStressMax, stress);
    }
}

// ---- 9. Divide ----

void World::phase_divide() {
    collect_cell_sites();
    std::vector<Intent> intents;

    for (const int site : cell_sites_) {
        const auto s = static_cast<std::size_t>(site);
        if (!cells_.awake[s]) continue;  // dormant: cooldown frozen (DECISIONS M3-7)
        if (cells_.cooldown[s] > 0) {
            --cells_.cooldown[s];
            continue;
        }
        if (cells_.age[s] < params_.divide_min_age ||
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
