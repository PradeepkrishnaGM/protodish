#include <fstream>
#include <sstream>

#include "doctest.h"
#include "params.hpp"
#include "world.hpp"

namespace {

std::string apply_text(evo::Params& p, const std::string& text) {
    std::istringstream in(text);
    return evo::apply_params(p, in);
}

std::string dump(const evo::Params& p) {
    std::ostringstream out;
    evo::write_params(p, out);
    return out.str();
}

std::uint64_t hash_after(const evo::Params& p, int ticks) {
    evo::World w(p, 77);
    for (int t = 0; t < ticks; ++t) w.step();
    return w.state_hash();
}

}  // namespace

TEST_CASE("params: key = value overrides each field type and ancestor genes") {
    evo::Params p;
    const auto err = apply_text(p,
                           "# a comment\n"
                           "\n"
                           "  spark_base = 500   # trailing comment\n"
                           "initial_cells=80\n"
                           "divide_cooldown = 7\n"
                           "ancestor.preferred_temp = 18.5\r\n"
                           "ancestor.tag = 0.25\n");
    CHECK(err.empty());
    CHECK(p.spark_base == 500.0);
    CHECK(p.initial_cells == 80);
    CHECK(p.divide_cooldown == 7u);
    CHECK(p.ancestor[evo::kPreferredTemp] == 18.5);
    CHECK(p.ancestor[evo::kTag] == 0.25);
    CHECK(p.spoilage_rate == evo::Params{}.spoilage_rate);  // untouched
}

TEST_CASE("params: bad input is rejected with the line number") {
    evo::Params p;
    CHECK(apply_text(p, "no_such_key = 1\n").find("line 1: unknown key") != std::string::npos);
    CHECK(apply_text(p, "\nspark_base 500\n").find("line 2: expected key = value") != std::string::npos);
    CHECK(apply_text(p, "spark_base = lots\n").find("bad value") != std::string::npos);
    CHECK(apply_text(p, "spark_base = 5 0\n").find("bad value") != std::string::npos);
    CHECK(apply_text(p, "spark_base = nan\n").find("bad value") != std::string::npos);
    CHECK(apply_text(p, "initial_cells = 2.5\n").find("bad value") != std::string::npos);
    CHECK(apply_text(p, "divide_cooldown = -1\n").find("bad value") != std::string::npos);
    CHECK(apply_text(p, "ancestor.harvest = 1.5\n").find("bad value") != std::string::npos);
    CHECK(apply_text(p, "ancestor.tag = 1\n").find("bad value") != std::string::npos);
    CHECK(apply_text(p, "ancestor.wings = 1\n").find("unknown key") != std::string::npos);
    CHECK(apply_text(p, "spark_base = 1\nspark_base = 2\n").find("line 2: duplicate") !=
          std::string::npos);
}

TEST_CASE("params: write_params output reads back to identical values") {
    evo::Params p;
    p.spoilage_rate = 0.0123456789;
    p.ancestor[evo::kDiet] = 0.1;
    evo::Params q;
    CHECK(apply_text(q, dump(p)).empty());
    CHECK(dump(q) == dump(p));
}

TEST_CASE("params: validation catches values the engine cannot run with") {
    evo::Params p;
    CHECK(evo::validate_params(p).empty());
    p.clone_cost = 11.0;
    CHECK_FALSE(evo::validate_params(p).empty());
    CHECK_THROWS_AS(evo::World(p, 1), std::invalid_argument);
    p = evo::Params{};
    p.disaster_size = 200;
    CHECK_FALSE(evo::validate_params(p).empty());
}

TEST_CASE("params: no file gives the default hash; a file changes the run") {
    const evo::Params defaults;
    const std::uint64_t base = hash_after(defaults, 300);

    // An empty file, or one restating every default, leaves the run unchanged.
    evo::Params empty;
    CHECK(apply_text(empty, "").empty());
    CHECK(hash_after(empty, 300) == base);
    evo::Params restated;
    CHECK(apply_text(restated, dump(defaults)).empty());
    CHECK(hash_after(restated, 300) == base);

    // The example file changes it.
    evo::Params changed;
    CHECK(evo::load_params_file(changed, EVO_TEST_DATA_DIR "/example.params").empty());
    CHECK(changed.spark_base == 500.0);
    CHECK(changed.initial_cells == 80);
    CHECK(changed.ancestor[evo::kPreferredTemp] == 18.0);
    CHECK(hash_after(changed, 300) != base);
}

TEST_CASE("params: a missing file is an error") {
    evo::Params p;
    CHECK(evo::load_params_file(p, EVO_TEST_DATA_DIR "/does_not_exist.params")
              .find("cannot open") != std::string::npos);
}
