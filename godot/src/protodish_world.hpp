#pragma once

// Thin Godot wrapper around evo::World (RULES.md, "Building the engine"). It owns one
// world, steps it, and paints it into an Image. No simulation logic lives here.

#include <cstdint>
#include <memory>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include "history.hpp"
#include "inspect.hpp"
#include "view.hpp"
#include "world.hpp"

namespace godot {

class ProtodishWorld : public RefCounted {
    GDCLASS(ProtodishWorld, RefCounted)

public:
    // Same order as evo::ViewMode.
    enum ViewMode {
        VIEW_LINEAGE,
        VIEW_ENERGY,
        VIEW_FEEDING,
        VIEW_INFECTION,
        VIEW_GROUND,
        VIEW_GROUND_FOOD_A,
        VIEW_GROUND_FOOD_B,
        VIEW_GROUND_MINERALS,
        VIEW_MODE_COUNT
    };

    ProtodishWorld();
    ~ProtodishWorld() override;

    // Builds a new world from the starting conditions. `params_text` holds `key = value`
    // lines applied on top of the RULES.md defaults (a preset file's contents). Returns
    // "" on success; otherwise an error message, and the current world is kept.
    String reset(int64_t seed, const String& params_text = String());
    // Runs up to n ticks and returns how many ran; stops early at extinction, and runs
    // none once the world has been ended.
    int64_t step(int64_t n);
    // End world (DECISIONS M7-1): clears every cell, keeping its matter as food.
    void end_world();

    int64_t get_tick() const;
    int64_t get_seed() const;
    int64_t get_cell_count() const;
    bool is_extinct() const;
    int64_t get_extinct_at() const;  // -1 while alive
    bool is_ended() const;
    int64_t get_ended_at() const;  // -1 unless ended
    int64_t get_width() const;
    int64_t get_height() const;
    // The core's 64-bit state hash, as 16 hex digits (it does not fit a signed int64).
    String get_state_hash() const;
    double get_total_matter() const;

    // Paints one pixel per site in the given ViewMode and returns the image
    // (width × height, RGB8). An unknown mode paints Lineage.
    Ref<Image> render(int64_t mode);
    // Display names of the view modes, in ViewMode order.
    PackedStringArray get_view_mode_names() const;
    // Legend of a view mode: an Array of Dictionaries {label, colors (PackedColorArray),
    // low, high}. One color is a swatch; several are a gradient from low to high.
    Array get_legend(int64_t mode) const;

    // Statistics panel (RULES.md, "Statistics"): a Dictionary built from the census, plus
    // the season and the temperature and light range of the tick just run. Keys are listed
    // in protodish_world.cpp.
    Dictionary get_stats() const;
    // Population graph: {ticks: PackedInt64Array, cells, producers, infected:
    // PackedInt32Array}, at most max_points points, each the peak of the ticks it covers.
    Dictionary get_history(int64_t max_points) const;

    // Population graph from from_tick on (see get_history).
    Dictionary get_history_since(int64_t max_points, int64_t from_tick) const;
    // One pixel per row, the row's temperature in the tick just run (blue cold, red hot).
    Ref<Image> render_temperature_strip() const;

    // Click-to-inspect. select_site() selects the cell on a site (false if it is empty);
    // the selection then follows that cell by ID until it dies.
    bool select_site(int64_t site);
    void clear_selection();
    int64_t get_selected_site() const;  // -1 unless the selected cell is alive
    // The selected cell: {state: "none" | "alive" | "dead" | "removed", id, site, row, col,
    // death_tick, death_cause}, and while alive its genes (Array of {name, value, min, max})
    // and state (stores, age, stress, ...). Keys are listed in protodish_world.cpp.
    Dictionary inspect_cell() const;
    // The ground and climate of one site: {row, col, food_a, food_b, minerals, temperature,
    // light, occupied}.
    Dictionary inspect_site(int64_t site) const;

protected:
    static void _bind_methods();

private:
    std::unique_ptr<evo::World> world_;
    PackedByteArray pixels_;
    evo::PopulationHistory history_;  // after every tick, from the starting state on
    evo::CellTracker tracker_;
    int64_t ended_at_ = -1;
};

}  // namespace godot

VARIANT_ENUM_CAST(ProtodishWorld::ViewMode);
