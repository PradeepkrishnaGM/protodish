#include "view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace evo {

namespace {

std::uint8_t to_byte(double v) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
}

Rgb hsv(double h, double s, double v) {
    h = (h - std::floor(h)) * 6.0;
    const int i = static_cast<int>(h) % 6;
    const double f = h - std::floor(h);
    const double p = v * (1.0 - s);
    const double q = v * (1.0 - s * f);
    const double t = v * (1.0 - s * (1.0 - f));
    switch (i) {
        case 0: return {to_byte(v), to_byte(t), to_byte(p)};
        case 1: return {to_byte(q), to_byte(v), to_byte(p)};
        case 2: return {to_byte(p), to_byte(v), to_byte(t)};
        case 3: return {to_byte(p), to_byte(q), to_byte(v)};
        case 4: return {to_byte(t), to_byte(p), to_byte(v)};
        default: return {to_byte(v), to_byte(p), to_byte(q)};
    }
}

Rgb lerp(const Rgb& a, const Rgb& b, double t) {
    Rgb out{};
    for (int k = 0; k < 3; ++k) {
        out[static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(std::lround(
            a[static_cast<std::size_t>(k)] + (b[static_cast<std::size_t>(k)] - a[static_cast<std::size_t>(k)]) * t));
    }
    return out;
}

double ground_level(double amount, double full) { return std::sqrt(std::max(0.0, amount) / full); }

std::string number(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g", v);
    return buf;
}

template <typename F>
std::vector<Rgb> sample(F color_at) {
    std::vector<Rgb> out;
    for (int k = 0; k < view::kGradientSamples; ++k) out.push_back(color_at(k / double(view::kGradientSamples - 1)));
    return out;
}

}  // namespace

const char* view_mode_name(ViewMode mode) {
    switch (mode) {
        case ViewMode::Lineage: return "Lineage";
        case ViewMode::Energy: return "Energy";
        case ViewMode::Feeding: return "Feeding type";
        case ViewMode::Infection: return "Infection";
        case ViewMode::Ground: return "Ground";
        case ViewMode::GroundFoodA: return "Ground: food A";
        case ViewMode::GroundFoodB: return "Ground: food B";
        case ViewMode::GroundMinerals: return "Ground: minerals";
        case ViewMode::Count: break;
    }
    return "?";
}

Rgb tag_color(double tag) { return hsv(tag, view::kTagSaturation, view::kTagValue); }

Rgb energy_color(double fraction) {
    const double t = std::clamp(fraction, 0.0, 1.0);
    return t < 0.5 ? lerp(view::kEnergyLow, view::kEnergyMid, t * 2.0)
                   : lerp(view::kEnergyMid, view::kEnergyHigh, (t - 0.5) * 2.0);
}

Rgb ground_color(double food_a, double food_b, double minerals) {
    auto channel = [](double amount, double full) { return to_byte(ground_level(amount, full)); };
    return {channel(food_a, view::kGroundFoodFull), channel(food_b, view::kGroundFoodFull),
            channel(minerals, view::kGroundMineralFull)};
}

Rgb layer_color(double amount, double full, const Rgb& color) {
    constexpr Rgb black = {0, 0, 0};
    constexpr Rgb white = {255, 255, 255};
    const double t = std::min(1.0, ground_level(amount, full));
    return t < view::kLayerKnee ? lerp(black, color, t / view::kLayerKnee)
                                : lerp(color, white, (t - view::kLayerKnee) / (1.0 - view::kLayerKnee));
}

Rgb temperature_color(double temperature, const Params& p) {
    const double t = std::clamp((temperature - p.temp_min) / (p.temp_max - p.temp_min), 0.0, 1.0);
    return t < 0.5 ? lerp(view::kCold, view::kMild, t * 2.0) : lerp(view::kMild, view::kHot, (t - 0.5) * 2.0);
}

namespace {

std::vector<LegendItem> mode_legend(ViewMode mode, const Params& p) {
    const LegendItem empty{"empty site", {view::kEmpty}, "", ""};
    const auto layer = [](const char* label, double full, const Rgb& color) {
        return LegendItem{label, sample([&](double t) { return layer_color(t * t * full, full, color); }), "0",
                          number(full) + "+"};
    };
    switch (mode) {
        case ViewMode::Lineage:
            return {{"tag (lineage)", sample([](double t) { return tag_color(t); }), "0", "1"}, empty};
        case ViewMode::Energy:
            return {{"stores A + B", sample([](double t) { return energy_color(t); }), "0", number(p.store_max) + "+"},
                    empty};
        case ViewMode::Feeding:
            return {{"producer (photosynthesis > harvest)", {view::kProducer}, "", ""},
                    {"consumer", {view::kConsumer}, "", ""},
                    empty};
        case ViewMode::Infection:
            return {{"healthy", {view::kHealthy}, "", ""},
                    {"infected: virus tag", sample([](double t) { return tag_color(t); }), "0", "1"},
                    empty};
        case ViewMode::Ground:
            return {{"food A", {{255, 0, 0}}, "", ""},
                    {"food B", {{0, 255, 0}}, "", ""},
                    {"minerals", {{0, 0, 255}}, "", ""},
                    {"food A + B", {{255, 255, 0}}, "", ""},
                    {"none (brighter = more)", {{0, 0, 0}}, "", ""}};
        case ViewMode::GroundFoodA: return {layer("food A per site", view::kGroundFoodFull, view::kLayerFoodA)};
        case ViewMode::GroundFoodB: return {layer("food B per site", view::kGroundFoodFull, view::kLayerFoodB)};
        case ViewMode::GroundMinerals:
            return {layer("minerals per site", view::kGroundMineralFull, view::kLayerMinerals)};
        case ViewMode::Count: break;
    }
    return {};
}

}  // namespace

std::vector<LegendItem> view_legend(ViewMode mode, const Params& p) {
    std::vector<LegendItem> items = mode_legend(mode, p);
    items.push_back({"row temperature (left strip)",
                     sample([&](double t) { return temperature_color(p.temp_min + t * (p.temp_max - p.temp_min), p); }),
                     number(p.temp_min) + " °C", number(p.temp_max) + " °C"});
    return items;
}

void paint_view(const World& w, ViewMode mode, std::uint8_t* rgb) {
    const CellArrays& c = w.cells();
    const auto& g = c.genes;
    const auto food_a = w.food_a();
    const auto food_b = w.food_b();
    const auto minerals = w.minerals();
    const double store_max = w.params().store_max;
    const int n = w.site_count();
    for (int s = 0; s < n; ++s) {
        const auto i = static_cast<std::size_t>(s);
        Rgb px = view::kEmpty;
        if (mode == ViewMode::Ground) {
            px = ground_color(food_a[i], food_b[i], minerals[i]);
        } else if (mode == ViewMode::GroundFoodA) {
            px = layer_color(food_a[i], view::kGroundFoodFull, view::kLayerFoodA);
        } else if (mode == ViewMode::GroundFoodB) {
            px = layer_color(food_b[i], view::kGroundFoodFull, view::kLayerFoodB);
        } else if (mode == ViewMode::GroundMinerals) {
            px = layer_color(minerals[i], view::kGroundMineralFull, view::kLayerMinerals);
        } else if (c.alive[i]) {
            switch (mode) {
                case ViewMode::Lineage: px = tag_color(g[kTag][i]); break;
                case ViewMode::Energy: px = energy_color((c.store_a[i] + c.store_b[i]) / store_max); break;
                case ViewMode::Feeding:
                    px = g[kPhotosynthesis][i] > g[kHarvest][i] ? view::kProducer : view::kConsumer;
                    break;
                case ViewMode::Infection: px = c.infected[i] ? tag_color(c.virus_tag[i]) : view::kHealthy; break;
                default: break;
            }
        }
        std::copy(px.begin(), px.end(), rgb + i * 3);
    }
}

}  // namespace evo
