// World-view colors for the app (M7b).

#include <vector>

#include "doctest.h"
#include "lab.hpp"
#include "view.hpp"

using lab::at;

namespace {

evo::Rgb pixel(const evo::World& w, evo::ViewMode mode, int site) {
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(w.site_count()) * 3);
    evo::paint_view(w, mode, buf.data());
    const auto i = static_cast<std::size_t>(site) * 3;
    return {buf[i], buf[i + 1], buf[i + 2]};
}

int brightness(const evo::Rgb& c) { return c[0] + c[1] + c[2]; }

}  // namespace

TEST_CASE("view: empty sites use the background in every cell mode") {
    const evo::Params p = lab::params();
    evo::World w(p, 1);
    for (int m = 0; m < evo::kViewModeCount; ++m) {
        const auto mode = static_cast<evo::ViewMode>(m);
        if (mode == evo::ViewMode::Ground) continue;
        CHECK(pixel(w, mode, at(32, 5)) == evo::view::kEmpty);
    }
}

TEST_CASE("view: lineage colors follow the tag") {
    const evo::Params p = lab::params();
    evo::World w(p, 1);
    evo::Genome g = lab::still(p);
    g[evo::kTag] = 0.0;  // red
    REQUIRE(w.add_cell(at(32, 5), g, 10, 10, 10));
    g[evo::kTag] = 1.0 / 3.0;  // green
    REQUIRE(w.add_cell(at(32, 9), g, 10, 10, 10));
    const evo::Rgb red = pixel(w, evo::ViewMode::Lineage, at(32, 5));
    const evo::Rgb green = pixel(w, evo::ViewMode::Lineage, at(32, 9));
    CHECK(red[0] > 200);
    CHECK(red[1] < 60);
    CHECK(green[1] > 200);
    CHECK(green[0] < 60);
    CHECK(evo::tag_color(0.98) != evo::tag_color(0.5));
    CHECK(evo::tag_color(1.0) == evo::tag_color(0.0));  // the tag is circular
}

TEST_CASE("view: energy gets brighter with stores and saturates at store_max") {
    const evo::Params p = lab::params();
    evo::World w(p, 1);
    const evo::Genome g = lab::still(p);
    REQUIRE(w.add_cell(at(32, 5), g, 2, 2, 10));
    REQUIRE(w.add_cell(at(32, 9), g, 12, 12, 10));
    REQUIRE(w.add_cell(at(32, 13), g, 25, 25, 10));
    REQUIRE(w.add_cell(at(32, 17), g, 50, 50, 10));
    const auto e = [&](int col) { return pixel(w, evo::ViewMode::Energy, at(32, col)); };
    CHECK(brightness(e(5)) < brightness(e(9)));
    CHECK(brightness(e(9)) < brightness(e(13)));
    CHECK(e(13) == e(17));  // A + B = store_max is already full
    CHECK(e(17) == evo::energy_color(1.0));
}

TEST_CASE("view: feeding type uses the RULES.md gene test") {
    const evo::Params p = lab::params();
    evo::World w(p, 1);
    evo::Genome g = lab::still(p);
    g[evo::kHarvest] = 0.3;
    g[evo::kPhotosynthesis] = 0.4;
    REQUIRE(w.add_cell(at(32, 5), g, 10, 10, 10));
    g[evo::kPhotosynthesis] = 0.3;  // equal is not higher, so a consumer
    REQUIRE(w.add_cell(at(32, 9), g, 10, 10, 10));
    CHECK(pixel(w, evo::ViewMode::Feeding, at(32, 5)) == evo::view::kProducer);
    CHECK(pixel(w, evo::ViewMode::Feeding, at(32, 9)) == evo::view::kConsumer);
}

TEST_CASE("view: infection shows the virus tag; healthy cells are grey") {
    const evo::Params p = lab::params();
    evo::World w(p, 1);
    const evo::Genome g = lab::still(p);
    REQUIRE(w.add_cell(at(32, 5), g, 10, 10, 10));
    REQUIRE(w.add_cell(at(32, 9), g, 10, 10, 10));
    REQUIRE(w.add_cell(at(32, 13), g, 10, 10, 10));
    w.set_cell_virus(at(32, 9), 0.5);
    w.set_cell_virus(at(32, 13), 0.1);
    CHECK(pixel(w, evo::ViewMode::Infection, at(32, 5)) == evo::view::kHealthy);
    CHECK(pixel(w, evo::ViewMode::Infection, at(32, 9)) == evo::tag_color(0.5));
    CHECK(pixel(w, evo::ViewMode::Infection, at(32, 13)) == evo::tag_color(0.1));
}

TEST_CASE("view: ground maps A, B and minerals to red, green and blue, ignoring cells") {
    const evo::Params p = lab::params();
    evo::World w(p, 1);
    lab::clear_ground(w);
    w.set_site(at(32, 5), 3.0, 0.0, 0.0);
    w.set_site(at(32, 9), 0.0, 3.0, 0.0);
    w.set_site(at(32, 13), 0.0, 0.0, 12.5);
    w.set_site(at(32, 17), 100.0, 100.0, 1000.0);
    REQUIRE(w.add_cell(at(32, 21), lab::still(p), 10, 10, 10));
    const auto gr = [&](int col) { return pixel(w, evo::ViewMode::Ground, at(32, col)); };
    CHECK(gr(5) == evo::Rgb{128, 0, 0});  // sqrt(3 / 12) = 0.5
    CHECK(gr(9) == evo::Rgb{0, 128, 0});
    CHECK(gr(13) == evo::Rgb{0, 0, 128});  // sqrt(12.5 / 50) = 0.5
    CHECK(gr(17) == evo::Rgb{255, 255, 255});
    CHECK(gr(21) == evo::Rgb{0, 0, 0});  // the cell is not drawn
}

TEST_CASE("view: painting does not change the world") {
    const evo::Params p;
    evo::World w(p, 4);
    for (int t = 0; t < 200; ++t) w.step();
    const auto before = w.state_hash();
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(w.site_count()) * 3);
    for (int m = 0; m < evo::kViewModeCount; ++m) evo::paint_view(w, static_cast<evo::ViewMode>(m), buf.data());
    CHECK(w.state_hash() == before);
}
