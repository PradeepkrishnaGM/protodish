#include "world.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace evo {

namespace {

// Neumaier compensated sum.
template <typename Range>
void neumaier_add(double& sum, double& comp, const Range& values) {
    for (const double v : values) {
        const double t = sum + v;
        if (std::fabs(sum) >= std::fabs(v)) {
            comp += (sum - t) + v;
        } else {
            comp += (v - t) + sum;
        }
        sum = t;
    }
}

double accurate_sum(std::span<const double> values) {
    double sum = 0.0, comp = 0.0;
    neumaier_add(sum, comp, values);
    return sum + comp;
}

class Fnv1a {
public:
    void bytes(const void* data, std::size_t n) {
        const auto* p = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < n; ++i) {
            h_ ^= p[i];
            h_ *= 0x100000001b3ULL;
        }
    }
    template <typename T>
    void value(const T& v) { bytes(&v, sizeof(T)); }
    template <typename T>
    void array(const std::vector<T>& v) { bytes(v.data(), v.size() * sizeof(T)); }
    std::uint64_t digest() const { return h_; }

private:
    std::uint64_t h_ = 0xcbf29ce484222325ULL;
};

}  // namespace

// ---- CellArrays ----

void CellArrays::resize(std::size_t n) {
    alive.assign(n, 0);
    id.assign(n, 0);
    parent_id.assign(n, 0);
    parent2_id.assign(n, 0);
    bonds.assign(n, 0);
    store_a.assign(n, 0.0);
    store_b.assign(n, 0.0);
    age.assign(n, 0);
    cooldown.assign(n, 0);
    stress.assign(n, 0.0);
    moved.assign(n, 0);
    infected.assign(n, 0);
    virus_tag.assign(n, 0.0);
    for (auto& g : genes) g.assign(n, 0.0);
    awake.assign(n, 0);
    thermal_eff.assign(n, 0.0);
    supply.assign(n, 0.0);
    eff_harvest.assign(n, 0.0);
    eff_attack.assign(n, 0.0);
    eff_defense.assign(n, 0.0);
    drained.assign(n, 0);
    gross_intake.assign(n, 0.0);
    feed_capacity.assign(n, 0.0);
}

void CellArrays::move(std::size_t from, std::size_t to) {
    alive[to] = alive[from];
    id[to] = id[from];
    parent_id[to] = parent_id[from];
    parent2_id[to] = parent2_id[from];
    bonds[to] = bonds[from];  // only free cells move, so this is 0
    store_a[to] = store_a[from];
    store_b[to] = store_b[from];
    age[to] = age[from];
    cooldown[to] = cooldown[from];
    stress[to] = stress[from];
    moved[to] = moved[from];
    infected[to] = infected[from];
    virus_tag[to] = virus_tag[from];
    for (auto& g : genes) g[to] = g[from];
    awake[to] = awake[from];
    thermal_eff[to] = thermal_eff[from];
    supply[to] = supply[from];
    eff_harvest[to] = eff_harvest[from];
    eff_attack[to] = eff_attack[from];
    eff_defense[to] = eff_defense[from];
    drained[to] = drained[from];
    gross_intake[to] = gross_intake[from];
    feed_capacity[to] = feed_capacity[from];
    clear(from);
}

void CellArrays::clear(std::size_t s) {
    alive[s] = 0;
    id[s] = 0;
    parent_id[s] = 0;
    parent2_id[s] = 0;
    bonds[s] = 0;
    store_a[s] = 0.0;
    store_b[s] = 0.0;
    age[s] = 0;
    cooldown[s] = 0;
    stress[s] = 0.0;
    moved[s] = 0;
    infected[s] = 0;
    virus_tag[s] = 0.0;
    for (auto& g : genes) g[s] = 0.0;
    awake[s] = 0;
    thermal_eff[s] = 0.0;
    supply[s] = 0.0;
    eff_harvest[s] = 0.0;
    eff_attack[s] = 0.0;
    eff_defense[s] = 0.0;
    drained[s] = 0;
    gross_intake[s] = 0.0;
    feed_capacity[s] = 0.0;
}

Genome CellArrays::genome(std::size_t s) const {
    Genome g{};
    for (std::size_t i = 0; i < kGeneCount; ++i) g[i] = genes[i][s];
    return g;
}

void CellArrays::set_genome(std::size_t s, const Genome& g) {
    for (std::size_t i = 0; i < kGeneCount; ++i) genes[i][s] = g[i];
}

// ---- World ----

World::World(const Params& params, std::uint64_t seed)
    : params_(params),
      climate_(params_),
      rng_(seed),
      seed_(seed),
      n_sites_(params.grid_width * params.grid_height) {
    if (const std::string err = validate_params(params_); !err.empty()) {
        throw std::invalid_argument("invalid params: " + err);
    }
    const auto n = static_cast<std::size_t>(n_sites_);
    cur_.resize(n);
    next_.resize(n);
    cells_.resize(n);
    cell_sites_.reserve(n + 1);
    for (auto* v : {&scratch_.demand_a, &scratch_.demand_b, &scratch_.demand_m,
                    &scratch_.drain_demand, &scratch_.get_a, &scratch_.get_b}) {
        v->assign(n, 0.0);
    }
    scratch_.index_of.assign(n, 0);
    for (auto* v : {&scratch_.ready, &scratch_.used, &scratch_.newborn}) v->assign(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        cur_.food_a[i] = params_.initial_food_a;
        cur_.food_b[i] = params_.initial_food_b;
        cur_.minerals[i] = params_.initial_minerals;
    }

    const int w = params_.grid_width;
    const int h = params_.grid_height;
    neighbors_.resize(n * 8);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            for (int d = 0; d < 8; ++d) {
                const int nx = (x + kDirDx[d] + w) % w;
                const int ny = (y + kDirDy[d] + h) % h;
                neighbors_[static_cast<std::size_t>((y * w + x) * 8 + d)] = ny * w + nx;
            }
        }
    }

    row_temperature_.resize(static_cast<std::size_t>(h));
    row_light_.resize(static_cast<std::size_t>(h));
    prepare_climate();

    // The ancestors, at distinct seeded sites.
    for (int i = 0; i < params_.initial_cells && cell_count_ < n_sites_; ++i) {
        int site = 0;
        do {
            site = static_cast<int>(rng_.below(static_cast<std::uint32_t>(n_sites_)));
        } while (cells_.alive[static_cast<std::size_t>(site)]);
        add_cell(site, params_.ancestor, params_.initial_store_a, params_.initial_store_b,
                 params_.initial_age);
    }
}

bool World::add_cell(int site, const Genome& g, double store_a, double store_b,
                     std::uint32_t age) {
    const auto s = static_cast<std::size_t>(site);
    if (cells_.alive[s]) return false;
    cells_.clear(s);
    cells_.alive[s] = 1;
    cells_.id[s] = next_id_++;
    cells_.store_a[s] = store_a;
    cells_.store_b[s] = store_b;
    cells_.age[s] = age;
    cells_.set_genome(s, g);
    ++cell_count_;
    extinct_at_.reset();
    return true;
}

void World::set_site(int site, double food_a, double food_b, double minerals) {
    const auto s = static_cast<std::size_t>(site);
    cur_.food_a[s] = food_a;
    cur_.food_b[s] = food_b;
    cur_.minerals[s] = minerals;
}

void World::set_cell_stores(int site, double store_a, double store_b) {
    cells_.store_a[static_cast<std::size_t>(site)] = store_a;
    cells_.store_b[static_cast<std::size_t>(site)] = store_b;
}

void World::set_cell_cooldown(int site, std::uint32_t cooldown) {
    cells_.cooldown[static_cast<std::size_t>(site)] = cooldown;
}

void World::set_cell_stress(int site, double stress) {
    cells_.stress[static_cast<std::size_t>(site)] = stress;
}

void World::set_cell_virus(int site, double tag) {
    const auto s = static_cast<std::size_t>(site);
    cells_.infected[s] = tag >= 0.0 ? 1 : 0;
    cells_.virus_tag[s] = tag >= 0.0 ? wrap_tag(tag) : 0.0;
}

void World::link(int a, int b) {
    const int d = direction(a, b);
    cells_.bonds[static_cast<std::size_t>(a)] |= static_cast<std::uint8_t>(1u << d);
    cells_.bonds[static_cast<std::size_t>(b)] |= static_cast<std::uint8_t>(1u << ((d + 4) % 8));
}

bool World::add_bond(int a, int b) {
    if (!cells_.alive[static_cast<std::size_t>(a)] || !cells_.alive[static_cast<std::size_t>(b)] ||
        a == b || direction(a, b) < 0) {
        return false;
    }
    link(a, b);
    return true;
}

std::vector<int> World::body_labels() const {
    std::vector<int> label(static_cast<std::size_t>(n_sites_), -1);
    std::vector<int> stack;
    int next = 0;
    for (int s = 0; s < n_sites_; ++s) {
        if (!cells_.alive[static_cast<std::size_t>(s)] || label[static_cast<std::size_t>(s)] >= 0) continue;
        label[static_cast<std::size_t>(s)] = next;
        stack.push_back(s);
        while (!stack.empty()) {
            const int c = stack.back();
            stack.pop_back();
            for (int d = 0; d < 8; ++d) {
                if (!(cells_.bonds[static_cast<std::size_t>(c)] & (1u << d))) continue;
                const int t = neighbor(c, d);
                if (label[static_cast<std::size_t>(t)] < 0) {
                    label[static_cast<std::size_t>(t)] = next;
                    stack.push_back(t);
                }
            }
        }
        ++next;
    }
    return label;
}

void World::prepare_climate() {
    season_ = climate_.season(tick_);
    for (int row = 0; row < params_.grid_height; ++row) {
        row_temperature_[static_cast<std::size_t>(row)] = climate_.temperature(season_, row);
        row_light_[static_cast<std::size_t>(row)] = climate_.light(season_, row);
    }
}

void World::collect_cell_sites() {
    // Branch-free scan: write every site, advance only past occupied ones.
    cell_sites_.resize(static_cast<std::size_t>(n_sites_) + 1);
    std::size_t k = 0;
    for (int s = 0; s < n_sites_; ++s) {
        cell_sites_[k] = s;
        k += cells_.alive[static_cast<std::size_t>(s)];
    }
    cell_sites_.resize(k);
}

void World::kill_cell(std::size_t s, DeathCause cause) {
    // Break every bond. Pieces of a split body are simply separate bond groups now.
    for (int d = 0; d < 8; ++d) {
        if (cells_.bonds[s] & (1u << d)) {
            const auto t = static_cast<std::size_t>(neighbor(static_cast<int>(s), d));
            cells_.bonds[t] = static_cast<std::uint8_t>(cells_.bonds[t] & ~(1u << ((d + 4) % 8)));
        }
    }
    cur_.food_a[s] += cells_.store_a[s] + params_.body_mass_a;
    cur_.food_b[s] += cells_.store_b[s] + params_.body_mass_b;
    deaths_.push_back({tick_, cells_.id[s], cells_.age[s], static_cast<int>(s), cause});
    cells_.clear(s);
    --cell_count_;
}

void World::add_to_store(std::size_t s, double a, double b) {
    double na = cells_.store_a[s] + a;
    double nb = cells_.store_b[s] + b;
    if (na > params_.store_max) {
        cur_.food_a[s] += na - params_.store_max;
        na = params_.store_max;
    }
    if (nb > params_.store_max) {
        cur_.food_b[s] += nb - params_.store_max;
        nb = params_.store_max;
    }
    cells_.store_a[s] = na;
    cells_.store_b[s] = nb;
}

void World::step() {
    births_.clear();
    deaths_.clear();
    const bool had_cells = cell_count_ > 0;

    phase_environment();
    phase_sense();
    phase_move();
    phase_feed();
    phase_attack();
    phase_share();
    phase_infect();
    phase_upkeep();
    phase_divide();
    phase_record();

    if (had_cells && cell_count_ == 0) extinct_at_ = tick_;
    ++tick_;
}

void World::phase_environment() {
    prepare_climate();

    apply_sparks(cur_, next_, climate_.spark_count(season_), rng_, params_);
    std::swap(cur_, next_);

    apply_spoilage(cur_, next_, params_);
    std::swap(cur_, next_);

    if (tick_ > 0 && tick_ % static_cast<std::uint64_t>(params_.disaster_interval) == 0) {
        last_disaster_ = draw_disaster(tick_, rng_, params_);
        const int w = params_.grid_width;
        const int h = params_.grid_height;
        for (int dy = 0; dy < params_.disaster_size; ++dy) {
            const int row = (last_disaster_->y + dy) % h;
            for (int dx = 0; dx < params_.disaster_size; ++dx) {
                const int col = (last_disaster_->x + dx) % w;
                const auto s = static_cast<std::size_t>(row * w + col);
                if (cells_.alive[s]) kill_cell(s, DeathCause::Disaster);
            }
        }
    }
}

MatterTotals World::matter() const {
    MatterTotals m;
    m.food_a = accurate_sum(cur_.food_a);
    m.food_b = accurate_sum(cur_.food_b);
    m.minerals = accurate_sum(cur_.minerals);
    double sum = 0.0, comp = 0.0;
    neumaier_add(sum, comp, cells_.store_a);
    neumaier_add(sum, comp, cells_.store_b);
    m.cells = sum + comp + cell_count_ * (params_.body_mass_a + params_.body_mass_b);
    return m;
}

std::uint64_t World::state_hash() const {
    Fnv1a h;
    h.value(tick_);
    h.value(next_id_);
    h.value(rng_.state());
    h.value(rng_.increment());
    h.array(cur_.food_a);
    h.array(cur_.food_b);
    h.array(cur_.minerals);
    h.array(cells_.alive);
    h.array(cells_.id);
    h.array(cells_.parent_id);
    h.array(cells_.parent2_id);
    h.array(cells_.bonds);
    h.array(cells_.store_a);
    h.array(cells_.store_b);
    h.array(cells_.age);
    h.array(cells_.cooldown);
    h.array(cells_.stress);
    h.array(cells_.infected);
    h.array(cells_.virus_tag);
    for (const auto& g : cells_.genes) h.array(g);
    return h.digest();
}

}  // namespace evo
