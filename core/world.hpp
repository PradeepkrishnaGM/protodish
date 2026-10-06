#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "environment.hpp"
#include "genome.hpp"
#include "params.hpp"
#include "rng.hpp"

namespace evo {

struct MatterTotals {
    double food_a = 0.0;
    double food_b = 0.0;
    double minerals = 0.0;
    double cells = 0.0;  // stores + body mass of every living cell
    double total() const { return food_a + food_b + minerals + cells; }
};

// Cell properties, one entry per site (structure of arrays). alive[s] says whether
// site s holds a cell; the other entries at s are meaningful only when it does.
struct CellArrays {
    std::vector<std::uint8_t> alive;
    std::vector<std::uint64_t> id;
    std::vector<std::uint64_t> parent_id;
    std::vector<std::uint64_t> parent2_id;  // 0 unless born of a mating
    std::vector<std::uint8_t> bonds;        // bit d: bonded to neighbor(s, d); kept symmetric
    std::vector<double> store_a;
    std::vector<double> store_b;
    std::vector<std::uint32_t> age;
    std::vector<std::uint32_t> cooldown;
    std::vector<double> stress;
    std::vector<std::uint8_t> moved;  // moved this tick
    std::array<std::vector<double>, kGeneCount> genes;

    // Sense results for the current tick; they travel with the cell (DECISIONS M2-14).
    std::vector<std::uint8_t> awake;
    std::vector<double> thermal_eff;
    std::vector<double> supply;
    // Role-adjusted genes for this tick (RULES.md rule 1, role split).
    std::vector<double> eff_harvest;
    std::vector<double> eff_attack;
    std::vector<double> eff_defense;
    // Per-tick results used by later phases of the same tick.
    std::vector<std::uint8_t> drained;   // lost matter to an attack this tick
    std::vector<double> gross_intake;    // Feed intake before leak (food taken + made)
    std::vector<double> feed_capacity;   // A + B + photosynthesis capacity this tick

    void resize(std::size_t n);
    void move(std::size_t from, std::size_t to);
    void clear(std::size_t s);
    Genome genome(std::size_t s) const;
    void set_genome(std::size_t s, const Genome& g);
};

enum class BirthKind : std::uint8_t { AttachedClone, ReleasedClone, Mating };
enum class DeathCause : std::uint8_t { Disaster, Drained, Starved };

struct BirthEvent {
    std::uint64_t tick;
    std::uint64_t id;
    std::uint64_t parent_id;
    std::uint64_t parent2_id;  // 0 unless a mating
    int site;
    Genome genome;
    BirthKind kind;
};

struct DeathEvent {
    std::uint64_t tick;
    std::uint64_t id;
    std::uint32_t age;
    int site;
    DeathCause cause;
};

class World {
public:
    World(const Params& params, std::uint64_t seed);

    // Runs the ten phases of the current tick, then advances the tick counter.
    void step();

    const Params& params() const { return params_; }
    const Climate& climate() const { return climate_; }
    std::uint64_t tick() const { return tick_; }
    std::uint64_t seed() const { return seed_; }
    int site_count() const { return n_sites_; }
    int neighbor(int site, int dir) const { return neighbors_[static_cast<std::size_t>(site * 8 + dir)]; }

    // Season, temperature and light of the most recently prepared tick
    // (tick 0 before the first step, otherwise the tick just run).
    double season() const { return season_; }
    double row_temperature(int row) const { return row_temperature_[static_cast<std::size_t>(row)]; }
    double row_light(int row) const { return row_light_[static_cast<std::size_t>(row)]; }
    double site_temperature(int site) const { return row_temperature(site / params_.grid_width); }
    double site_light(int site) const { return row_light(site / params_.grid_width); }

    std::span<const double> food_a() const { return cur_.food_a; }
    std::span<const double> food_b() const { return cur_.food_b; }
    std::span<const double> minerals() const { return cur_.minerals; }
    const CellArrays& cells() const { return cells_; }
    int cell_count() const { return cell_count_; }

    const std::optional<DisasterEvent>& last_disaster() const { return last_disaster_; }
    // Events of the tick just run.
    const std::vector<BirthEvent>& births() const { return births_; }
    const std::vector<DeathEvent>& deaths() const { return deaths_; }
    // Tick at which the last cell died, if the world has gone extinct.
    const std::optional<std::uint64_t>& extinct_at() const { return extinct_at_; }

    MatterTotals matter() const;
    std::uint64_t state_hash() const;

    // Setup hooks for tests and tools. These add or replace matter, so they break
    // conservation if used mid-run. add_cell returns false if the site is taken.
    bool add_cell(int site, const Genome& g, double store_a, double store_b, std::uint32_t age);
    void set_site(int site, double food_a, double food_b, double minerals);
    void set_cell_stores(int site, double store_a, double store_b);
    void set_cell_cooldown(int site, std::uint32_t cooldown);
    void set_cell_stress(int site, double stress);
    // Bonds two living, adjacent cells. Returns false otherwise.
    bool add_bond(int a, int b);

    // Body label per site: -1 for an empty site, otherwise the index of the cell's
    // connected bond group, numbered in ascending order of each group's lowest site.
    // A free cell is a group of 1; bodies are the groups of 2 or more (DECISIONS M4-5).
    std::vector<int> body_labels() const;

    // Relations from RULES.md rule 1. They depend on genes and dormancy, not position.
    bool is_kin(int a, int b) const;
    bool bonded(int a, int b) const;
    bool is_inner(int s) const { return cells_.bonds[static_cast<std::size_t>(s)] == 0xFF; }
    bool treats_as_prey(int a, int b) const;
    double effective_defense(int s) const;

private:
    void prepare_climate();
    void collect_cell_sites();
    void kill_cell(std::size_t s, DeathCause cause);
    void add_to_store(std::size_t s, double a, double b);
    int direction(int from, int to) const;  // d with neighbor(from, d) == to, or -1
    void link(int a, int b);

    void phase_environment();
    void phase_sense();
    void phase_move();
    void phase_feed();
    void phase_attack();
    void phase_share();
    void phase_infect() {}  // M5
    void phase_upkeep();
    void phase_divide();
    void phase_record() {}  // events are collected by the phases; files are written in M6

    Params params_;
    Climate climate_;
    Rng rng_;
    std::uint64_t seed_;
    int n_sites_;
    std::uint64_t tick_ = 0;
    std::uint64_t next_id_ = 1;
    std::vector<int> neighbors_;  // 8 per site: N, NE, E, SE, S, SW, W, NW

    double season_ = 0.0;
    std::vector<double> row_temperature_;
    std::vector<double> row_light_;

    SiteState cur_;
    SiteState next_;
    CellArrays cells_;
    int cell_count_ = 0;
    std::vector<int> cell_sites_;  // occupied sites in ascending order, rebuilt per phase

    std::optional<DisasterEvent> last_disaster_;
    std::vector<BirthEvent> births_;
    std::vector<DeathEvent> deaths_;
    std::optional<std::uint64_t> extinct_at_;
};

}  // namespace evo
