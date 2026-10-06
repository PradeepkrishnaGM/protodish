#pragma once

#include <cstdint>

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
};

}  // namespace evo
