#include "protodish_world.hpp"

#include <cstdio>

#include <godot_cpp/core/class_db.hpp>

namespace godot {

ProtodishWorld::ProtodishWorld() { reset(1); }
ProtodishWorld::~ProtodishWorld() = default;

void ProtodishWorld::reset(int64_t seed) {
    const evo::Params params;  // RULES.md defaults; always valid, so World does not throw
    world_ = std::make_unique<evo::World>(params, static_cast<uint64_t>(seed));
    pixels_.resize(static_cast<int64_t>(world_->site_count()) * 3);
}

int64_t ProtodishWorld::step(int64_t n) {
    int64_t ran = 0;
    while (ran < n && !world_->extinct_at()) {
        world_->step();
        ++ran;
    }
    return ran;
}

int64_t ProtodishWorld::get_tick() const { return static_cast<int64_t>(world_->tick()); }
int64_t ProtodishWorld::get_seed() const { return static_cast<int64_t>(world_->seed()); }
int64_t ProtodishWorld::get_cell_count() const { return world_->cell_count(); }
bool ProtodishWorld::is_extinct() const { return world_->extinct_at().has_value(); }
int64_t ProtodishWorld::get_extinct_at() const {
    return world_->extinct_at() ? static_cast<int64_t>(*world_->extinct_at()) : -1;
}
int64_t ProtodishWorld::get_width() const { return world_->params().grid_width; }
int64_t ProtodishWorld::get_height() const { return world_->params().grid_height; }

String ProtodishWorld::get_state_hash() const {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(world_->state_hash()));
    return String(buf);
}

static_assert(ProtodishWorld::VIEW_MODE_COUNT == evo::kViewModeCount, "view modes out of step with the core");

Ref<Image> ProtodishWorld::render(int64_t mode) {
    const auto m = (mode >= 0 && mode < evo::kViewModeCount) ? static_cast<evo::ViewMode>(mode) : evo::ViewMode::Lineage;
    evo::paint_view(*world_, m, pixels_.ptrw());
    return Image::create_from_data(static_cast<int32_t>(get_width()), static_cast<int32_t>(get_height()),
                                   false, Image::FORMAT_RGB8, pixels_);
}

PackedStringArray ProtodishWorld::get_view_mode_names() const {
    PackedStringArray names;
    for (int m = 0; m < evo::kViewModeCount; ++m) names.push_back(evo::view_mode_name(static_cast<evo::ViewMode>(m)));
    return names;
}

void ProtodishWorld::_bind_methods() {
    ClassDB::bind_method(D_METHOD("reset", "seed"), &ProtodishWorld::reset);
    ClassDB::bind_method(D_METHOD("step", "n"), &ProtodishWorld::step);
    ClassDB::bind_method(D_METHOD("get_tick"), &ProtodishWorld::get_tick);
    ClassDB::bind_method(D_METHOD("get_seed"), &ProtodishWorld::get_seed);
    ClassDB::bind_method(D_METHOD("get_cell_count"), &ProtodishWorld::get_cell_count);
    ClassDB::bind_method(D_METHOD("is_extinct"), &ProtodishWorld::is_extinct);
    ClassDB::bind_method(D_METHOD("get_extinct_at"), &ProtodishWorld::get_extinct_at);
    ClassDB::bind_method(D_METHOD("get_width"), &ProtodishWorld::get_width);
    ClassDB::bind_method(D_METHOD("get_height"), &ProtodishWorld::get_height);
    ClassDB::bind_method(D_METHOD("get_state_hash"), &ProtodishWorld::get_state_hash);
    ClassDB::bind_method(D_METHOD("render", "mode"), &ProtodishWorld::render);
    ClassDB::bind_method(D_METHOD("get_view_mode_names"), &ProtodishWorld::get_view_mode_names);
    BIND_ENUM_CONSTANT(VIEW_LINEAGE);
    BIND_ENUM_CONSTANT(VIEW_ENERGY);
    BIND_ENUM_CONSTANT(VIEW_FEEDING);
    BIND_ENUM_CONSTANT(VIEW_INFECTION);
    BIND_ENUM_CONSTANT(VIEW_GROUND);
    BIND_ENUM_CONSTANT(VIEW_MODE_COUNT);
}

}  // namespace godot
