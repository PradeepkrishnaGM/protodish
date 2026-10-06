#pragma once

#include <cstdint>

#include "genome.hpp"

namespace evo {

// Every tunable number from RULES.md, with the RULES.md values as defaults.
// Phase code reads these; it never hard-codes a rule number.
struct Params {
    // The world
    int grid_width = 128;
    int grid_height = 128;

    // Environment: season and climate
    int year_length = 2000;            // ticks per year
    double temp_base = 15.0;           // °C
    double temp_season_amp = 10.0;     // °C per unit of season
    double temp_latitude_amp = 5.0;    // °C per unit of latitude
    double temp_min = 0.0;
    double temp_max = 30.0;
    double light_base = 0.5;
    double light_season_amp = 0.3;
    double light_latitude_amp = 0.2;
    double light_min = 0.0;
    double light_max = 1.0;

    // Environment: sparks, spoilage, disasters
    double spark_base = 375.0;         // strikes per tick at season 0
    double spark_season_amp = 250.0;   // extra strikes per unit of season
    double spark_amount = 4.0;         // max minerals converted per strike
    double spoilage_rate = 0.01;       // fraction of food turning to minerals per tick
    int disaster_interval = 5000;      // ticks between disasters
    int disaster_size = 24;            // side of the square that dies

    // Starting conditions
    double initial_food_a = 4.0;
    double initial_food_b = 4.0;
    double initial_minerals = 2.0;
    int initial_cells = 50;
    double initial_store_a = 15.0;
    double initial_store_b = 15.0;
    std::uint32_t initial_age = 10;
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

    // The cell
    double store_max = 50.0;           // per store; overflow falls onto the own site
    double body_mass_a = 4.0;
    double body_mass_b = 4.0;

    // Sense
    double thermal_width = 15.0;       // °C away from preferred at which efficiency hits 0
    double supply_food_norm = 20.0;    // food within reach is divided by this
    double supply_mineral_norm = 20.0; // minerals within reach are divided by this

    // Move
    double move_food_norm = 20.0;      // food on a candidate site is divided by this
    double neighbor_norm = 8.0;        // prey, threat and kin counts are divided by this

    // Feed
    double feed_rate = 2.0;            // capacity = feed_rate × harvest × diet² × efficiency

    // Upkeep (per tick)
    double cost_alive = 0.2;
    double cost_harvest = 0.2;         // × harvest
    double cost_attack = 0.5;          // × attack
    double cost_defense = 0.3;         // × defense
    double cost_crowding = 0.03;       // per occupied neighbor beyond crowding_free
    int crowding_free = 3;
    double cost_aging = 0.001;         // × age
    double cost_move = 0.2;            // if it moved this tick
    double cost_infection = 0.3;       // while it carries a virus
    double cost_photosynthesis = 0.3;  // × photosynthesis
    double cost_resistance = 0.2;      // × resistance
    double upkeep_temp_base = 0.5;     // total × (base + temperature / scale)
    double upkeep_temp_scale = 30.0;

    // Divide
    std::uint32_t divide_min_age = 10;
    double divide_min_store = 12.0;    // of each of A and B
    double clone_cost = 10.0;          // of each of A and B, paid by a cloning mother
    double daughter_store = 6.0;       // of each of A and B
    std::uint32_t divide_cooldown = 5;

    // Mutation
    double mutation_base = 0.05;       // chance per gene
    double mutation_stress_factor = 4.0;  // chance = base × (1 + factor × mutability × stress)
    double mutation_step = 0.1;        // max shift as a fraction of the gene's range
    double tag_mutation_step = 0.05;   // max shift of the tag (wraps)
};

}  // namespace evo
