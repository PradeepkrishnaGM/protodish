#pragma once

// Thin Godot wrapper around evo::World (RULES.md, "Building the engine"). It owns one
// world, steps it, and paints it into an Image. No simulation logic lives here.

#include <cstdint>
#include <memory>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

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

protected:
    static void _bind_methods();

private:
    std::unique_ptr<evo::World> world_;
    PackedByteArray pixels_;
    int64_t ended_at_ = -1;
};

}  // namespace godot

VARIANT_ENUM_CAST(ProtodishWorld::ViewMode);
