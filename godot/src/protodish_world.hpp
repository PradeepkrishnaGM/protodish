#pragma once

// Thin Godot wrapper around evo::World (RULES.md, "Building the engine"). It owns one
// world, steps it, and paints it into an Image. No simulation logic lives here.

#include <cstdint>
#include <memory>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include "view.hpp"
#include "world.hpp"

namespace godot {

class ProtodishWorld : public RefCounted {
    GDCLASS(ProtodishWorld, RefCounted)

public:
    // Same order as evo::ViewMode.
    enum ViewMode { VIEW_LINEAGE, VIEW_ENERGY, VIEW_FEEDING, VIEW_INFECTION, VIEW_GROUND, VIEW_MODE_COUNT };

    ProtodishWorld();
    ~ProtodishWorld() override;

    // Builds a new world from the starting conditions with the RULES.md defaults.
    void reset(int64_t seed);
    // Runs up to n ticks and returns how many ran; stops early at extinction.
    int64_t step(int64_t n);

    int64_t get_tick() const;
    int64_t get_seed() const;
    int64_t get_cell_count() const;
    bool is_extinct() const;
    int64_t get_extinct_at() const;  // -1 while alive
    int64_t get_width() const;
    int64_t get_height() const;
    // The core's 64-bit state hash, as 16 hex digits (it does not fit a signed int64).
    String get_state_hash() const;

    // Paints one pixel per site in the given ViewMode and returns the image
    // (width × height, RGB8). An unknown mode paints Lineage.
    Ref<Image> render(int64_t mode);
    // Display names of the view modes, in ViewMode order.
    PackedStringArray get_view_mode_names() const;

protected:
    static void _bind_methods();

private:
    std::unique_ptr<evo::World> world_;
    PackedByteArray pixels_;
};

}  // namespace godot

VARIANT_ENUM_CAST(ProtodishWorld::ViewMode);
