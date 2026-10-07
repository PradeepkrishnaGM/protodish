#include "protodish_world.hpp"

#include <cmath>
#include <cstdio>

#include <godot_cpp/core/class_db.hpp>

namespace godot {

namespace {

constexpr uint8_t kEmpty[3] = {16, 20, 24};  // background of an empty site

// Hue in [0, 1), full saturation, value v; writes 3 bytes.
void hsv_to_rgb(double h, double s, double v, uint8_t* out) {
    h = (h - std::floor(h)) * 6.0;
    const int i = static_cast<int>(h) % 6;
    const double f = h - std::floor(h);
    const double p = v * (1.0 - s);
    const double q = v * (1.0 - s * f);
    const double t = v * (1.0 - s * (1.0 - f));
    double r = v, g = t, b = p;
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    out[0] = static_cast<uint8_t>(std::lround(r * 255.0));
    out[1] = static_cast<uint8_t>(std::lround(g * 255.0));
    out[2] = static_cast<uint8_t>(std::lround(b * 255.0));
}

}  // namespace

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

Ref<Image> ProtodishWorld::render(int64_t mode) {
    (void)mode;  // only VIEW_LINEAGE so far
    const evo::CellArrays& c = world_->cells();
    const auto& tag = c.genes[evo::kTag];
    uint8_t* px = pixels_.ptrw();
    const int n = world_->site_count();
    for (int s = 0; s < n; ++s) {
        uint8_t* out = px + static_cast<std::ptrdiff_t>(s) * 3;
        const auto i = static_cast<std::size_t>(s);
        if (!c.alive[i]) {
            out[0] = kEmpty[0];
            out[1] = kEmpty[1];
            out[2] = kEmpty[2];
            continue;
        }
        hsv_to_rgb(tag[i], 0.85, 0.95, out);
    }
    return Image::create_from_data(static_cast<int32_t>(get_width()), static_cast<int32_t>(get_height()),
                                   false, Image::FORMAT_RGB8, pixels_);
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
    BIND_ENUM_CONSTANT(VIEW_LINEAGE);
}

}  // namespace godot
