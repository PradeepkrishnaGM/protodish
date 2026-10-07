#pragma once

// Colors of the app's world view (RULES.md, "The application", view mode). One RGB8
// pixel per site. This is presentation only: it reads the world and never changes it.
// It has no Godot code, so it is tested with the core.

#include <array>
#include <cstdint>

#include "world.hpp"

namespace evo {

enum class ViewMode : int {
    Lineage,    // tag as hue
    Energy,     // A + B in store, dark red (empty) to pale yellow (store_max or more)
    Feeding,    // producer (photosynthesis gene > harvest gene) or consumer
    Infection,  // infected cells in the hue of their virus tag, healthy cells grey
    Ground,     // food A, food B and minerals as red, green and blue; cells not shown
    Count
};

inline constexpr int kViewModeCount = static_cast<int>(ViewMode::Count);
const char* view_mode_name(ViewMode mode);

using Rgb = std::array<std::uint8_t, 3>;

// Display constants. They tune the picture, not the simulation, so they are not Params.
namespace view {
inline constexpr Rgb kEmpty = {16, 20, 24};       // empty site in every cell mode
inline constexpr Rgb kProducer = {90, 200, 80};
inline constexpr Rgb kConsumer = {235, 125, 45};
inline constexpr Rgb kHealthy = {72, 78, 86};
inline constexpr double kTagSaturation = 0.85;
inline constexpr double kTagValue = 0.95;
// Ground: a channel is sqrt(amount / full), at most 1. The square root keeps small
// amounts visible; "full" is about the 99th percentile seen in default runs.
inline constexpr double kGroundFoodFull = 12.0;
inline constexpr double kGroundMineralFull = 50.0;
}  // namespace view

Rgb tag_color(double tag);           // hue = tag, used by Lineage and Infection
Rgb energy_color(double fraction);   // 0 = empty stores, 1 = full; clamped
Rgb ground_color(double food_a, double food_b, double minerals);

// Writes site_count() × 3 bytes, row by row from row 0.
void paint_view(const World& w, ViewMode mode, std::uint8_t* rgb);

}  // namespace evo
