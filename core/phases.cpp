// Cell phases 2-9. Each phase gathers its decisions from the current state, then commits
// them, so no cell sees another's action from the same phase (DECISIONS.md, M2).

#include <algorithm>
#include <array>
#include <cmath>

#include "mutation.hpp"
#include "world.hpp"

namespace evo {

namespace {

// Sorts intents by target (then source) and keeps one random winner per target.
// One draw per contested target, in ascending target order.
// `intents` is sorted in place; the result is written to `winners` and returned.
const std::vector<Intent>& resolve_conflicts(std::vector<Intent>& intents, std::vector<Intent>& winners,
                                             Rng& rng) {
    std::sort(intents.begin(), intents.end(), [](const Intent& a, const Intent& b) {
        return a.target != b.target ? a.target < b.target : a.source < b.source;
    });
    winners.clear();
    for (std::size_t i = 0; i < intents.size();) {
        std::size_t j = i;
        while (j < intents.size() && intents[j].target == intents[i].target) ++j;
        const auto n = static_cast<std::uint32_t>(j - i);
        winners.push_back(intents[i + (n > 1 ? rng.below(n) : 0u)]);
        i = j;
    }
    return winners;
}

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

// Direction with column offset dx and row offset dy (each -1, 0 or 1), or -1 for (0, 0).
constexpr int direction_of(int dx, int dy) {
    for (int d = 0; d < 8; ++d) {
        if (kDirDx[d] == dx && kDirDy[d] == dy) return d;
    }
    return -1;
}

constexpr int clamp1(int v) { return v < -1 ? -1 : (v > 1 ? 1 : v); }

// How to reach each site of the 5 x 5 window around a cell (index (dy + 2) * 5 + dx + 2)
// with at most two neighbor steps: first, then second; -1 means no step.
struct WindowPath {
    int first;
    int second;
};

constexpr std::array<WindowPath, 25> make_window_paths() {
    std::array<WindowPath, 25> paths{};
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            const int ax = clamp1(dx), ay = clamp1(dy);
            const int first = direction_of(ax, ay);
            const int second = direction_of(dx - ax, dy - ay);
            paths[static_cast<std::size_t>((dy + 2) * 5 + dx + 2)] =
                first < 0 ? WindowPath{second, -1} : WindowPath{first, second};
        }
    }
    return paths;
}

constexpr std::array<WindowPath, 25> kWindowPath = make_window_paths();

}  // namespace

// ---- Relations ----

// is_kin, effective_defense and treats_as_prey are inline in world.hpp.

// ---- 2. Sense ----

void World::phase_sense() {
    collect_cell_sites();
    const auto& g = cells_.genes;
    for (const int site : cell_sites_) {
        const auto s = static_cast<std::size_t>(site);

        // Role split: outer cells × (1 − r) harvest, × (1 + r) attack and defense; inner the reverse.
        const double r = g[kRoleSplit][s];
        const double sign = is_inner(site) ? -1.0 : 1.0;
        cells_.eff_harvest[s] = g[kHarvest][s] * (1.0 - sign * r);
        cells_.eff_attack[s] = g[kAttack][s] * (1.0 + sign * r);
        cells_.eff_defense[s] = g[kDefense][s] * (1.0 + sign * r);

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
        cells_.photo_intake[s] = 0.0;
        cells_.feed_capacity[s] = 0.0;
    }
}

// ---- 3. Move ----

void World::phase_move() {
    const auto& g = cells_.genes;
    std::vector<Intent>& intents = intents_;
    intents.clear();

    for (const int site : cell_sites_) {
        const auto s = static_cast<std::size_t>(site);
        cells_.moved[s] = 0;
        if (!cells_.awake[s] || cells_.bonds[s]) continue;  // dormant or anchored in a body
        if (!rng_.chance(g[kMotility][s])) continue;

        // The neighbors of all 8 candidate sites lie in the 5 x 5 window around the cell.
        // Each window site's relations to the mover are worked out once, on first use.
        std::uint32_t known = 0;
        std::uint8_t kin_at[25], prey_at[25], threat_at[25];
        auto relations = [&](int ox, int oy) {
            const int wi = (oy + 2) * 5 + (ox + 2);
            if (!(known & (1u << wi))) {
                known |= 1u << wi;
                const WindowPath& path = kWindowPath[static_cast<std::size_t>(wi)];
                const int other = path.first < 0    ? site
                                  : path.second < 0 ? neighbor(site, path.first)
                                                    : neighbor(neighbor(site, path.first), path.second);
                const bool occupied = other != site && cells_.alive[static_cast<std::size_t>(other)];
                kin_at[wi] = occupied && is_kin(site, other);
                prey_at[wi] = occupied && treats_as_prey(site, other);
                threat_at[wi] = occupied && cells_.awake[static_cast<std::size_t>(other)] &&
                                treats_as_prey(other, site);  // DECISIONS M3-3
            }
            return wi;
        };

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
                const int wi = relations(kDirDx[d] + kDirDx[e], kDirDy[d] + kDirDy[e]);
                kin += kin_at[wi];
                prey += prey_at[wi];
                threats += threat_at[wi];
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

    for (const Intent& m : resolve_conflicts(intents, winners_, rng_)) {
        cells_.move(static_cast<std::size_t>(m.source), static_cast<std::size_t>(m.target));
        cells_.moved[static_cast<std::size_t>(m.target)] = 1;
    }
}

// ---- 4. Feed, photosynthesis and leak ----

void World::phase_feed() {
    collect_cell_sites();
    const auto& g = cells_.genes;
    const std::size_t n_cells = cell_sites_.size();

    Scratch& sc = scratch_;
    auto& req_a = sc.req_a;
    auto& req_b = sc.req_b;
    auto& req_m = sc.req_m;
    req_a.clear();
    req_b.clear();
    req_m.clear();
    auto& got_a = sc.got_a;
    auto& got_b = sc.got_b;
    auto& made = sc.made;
    got_a.assign(n_cells, 0.0);
    got_b.assign(n_cells, 0.0);
    made.assign(n_cells, 0.0);
    auto& demand_a = sc.demand_a;  // all zero on entry; commit_requests resets what it uses
    auto& demand_b = sc.demand_b;
    auto& demand_m = sc.demand_m;

    // Gather.
    for (std::size_t i = 0; i < n_cells; ++i) {
        const int site = cell_sites_[i];
        const auto s = static_cast<std::size_t>(site);
        if (!cells_.awake[s]) continue;

        const double diet = g[kDiet][s];
        const double eff = cells_.thermal_eff[s];
        const double base = params_.feed_rate * cells_.eff_harvest[s] * eff;
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
    auto& add_a = sc.add_a;
    auto& add_b = sc.add_b;
    add_a.assign(n_cells, 0.0);
    add_b.assign(n_cells, 0.0);
    auto& index_of = sc.index_of;  // read only at occupied sites, which are all written here
    for (std::size_t i = 0; i < n_cells; ++i) index_of[static_cast<std::size_t>(cell_sites_[i])] = i;

    for (std::size_t i = 0; i < n_cells; ++i) {
        const int site = cell_sites_[i];
        const auto s = static_cast<std::size_t>(site);
        const double diet = g[kDiet][s];
        const double gross_a = got_a[i] + made[i] * diet;
        const double gross_b = got_b[i] + made[i] * (1.0 - diet);
        cells_.gross_intake[s] = gross_a + gross_b;
        cells_.photo_intake[s] = made[i];

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
    const double max = params_.store_max;

    std::vector<Drain>& drains = drains_;
    drains.clear();
    std::vector<double>& demand = scratch_.drain_demand;  // all zero on entry and exit

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
            const double want = params_.drain_factor * (cells_.eff_attack[s] - effective_defense(v)) *
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
    taken_.resize(drains.size());

    // Victim scaling, using the victims' stores at the start of the phase.
    std::vector<Taken>& taken = taken_;
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
    for (const Drain& dr : drains) demand[static_cast<std::size_t>(dr.victim)] = 0.0;
}

// ---- 8. Upkeep, stress and death ----

void World::phase_upkeep() {
    const auto& g = cells_.genes;
    std::vector<double>& cost = scratch_.cost;
    cost.assign(cell_sites_.size(), 0.0);

    for (std::size_t i = 0; i < cell_sites_.size(); ++i) {
        const int site = cell_sites_[i];
        const auto s = static_cast<std::size_t>(site);
        int crowd = 0;
        for (int d = 0; d < 8; ++d) {
            if (cells_.alive[static_cast<std::size_t>(neighbor(site, d))]) ++crowd;
        }
        const int excess = std::max(0, crowd - params_.crowding_free);
        const double total = params_.cost_alive +
                             params_.cost_harvest * cells_.eff_harvest[s] +
                             params_.cost_attack * cells_.eff_attack[s] +
                             params_.cost_defense * cells_.eff_defense[s] +
                             params_.cost_crowding * excess +
                             params_.cost_aging * cells_.age[s] +
                             params_.cost_move * cells_.moved[s] +
                             params_.cost_photosynthesis * g[kPhotosynthesis][s] +
                             params_.cost_resistance * g[kResistance][s] +
                             params_.cost_infection * cells_.infected[s];
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
        if (cells_.infected[s]) stress += params_.stress_infected;
        if (cells_.feed_capacity[s] > 0.0 &&
            cells_.gross_intake[s] < params_.poor_intake * cells_.feed_capacity[s]) {
            stress += params_.stress_poor;
        }
        cells_.stress[s] = std::min(kStressMax, stress);
    }
}

// ---- 6. Share ----

void World::phase_share() {
    const std::size_t n_cells = cell_sites_.size();
    auto& give_a = scratch_.give_a;
    auto& give_b = scratch_.give_b;
    give_a.assign(n_cells, 0.0);
    give_b.assign(n_cells, 0.0);
    auto& get_a = scratch_.get_a;  // all zero on entry and exit
    auto& get_b = scratch_.get_b;

    // Gather from the stores at the start of the phase.
    for (std::size_t i = 0; i < n_cells; ++i) {
        const int site = cell_sites_[i];
        const auto s = static_cast<std::size_t>(site);
        const unsigned mask = cells_.bonds[s];
        if (!cells_.awake[s] || mask == 0) continue;
        const double share = cells_.genes[kShare][s];
        const double t = params_.share_threshold;
        give_a[i] = cells_.store_a[s] > t ? share * (cells_.store_a[s] - t) : 0.0;
        give_b[i] = cells_.store_b[s] > t ? share * (cells_.store_b[s] - t) : 0.0;
        if (give_a[i] <= 0.0 && give_b[i] <= 0.0) continue;
        int k = 0;
        for (int d = 0; d < 8; ++d) k += (mask >> d) & 1u;
        for (int d = 0; d < 8; ++d) {
            if (!(mask & (1u << d))) continue;
            const auto t_site = static_cast<std::size_t>(neighbor(site, d));
            get_a[t_site] += give_a[i] / k;
            get_b[t_site] += give_b[i] / k;
        }
    }

    // Commit: every gift leaves, then every receipt arrives (with overflow).
    for (std::size_t i = 0; i < n_cells; ++i) {
        const auto s = static_cast<std::size_t>(cell_sites_[i]);
        cells_.store_a[s] -= give_a[i];
        cells_.store_b[s] -= give_b[i];
    }
    for (std::size_t i = 0; i < n_cells; ++i) {
        const auto s = static_cast<std::size_t>(cell_sites_[i]);
        if (get_a[s] > 0.0 || get_b[s] > 0.0) add_to_store(s, get_a[s], get_b[s]);
        get_a[s] = 0.0;
        get_b[s] = 0.0;
    }
}

// ---- 7. Infect ----

void World::phase_infect() {
    const auto& tag = cells_.genes[kTag];
    const auto& resistance = cells_.genes[kResistance];

    // Gather: every rule reads the infection state at the start of the phase, and dormant
    // cells neither catch, pass nor clear a virus (DECISIONS M5-1, M5-2).
    std::vector<Intent>& intents = intents_;
    intents.clear();
    std::vector<int>& carriers = scratch_.carriers;  // infected and awake at the start of the phase
    carriers.clear();
    for (const int site : cell_sites_) {
        const auto s = static_cast<std::size_t>(site);
        if (!cells_.infected[s] || !cells_.awake[s]) continue;
        carriers.push_back(site);
        for (int d = 0; d < 8; ++d) {
            const int nb = neighbor(site, d);
            const auto n = static_cast<std::size_t>(nb);
            if (!cells_.alive[n] || cells_.infected[n] || !cells_.awake[n]) continue;
            if (tag_distance(cells_.virus_tag[s], tag[n]) > params_.virus_match) continue;
            if (rng_.chance(params_.spread_chance * (1.0 - resistance[n]))) intents.push_back({nb, site});
        }
    }

    // A target reached by several sources catches one of them, picked at random (M5-3).
    // Drift applies to the copy that passes (M5-4).
    const std::vector<Intent>& winners = resolve_conflicts(intents, winners_, rng_);
    for (const Intent& w : winners) {
        const auto t = static_cast<std::size_t>(w.target);
        double v = cells_.virus_tag[static_cast<std::size_t>(w.source)];
        if (rng_.chance(params_.drift_chance)) {
            v = wrap_tag(v + (2.0 * rng_.uniform() - 1.0) * params_.drift_step);
        }
        cells_.infected[t] = 1;
        cells_.virus_tag[t] = v;
    }

    for (const int site : carriers) {
        const auto s = static_cast<std::size_t>(site);
        if (rng_.chance(params_.recovery_chance * resistance[s])) {
            cells_.infected[s] = 0;
            cells_.virus_tag[s] = 0.0;
        }
    }

    // Outbreaks: healthy awake cells that did not catch a virus this tick. Cells that just
    // recovered were infected at the start of the phase, so they are not candidates.
    std::size_t wi = 0;
    std::size_t ci = 0;
    for (const int site : cell_sites_) {
        const auto s = static_cast<std::size_t>(site);
        while (wi < winners.size() && winners[wi].target < site) ++wi;
        while (ci < carriers.size() && carriers[ci] < site) ++ci;
        const bool caught = wi < winners.size() && winners[wi].target == site;
        const bool carrier = ci < carriers.size() && carriers[ci] == site;
        if (caught || carrier || cells_.infected[s] || !cells_.awake[s]) continue;
        if (rng_.chance(params_.outbreak_chance)) {
            cells_.infected[s] = 1;
            cells_.virus_tag[s] = tag[s];
        }
    }
}

// ---- 9. Divide ----

void World::phase_divide() {
    collect_cell_sites();
    std::vector<Intent>& intents = intents_;
    intents.clear();
    auto& ready = scratch_.ready;  // the three flag arrays are all zero on entry and exit
    auto& used = scratch_.used;
    auto& newborn = scratch_.newborn;

    // Gather: readiness (from the state at the start of the phase) and target sites.
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
            const int nb = neighbor(site, d);
            if (!cells_.alive[static_cast<std::size_t>(nb)]) empties[k++] = nb;
        }
        if (k == 0) continue;
        ready[s] = 1;
        intents.push_back({empties[k > 1 ? rng_.below(static_cast<std::uint32_t>(k)) : 0u], site});
    }

    std::vector<Intent>& winners = winners_;
    resolve_conflicts(intents, winners, rng_);
    std::sort(winners.begin(), winners.end(),
              [](const Intent& a, const Intent& b) { return a.source < b.source; });

    // A cell takes part in at most one birth per tick (DECISIONS M4-4).
    for (const Intent& w : winners) used[static_cast<std::size_t>(w.source)] = 1;

    const auto& g = cells_.genes;
    for (const Intent& w : winners) {
        const int mother = w.source;
        const int target = w.target;
        const auto m = static_cast<std::size_t>(mother);
        const auto d = static_cast<std::size_t>(target);

        const bool attached = rng_.chance(g[kAdhesion][m]);
        int partner = -1;
        if (!attached && rng_.chance(g[kMating][m])) {
            int candidates[8];
            int k = 0;
            for (int e = 0; e < 8; ++e) {
                const int c = neighbor(target, e);
                const auto ci = static_cast<std::size_t>(c);
                if (c == mother || !cells_.alive[ci] || newborn[ci] || !ready[ci] || used[ci]) continue;
                if (bonded(mother, c) || !is_kin(mother, c) || !is_kin(c, mother)) continue;
                candidates[k++] = c;
            }
            if (k > 0) partner = candidates[k > 1 ? rng_.below(static_cast<std::uint32_t>(k)) : 0u];
        }

        Genome genome = cells_.genome(m);
        double p = g[kMutability][m] * cells_.stress[m];
        if (partner >= 0) {
            const auto q = static_cast<std::size_t>(partner);
            used[q] = 1;
            for (std::size_t i = 0; i < kGeneCount; ++i) {
                if (rng_.below(2) == 1) genome[i] = g[i][q];
            }
            p = (p + g[kMutability][q] * cells_.stress[q]) / 2.0;
            for (const auto parent : {m, q}) {
                cells_.store_a[parent] -= params_.mating_cost;
                cells_.store_b[parent] -= params_.mating_cost;
                cells_.cooldown[parent] = params_.divide_cooldown;
            }
        } else {
            cells_.store_a[m] -= params_.clone_cost;
            cells_.store_b[m] -= params_.clone_cost;
            cells_.cooldown[m] = params_.divide_cooldown;
        }
        mutate(genome, mutation_chance(p, params_), rng_, params_);

        cells_.clear(d);
        cells_.alive[d] = 1;
        cells_.id[d] = next_id_++;
        cells_.parent_id[d] = cells_.id[m];
        cells_.parent2_id[d] = partner >= 0 ? cells_.id[static_cast<std::size_t>(partner)] : 0;
        cells_.store_a[d] = params_.daughter_store;
        cells_.store_b[d] = params_.daughter_store;
        cells_.set_genome(d, genome);
        ++cell_count_;
        newborn[d] = 1;

        if (attached) {
            // Bond to the mother and to her pre-existing bonded neighbors next to the daughter.
            for (int e = 0; e < 8; ++e) {
                const int c = neighbor(target, e);
                const auto ci = static_cast<std::size_t>(c);
                if (c != mother && cells_.alive[ci] && !newborn[ci] && bonded(c, mother)) link(target, c);
            }
            link(target, mother);
        }

        const BirthKind kind = attached        ? BirthKind::AttachedClone
                               : partner >= 0  ? BirthKind::Mating
                                               : BirthKind::ReleasedClone;
        births_.push_back({tick_, cells_.id[d], cells_.id[m], cells_.parent2_id[d], target, genome,
                           kind});
    }

    for (const Intent& i : intents) ready[static_cast<std::size_t>(i.source)] = 0;
    for (const Intent& w : winners) {
        used[static_cast<std::size_t>(w.source)] = 0;
        newborn[static_cast<std::size_t>(w.target)] = 0;
    }
    for (const BirthEvent& b : births_) {
        if (b.parent2_id != 0) {
            // A partner is flagged used; it was a ready cell next to the daughter's site.
            for (int e = 0; e < 8; ++e) used[static_cast<std::size_t>(neighbor(b.site, e))] = 0;
        }
    }
}

}  // namespace evo
