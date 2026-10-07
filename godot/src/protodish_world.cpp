#include "protodish_world.hpp"

#include <cstdio>
#include <sstream>
#include <string>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>

namespace godot {

namespace {

evo::ViewMode to_mode(int64_t mode) {
    return (mode >= 0 && mode < evo::kViewModeCount) ? static_cast<evo::ViewMode>(mode) : evo::ViewMode::Lineage;
}

}  // namespace

static_assert(ProtodishWorld::VIEW_MODE_COUNT == evo::kViewModeCount, "view modes out of step with the core");

ProtodishWorld::ProtodishWorld() { reset(1); }
ProtodishWorld::~ProtodishWorld() = default;

String ProtodishWorld::reset(int64_t seed, const String& params_text) {
    evo::Params params;  // RULES.md defaults
    std::istringstream in(params_text.utf8().get_data());
    std::string err = evo::apply_params(params, in);
    if (err.empty()) err = evo::validate_params(params);
    if (!err.empty()) return String::utf8(err.c_str());  // validated, so World below cannot throw
    world_ = std::make_unique<evo::World>(params, static_cast<uint64_t>(seed));
    pixels_.resize(static_cast<int64_t>(world_->site_count()) * 3);
    ended_at_ = -1;
    return String();
}

int64_t ProtodishWorld::step(int64_t n) {
    int64_t ran = 0;
    while (ran < n && ended_at_ < 0 && !world_->extinct_at()) {
        world_->step();
        ++ran;
    }
    return ran;
}

void ProtodishWorld::end_world() {
    if (ended_at_ >= 0) return;
    world_->clear_cells();
    ended_at_ = static_cast<int64_t>(world_->tick());
}

int64_t ProtodishWorld::get_tick() const { return static_cast<int64_t>(world_->tick()); }
int64_t ProtodishWorld::get_seed() const { return static_cast<int64_t>(world_->seed()); }
int64_t ProtodishWorld::get_cell_count() const { return world_->cell_count(); }
bool ProtodishWorld::is_extinct() const { return world_->extinct_at().has_value(); }
int64_t ProtodishWorld::get_extinct_at() const {
    return world_->extinct_at() ? static_cast<int64_t>(*world_->extinct_at()) : -1;
}
bool ProtodishWorld::is_ended() const { return ended_at_ >= 0; }
int64_t ProtodishWorld::get_ended_at() const { return ended_at_; }
int64_t ProtodishWorld::get_width() const { return world_->params().grid_width; }
int64_t ProtodishWorld::get_height() const { return world_->params().grid_height; }
double ProtodishWorld::get_total_matter() const { return world_->matter().total(); }

String ProtodishWorld::get_state_hash() const {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(world_->state_hash()));
    return String(buf);
}

Ref<Image> ProtodishWorld::render(int64_t mode) {
    evo::paint_view(*world_, to_mode(mode), pixels_.ptrw());
    return Image::create_from_data(static_cast<int32_t>(get_width()), static_cast<int32_t>(get_height()),
                                   false, Image::FORMAT_RGB8, pixels_);
}

PackedStringArray ProtodishWorld::get_view_mode_names() const {
    PackedStringArray names;
    for (int m = 0; m < evo::kViewModeCount; ++m) names.push_back(evo::view_mode_name(static_cast<evo::ViewMode>(m)));
    return names;
}

Array ProtodishWorld::get_legend(int64_t mode) const {
    Array out;
    for (const evo::LegendItem& item : evo::view_legend(to_mode(mode), world_->params())) {
        PackedColorArray colors;
        for (const evo::Rgb& c : item.colors) colors.push_back(Color::from_rgba8(c[0], c[1], c[2]));
        Dictionary d;
        d["label"] = String::utf8(item.label.c_str());
        d["colors"] = colors;
        d["low"] = String::utf8(item.low.c_str());
        d["high"] = String::utf8(item.high.c_str());
        out.push_back(d);
    }
    return out;
}

void ProtodishWorld::_bind_methods() {
    ClassDB::bind_method(D_METHOD("reset", "seed", "params_text"), &ProtodishWorld::reset, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("step", "n"), &ProtodishWorld::step);
    ClassDB::bind_method(D_METHOD("end_world"), &ProtodishWorld::end_world);
    ClassDB::bind_method(D_METHOD("get_tick"), &ProtodishWorld::get_tick);
    ClassDB::bind_method(D_METHOD("get_seed"), &ProtodishWorld::get_seed);
    ClassDB::bind_method(D_METHOD("get_cell_count"), &ProtodishWorld::get_cell_count);
    ClassDB::bind_method(D_METHOD("is_extinct"), &ProtodishWorld::is_extinct);
    ClassDB::bind_method(D_METHOD("get_extinct_at"), &ProtodishWorld::get_extinct_at);
    ClassDB::bind_method(D_METHOD("is_ended"), &ProtodishWorld::is_ended);
    ClassDB::bind_method(D_METHOD("get_ended_at"), &ProtodishWorld::get_ended_at);
    ClassDB::bind_method(D_METHOD("get_width"), &ProtodishWorld::get_width);
    ClassDB::bind_method(D_METHOD("get_height"), &ProtodishWorld::get_height);
    ClassDB::bind_method(D_METHOD("get_state_hash"), &ProtodishWorld::get_state_hash);
    ClassDB::bind_method(D_METHOD("get_total_matter"), &ProtodishWorld::get_total_matter);
    ClassDB::bind_method(D_METHOD("render", "mode"), &ProtodishWorld::render);
    ClassDB::bind_method(D_METHOD("get_view_mode_names"), &ProtodishWorld::get_view_mode_names);
    ClassDB::bind_method(D_METHOD("get_legend", "mode"), &ProtodishWorld::get_legend);
    BIND_ENUM_CONSTANT(VIEW_LINEAGE);
    BIND_ENUM_CONSTANT(VIEW_ENERGY);
    BIND_ENUM_CONSTANT(VIEW_FEEDING);
    BIND_ENUM_CONSTANT(VIEW_INFECTION);
    BIND_ENUM_CONSTANT(VIEW_GROUND);
    BIND_ENUM_CONSTANT(VIEW_GROUND_FOOD_A);
    BIND_ENUM_CONSTANT(VIEW_GROUND_FOOD_B);
    BIND_ENUM_CONSTANT(VIEW_GROUND_MINERALS);
    BIND_ENUM_CONSTANT(VIEW_MODE_COUNT);
}

}  // namespace godot
