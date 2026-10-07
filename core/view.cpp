#include "view.hpp"

#include <algorithm>
#include <cmath>

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

}  // namespace

const char* view_mode_name(ViewMode mode) {
    switch (mode) {
        case ViewMode::Lineage: return "Lineage";
        case ViewMode::Energy: return "Energy";
        case ViewMode::Feeding: return "Feeding type";
        case ViewMode::Infection: return "Infection";
        case ViewMode::Ground: return "Ground";
        case ViewMode::Count: break;
    }
    return "?";
}

Rgb tag_color(double tag) { return hsv(tag, view::kTagSaturation, view::kTagValue); }

Rgb energy_color(double fraction) {
    // Three stops: dark red, amber, pale yellow.
    constexpr Rgb low = {110, 25, 25};
    constexpr Rgb mid = {235, 150, 30};
    constexpr Rgb high = {255, 248, 200};
    const double t = std::clamp(fraction, 0.0, 1.0);
    return t < 0.5 ? lerp(low, mid, t * 2.0) : lerp(mid, high, (t - 0.5) * 2.0);
}

Rgb ground_color(double food_a, double food_b, double minerals) {
    auto channel = [](double amount, double full) { return to_byte(std::sqrt(std::max(0.0, amount) / full)); };
    return {channel(food_a, view::kGroundFoodFull), channel(food_b, view::kGroundFoodFull),
            channel(minerals, view::kGroundMineralFull)};
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
