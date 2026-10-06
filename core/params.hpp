#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

#include "genome.hpp"

namespace evo {

// Every tunable number from RULES.md, with the RULES.md values as defaults.
// Phase code reads these; it never hard-codes a rule number.
//
// The list below defines both the struct fields and the keys a params file may set
// (see load_params). X(type, name, default).
#define EVO_PARAMS(X)                                                                         \
    /* The world */                                                                           \
    X(int, grid_width, 128)                                                                   \
    X(int, grid_height, 128)                                                                  \
    /* Environment: season and climate */                                                     \
    X(int, year_length, 2000)                 /* ticks per year */                            \
    X(double, temp_base, 15.0)                /* °C */                                        \
    X(double, temp_season_amp, 10.0)          /* °C per unit of season */                     \
    X(double, temp_latitude_amp, 5.0)         /* °C per unit of latitude */                   \
    X(double, temp_min, 0.0)                                                                  \
    X(double, temp_max, 30.0)                                                                 \
    X(double, light_base, 0.5)                                                                \
    X(double, light_season_amp, 0.3)                                                          \
    X(double, light_latitude_amp, 0.2)                                                        \
    X(double, light_min, 0.0)                                                                 \
    X(double, light_max, 1.0)                                                                 \
    /* Environment: sparks, spoilage, disasters */                                            \
    X(double, spark_base, 375.0)              /* strikes per tick at season 0 */              \
    X(double, spark_season_amp, 250.0)        /* extra strikes per unit of season */          \
    X(double, spark_amount, 4.0)              /* max minerals converted per strike */         \
    X(double, spoilage_rate, 0.01)            /* fraction of food turning to minerals */      \
    X(int, disaster_interval, 5000)           /* ticks between disasters */                   \
    X(int, disaster_size, 24)                 /* side of the square that dies */              \
    /* Starting conditions (ancestor genes are set with ancestor.<gene>) */                   \
    X(double, initial_food_a, 4.0)                                                            \
    X(double, initial_food_b, 4.0)                                                            \
    X(double, initial_minerals, 2.0)                                                          \
    X(int, initial_cells, 50)                                                                 \
    X(double, initial_store_a, 15.0)                                                          \
    X(double, initial_store_b, 15.0)                                                          \
    X(std::uint32_t, initial_age, 10)                                                         \
    /* The cell */                                                                            \
    X(double, store_max, 50.0)                /* per store; overflow falls on own site */     \
    X(double, body_mass_a, 4.0)                                                               \
    X(double, body_mass_b, 4.0)                                                               \
    /* Sense */                                                                               \
    X(double, thermal_width, 15.0)            /* °C from preferred where efficiency is 0 */  \
    X(double, supply_food_norm, 20.0)         /* food within reach is divided by this */      \
    X(double, supply_mineral_norm, 20.0)      /* minerals within reach are divided by this */ \
    /* Move */                                                                                \
    X(double, move_food_norm, 20.0)           /* food on a candidate site / this */           \
    X(double, neighbor_norm, 8.0)             /* prey, threat and kin counts / this */        \
    /* Feed */                                                                                \
    X(double, feed_rate, 2.0)                 /* capacity = rate × harvest × diet² × eff */   \
    /* Upkeep (per tick) */                                                                   \
    X(double, cost_alive, 0.2)                                                                \
    X(double, cost_harvest, 0.2)              /* × harvest */                                 \
    X(double, cost_attack, 0.5)               /* × attack */                                  \
    X(double, cost_defense, 0.3)              /* × defense */                                 \
    X(double, cost_crowding, 0.03)            /* per occupied neighbor beyond crowding_free */\
    X(int, crowding_free, 3)                                                                  \
    X(double, cost_aging, 0.001)              /* × age */                                     \
    X(double, cost_move, 0.2)                 /* if it moved this tick */                     \
    X(double, cost_infection, 0.3)            /* while it carries a virus */                  \
    X(double, cost_photosynthesis, 0.3)       /* × photosynthesis */                          \
    X(double, cost_resistance, 0.2)           /* × resistance */                              \
    X(double, upkeep_temp_base, 0.5)          /* total × (base + temperature / scale) */      \
    X(double, upkeep_temp_scale, 30.0)                                                        \
    /* Divide */                                                                              \
    X(std::uint32_t, divide_min_age, 10)                                                      \
    X(double, divide_min_store, 12.0)         /* of each of A and B */                        \
    X(double, clone_cost, 10.0)               /* of each of A and B, cloning mother */        \
    X(double, daughter_store, 6.0)            /* of each of A and B */                        \
    X(std::uint32_t, divide_cooldown, 5)                                                      \
    /* Mutation */                                                                            \
    X(double, mutation_base, 0.05)            /* chance per gene */                           \
    X(double, mutation_stress_factor, 4.0)    /* base × (1 + factor × mutability × stress) */ \
    X(double, mutation_step, 0.1)             /* max shift as a fraction of the range */      \
    X(double, tag_mutation_step, 0.05)        /* max shift of the tag (wraps) */

struct Params {
#define EVO_PARAM_FIELD(type, name, def) type name = def;
    EVO_PARAMS(EVO_PARAM_FIELD)
#undef EVO_PARAM_FIELD

    Genome ancestor = {
        0.5,   // tag
        0.1,   // tolerance
        0.8,   // harvest
        0.5,   // diet
        0.0,   // photosynthesis
        15.0,  // preferred temperature
        0.0,   // attack
        0.0,   // defense
        0.0,   // resistance
        0.0,   // adhesion
        0.0,   // share
        0.0,   // role split
        0.5,   // motility
        0.5,   // appetite
        0.0,   // caution
        0.0,   // boldness
        0.0,   // sociability
        0.0,   // dormancy
        0.0,   // mutability
        0.0,   // mating
    };
};

// Applies `key = value` lines to `p`. Blank lines and lines starting with '#' are skipped;
// a '#' after a value starts a comment. Keys are Params field names, or ancestor.<gene>
// with the gene names from kGeneInfo. Returns an empty string on success, otherwise a
// message naming the first bad line; `p` may then be partly updated.
std::string apply_params(Params& p, std::istream& in);
std::string load_params_file(Params& p, const std::string& path);

// Checks values the engine relies on. Returns an empty string when they are usable.
std::string validate_params(const Params& p);

// Writes every value in the same key = value format.
void write_params(const Params& p, std::ostream& out);

}  // namespace evo
