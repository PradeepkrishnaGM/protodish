#include "protodish_world.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>

#include "records.hpp"

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
    history_.clear();
    history_.record(*world_);
    tracker_.clear();
    return String();
}

int64_t ProtodishWorld::step(int64_t n) {
    int64_t ran = 0;
    while (ran < n && ended_at_ < 0 && !world_->extinct_at()) {
        world_->step();
        history_.record(*world_);
        tracker_.update(*world_);
        ++ran;
    }
    return ran;
}

void ProtodishWorld::end_world() {
    if (ended_at_ >= 0) return;
    world_->clear_cells();
    tracker_.removed(*world_);
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

Dictionary ProtodishWorld::get_stats() const {
    const evo::Census c = evo::take_census(*world_, /*with_hash=*/false);
    const evo::Params& p = world_->params();
    double t_min = world_->row_temperature(0), t_max = t_min;
    double l_min = world_->row_light(0), l_max = l_min;
    for (int row = 1; row < p.grid_height; ++row) {
        t_min = std::min(t_min, world_->row_temperature(row));
        t_max = std::max(t_max, world_->row_temperature(row));
        l_min = std::min(l_min, world_->row_light(row));
        l_max = std::max(l_max, world_->row_light(row));
    }
    Dictionary d;
    d["tick"] = static_cast<int64_t>(c.tick);
    d["year_length"] = p.year_length;
    d["season"] = world_->season();  // of the tick just run (tick 0 before the first)
    d["temp_min"] = t_min;
    d["temp_max"] = t_max;
    d["light_min"] = l_min;
    d["light_max"] = l_max;
    d["cells"] = c.cells;
    d["free_cells"] = c.free_cells;
    d["body_cells"] = c.body_cells;
    d["bodies"] = c.bodies;
    d["largest_body"] = c.largest_body;
    d["clusters"] = c.clusters.all;
    d["clusters_large"] = c.clusters.large;
    d["cluster_min_size"] = p.cluster_min_size;
    d["producers"] = c.producers;
    d["consumers"] = c.consumers;
    d["infected"] = c.infected;
    d["matter_cells"] = c.matter.cells;
    d["matter_food_a"] = c.matter.food_a;
    d["matter_food_b"] = c.matter.food_b;
    d["matter_minerals"] = c.matter.minerals;
    d["matter_total"] = c.matter.total();
    return d;
}

Dictionary ProtodishWorld::get_history(int64_t max_points) const { return get_history_since(max_points, 0); }

Dictionary ProtodishWorld::get_history_since(int64_t max_points, int64_t from_tick) const {
    const auto s = history_.downsample(static_cast<std::size_t>(std::max<int64_t>(max_points, 1)),
                                       static_cast<uint64_t>(std::max<int64_t>(from_tick, 0)));
    PackedInt64Array ticks;
    PackedInt32Array cells, producers, infected;
    for (std::size_t k = 0; k < s.tick.size(); ++k) {
        ticks.push_back(static_cast<int64_t>(s.tick[k]));
        cells.push_back(s.cells[k]);
        producers.push_back(s.producers[k]);
        infected.push_back(s.infected[k]);
    }
    Dictionary d;
    d["ticks"] = ticks;
    d["cells"] = cells;
    d["producers"] = producers;
    d["infected"] = infected;
    return d;
}

Ref<Image> ProtodishWorld::render_temperature_strip() const {
    const int h = world_->params().grid_height;
    PackedByteArray px;
    px.resize(h * 3);
    for (int row = 0; row < h; ++row) {
        const evo::Rgb c = evo::temperature_color(world_->row_temperature(row), world_->params());
        for (int k = 0; k < 3; ++k) px[row * 3 + k] = c[static_cast<std::size_t>(k)];
    }
    return Image::create_from_data(1, h, false, Image::FORMAT_RGB8, px);
}

bool ProtodishWorld::select_site(int64_t site) {
    tracker_.select(*world_, static_cast<int>(site));
    return tracker_.state() == evo::CellTracker::State::Alive;
}

void ProtodishWorld::clear_selection() { tracker_.clear(); }

int64_t ProtodishWorld::get_selected_site() const {
    return tracker_.state() == evo::CellTracker::State::Alive ? tracker_.site() : -1;
}

Dictionary ProtodishWorld::inspect_cell() const {
    using State = evo::CellTracker::State;
    Dictionary d;
    const State st = tracker_.state();
    d["state"] = st == State::Alive ? "alive" : st == State::Dead ? "dead" : st == State::Removed ? "removed" : "none";
    if (st == State::None) return d;
    const int site = tracker_.site();
    const int w = world_->params().grid_width;
    d["id"] = static_cast<int64_t>(tracker_.id());
    d["site"] = site;
    d["row"] = site / w;
    d["col"] = site % w;
    d["death_tick"] = static_cast<int64_t>(tracker_.death_tick());
    d["death_cause"] = st == State::Dead ? evo::death_cause_name(tracker_.death_cause()) : "";
    if (st != State::Alive) return d;

    const evo::CellArrays& c = world_->cells();
    const auto i = static_cast<std::size_t>(site);
    Array genes;
    for (std::size_t k = 0; k < evo::kGeneCount; ++k) {
        Dictionary g;
        g["name"] = evo::kGeneInfo[k].name;
        g["value"] = c.genes[k][i];
        g["min"] = evo::kGeneInfo[k].min;
        g["max"] = evo::kGeneInfo[k].max;
        genes.push_back(g);
    }
    bool newborn = false;
    for (const evo::BirthEvent& b : world_->births()) newborn = newborn || b.id == tracker_.id();
    int bond_count = 0;
    for (int k = 0; k < 8; ++k) bond_count += (c.bonds[i] >> k) & 1;
    d["genes"] = genes;
    d["parent_id"] = static_cast<int64_t>(c.parent_id[i]);
    d["parent2_id"] = static_cast<int64_t>(c.parent2_id[i]);
    d["store_a"] = c.store_a[i];
    d["store_b"] = c.store_b[i];
    d["store_max"] = world_->params().store_max;
    d["age"] = static_cast<int64_t>(c.age[i]);
    d["cooldown"] = static_cast<int64_t>(c.cooldown[i]);
    d["stress"] = c.stress[i];
    d["infected"] = c.infected[i] != 0;
    d["virus_tag"] = c.virus_tag[i];
    d["newborn"] = newborn;  // born in the tick just run, so not yet sensed
    d["dormant"] = !newborn && !c.awake[i] && world_->tick() > 0;
    d["thermal_eff"] = c.thermal_eff[i];
    d["supply"] = c.supply[i];
    d["bonds"] = bond_count;
    d["inner"] = world_->is_inner(site);
    d["body_size"] = evo::body_size(*world_, site);
    d["producer"] = c.genes[evo::kPhotosynthesis][i] > c.genes[evo::kHarvest][i];
    return d;
}

Dictionary ProtodishWorld::inspect_site(int64_t site) const {
    Dictionary d;
    if (site < 0 || site >= world_->site_count()) return d;
    const auto i = static_cast<std::size_t>(site);
    const int s = static_cast<int>(site);
    d["row"] = s / world_->params().grid_width;
    d["col"] = s % world_->params().grid_width;
    d["food_a"] = world_->food_a()[i];
    d["food_b"] = world_->food_b()[i];
    d["minerals"] = world_->minerals()[i];
    d["temperature"] = world_->site_temperature(s);
    d["light"] = world_->site_light(s);
    d["occupied"] = world_->cells().alive[i] != 0;
    return d;
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
    ClassDB::bind_method(D_METHOD("get_stats"), &ProtodishWorld::get_stats);
    ClassDB::bind_method(D_METHOD("get_history", "max_points"), &ProtodishWorld::get_history);
    ClassDB::bind_method(D_METHOD("get_history_since", "max_points", "from_tick"), &ProtodishWorld::get_history_since);
    ClassDB::bind_method(D_METHOD("render_temperature_strip"), &ProtodishWorld::render_temperature_strip);
    ClassDB::bind_method(D_METHOD("select_site", "site"), &ProtodishWorld::select_site);
    ClassDB::bind_method(D_METHOD("clear_selection"), &ProtodishWorld::clear_selection);
    ClassDB::bind_method(D_METHOD("get_selected_site"), &ProtodishWorld::get_selected_site);
    ClassDB::bind_method(D_METHOD("inspect_cell"), &ProtodishWorld::inspect_cell);
    ClassDB::bind_method(D_METHOD("inspect_site", "site"), &ProtodishWorld::inspect_site);
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
