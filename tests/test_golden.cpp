// Golden state hashes, recorded before the M6 speed work (tests/data/golden_hashes.txt).
// Any optimisation must leave every one of them unchanged. A change to the rules changes
// them on purpose; then the file is regenerated and the reason recorded in DECISIONS.md.

#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "doctest.h"
#include "world.hpp"

namespace {

struct Checkpoint {
    std::uint64_t tick;
    std::uint64_t hash;
};

using Key = std::pair<std::string, std::uint64_t>;  // world, seed

std::map<Key, std::vector<Checkpoint>> load_golden() {
    std::map<Key, std::vector<Checkpoint>> out;
    std::ifstream in(EVO_TEST_DATA_DIR "/golden_hashes.txt");
    REQUIRE(in);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::string world, hash_hex;
        std::uint64_t seed = 0, tick = 0;
        ls >> world >> seed >> tick >> hash_hex;
        REQUIRE(ls);
        out[{world, seed}].push_back({tick, std::stoull(hash_hex, nullptr, 16)});
    }
    return out;
}

evo::Params world_params(const std::string& world) {
    evo::Params p;
    if (world == "mild") {
        REQUIRE(evo::load_params_file(p, EVO_TEST_DATA_DIR "/mild.params").empty());
    } else {
        REQUIRE(world == "default");
    }
    return p;
}

}  // namespace

TEST_CASE("golden: state hashes match the recorded history" * doctest::test_suite("slow")) {
    const auto golden = load_golden();
    REQUIRE(golden.size() == 6);
    for (const auto& [key, checkpoints] : golden) {
        CAPTURE(key.first);
        CAPTURE(key.second);
        evo::World w(world_params(key.first), key.second);
        for (const Checkpoint& c : checkpoints) {
            while (w.tick() < c.tick) w.step();
            CAPTURE(c.tick);
            CHECK(w.state_hash() == c.hash);
        }
    }
}
