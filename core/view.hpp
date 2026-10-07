#pragma once

// Colors of the app's world view (RULES.md, "The application", view mode). One RGB8
// pixel per site. This is presentation only: it reads the world and never changes it.
// It has no Godot code, so it is tested with the core.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "world.hpp"

namespace evo {

enum class ViewMode : int {
    Lineage,    // tag as hue
    Energy,     // A + B in store, dark red (empty) to pale yellow (store_max or more)
    Feeding,    // producer (photosynthesis gene > harvest gene) or consumer
    Infection,  // infected cells in the hue of their virus tag, healthy cells grey
    Ground,     // food A, food B and minerals as red, green and blue; cells not shown
    // One ground layer alone, black (none) through its color to white (full or more).
    GroundFoodA,
    GroundFoodB,
    GroundMinerals,
    Count
};

inline constexpr int kViewModeCount = static_cast<int>(ViewMode::Count);
const char* view_mode_name(ViewMode mode);
// Ground modes draw every site and leave cells out.
inline bool is_ground_mode(ViewMode m) { return m >= ViewMode::Ground && m < ViewMode::Count; }

using Rgb = std::array<std::uint8_t, 3>;

// Display constants. They tune the picture, not the simulation, so they are not Params.
namespace view {
inline constexpr Rgb kEmpty = {16, 20, 24};       // empty site in every cell mode
inline constexpr Rgb kProducer = {90, 200, 80};
inline constexpr Rgb kConsumer = {235, 125, 45};
inline constexpr Rgb kHealthy = {72, 78, 86};
// Energy ramp. The lowest stop is bright enough to stand out from kEmpty (contrast ratio
// above 3, tested).
inline constexpr Rgb kEnergyLow = {190, 60, 60};
inline constexpr Rgb kEnergyMid = {235, 150, 30};
inline constexpr Rgb kEnergyHigh = {255, 248, 200};
// Single ground layers: the color at sqrt(amount / full) = kLayerKnee, white at 1.
inline constexpr Rgb kLayerFoodA = {235, 50, 50};
inline constexpr Rgb kLayerFoodB = {50, 205, 70};
inline constexpr Rgb kLayerMinerals = {70, 120, 255};
inline constexpr double kLayerKnee = 0.7;
inline constexpr int kGradientSamples = 16;  // colors per legend gradient
// Row temperature strip beside the grid: blue at temp_min, pale at the midpoint, red at
// temp_max (Params; 0 and 30 °C by default). The scale is fixed, so seasons show.
inline constexpr Rgb kCold = {45, 95, 235};
inline constexpr Rgb kMild = {215, 215, 220};
inline constexpr Rgb kHot = {235, 55, 40};
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
Rgb layer_color(double amount, double full, const Rgb& color);  // one ground layer

// One legend line. One color is a swatch; several are a gradient from `low` to `high`.
struct LegendItem {
    std::string label;
    std::vector<Rgb> colors;
    std::string low;
    std::string high;
};
Rgb temperature_color(double temperature, const Params& p);
// Every mode's legend ends with the temperature strip's entry.
std::vector<LegendItem> view_legend(ViewMode mode, const Params& p);

// Writes site_count() × 3 bytes, row by row from row 0.
void paint_view(const World& w, ViewMode mode, std::uint8_t* rgb);

}  // namespace evo
