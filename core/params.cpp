#include "params.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <istream>
#include <ostream>
#include <set>
#include <string_view>
#include <type_traits>

namespace evo {

namespace {

std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(" \t\r");
    return s.substr(first, last - first + 1);
}

// Whole-string, locale-independent parsing.
template <typename T>
bool parse_value(std::string_view text, T& out) {
    T v{};
    const auto* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, v);
    if (ec != std::errc() || ptr != end) return false;
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(v)) return false;
    }
    out = v;
    return true;
}

bool set_field(Params& p, std::string_view key, std::string_view value) {
#define EVO_PARAM_SET(type, name, def) \
    if (key == #name) return parse_value<type>(value, p.name);
    EVO_PARAMS(EVO_PARAM_SET)
#undef EVO_PARAM_SET
    constexpr std::string_view kPrefix = "ancestor.";
    if (key.substr(0, kPrefix.size()) == kPrefix) {
        const std::string_view gene = key.substr(kPrefix.size());
        for (std::size_t i = 0; i < kGeneCount; ++i) {
            if (gene != kGeneInfo[i].name) continue;
            double v = 0.0;
            if (!parse_value(value, v)) return false;
            const auto& info = kGeneInfo[i];
            if (v < info.min || (info.circular ? v >= info.max : v > info.max)) return false;
            p.ancestor[i] = v;
            return true;
        }
    }
    return false;
}

bool is_key(std::string_view key) {
#define EVO_PARAM_KEY(type, name, def) \
    if (key == #name) return true;
    EVO_PARAMS(EVO_PARAM_KEY)
#undef EVO_PARAM_KEY
    for (const auto& info : kGeneInfo) {
        if (key == std::string("ancestor.") + info.name) return true;
    }
    return false;
}

// Shortest text that reads back to the same value.
template <typename T>
std::string format_value(T v) {
    char buf[64];
    const auto [ptr, ec] = std::to_chars(buf, buf + sizeof buf, v);
    return ec == std::errc() ? std::string(buf, ptr) : std::string("?");
}

}  // namespace

std::string apply_params(Params& p, std::istream& in) {
    std::string line;
    std::set<std::string, std::less<>> seen;
    for (int line_no = 1; std::getline(in, line); ++line_no) {
        std::string_view s = line;
        if (const auto hash = s.find('#'); hash != std::string_view::npos) s = s.substr(0, hash);
        s = trim(s);
        if (s.empty()) continue;

        const std::string where = "line " + std::to_string(line_no) + ": ";
        const auto eq = s.find('=');
        if (eq == std::string_view::npos) return where + "expected key = value";
        const std::string_view key = trim(s.substr(0, eq));
        const std::string_view value = trim(s.substr(eq + 1));
        if (!is_key(key)) return where + "unknown key '" + std::string(key) + "'";
        if (!seen.insert(std::string(key)).second) {
            return where + "duplicate key '" + std::string(key) + "'";
        }
        if (!set_field(p, key, value)) {
            return where + "bad value '" + std::string(value) + "' for " + std::string(key);
        }
    }
    return {};
}

std::string load_params_file(Params& p, const std::string& path) {
    std::ifstream in(path);
    if (!in) return "cannot open " + path;
    std::string err = apply_params(p, in);
    return err.empty() ? err : path + ", " + err;
}

std::string validate_params(const Params& p) {
    if (p.grid_width < 3 || p.grid_height < 3) return "grid must be at least 3 x 3";
    if (p.year_length < 1) return "year_length must be at least 1";
    if (p.disaster_interval < 1) return "disaster_interval must be at least 1";
    if (p.disaster_size < 0 || p.disaster_size > p.grid_width || p.disaster_size > p.grid_height) {
        return "disaster_size must fit the grid";
    }
    if (p.initial_cells < 0 || p.initial_cells > p.grid_width * p.grid_height) {
        return "initial_cells must fit the grid";
    }
    if (p.store_max <= 0.0) return "store_max must be positive";
    if (p.thermal_width <= 0.0 || p.upkeep_temp_scale <= 0.0 || p.supply_food_norm <= 0.0 ||
        p.supply_mineral_norm <= 0.0 || p.move_food_norm <= 0.0 || p.neighbor_norm <= 0.0) {
        return "normalising widths and scales must be positive";
    }
    // Division moves clone_cost of each food into the daughter's stores and body mass.
    if (p.clone_cost != p.daughter_store + p.body_mass_a ||
        p.clone_cost != p.daughter_store + p.body_mass_b) {
        return "clone_cost must equal daughter_store + body_mass (matter conservation)";
    }
    if (2.0 * p.mating_cost != p.daughter_store + p.body_mass_a ||
        2.0 * p.mating_cost != p.daughter_store + p.body_mass_b) {
        return "2 × mating_cost must equal daughter_store + body_mass (matter conservation)";
    }
    return {};
}

void write_params(const Params& p, std::ostream& out) {
#define EVO_PARAM_WRITE(type, name, def) out << #name " = " << format_value(p.name) << '\n';
    EVO_PARAMS(EVO_PARAM_WRITE)
#undef EVO_PARAM_WRITE
    for (std::size_t i = 0; i < kGeneCount; ++i) {
        out << "ancestor." << kGeneInfo[i].name << " = " << format_value(p.ancestor[i]) << '\n';
    }
}

}  // namespace evo
