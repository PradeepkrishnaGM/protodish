// Headless runner: runs N ticks from a seed and writes a summary CSV.
// World totals and population only; the census CSV and lineage log arrive in M6.
// Each row is the state at the start of tick T, with the season of tick T.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "world.hpp"

namespace {

void usage() {
    std::fprintf(stderr,
                 "usage: evolve [--seed N] [--ticks N] [--every N] [--out FILE]\n"
                 "  --seed   RNG seed (default 1)\n"
                 "  --ticks  ticks to run (default 1000)\n"
                 "  --every  summary interval in ticks (default 100)\n"
                 "  --out    summary CSV path (default stdout)\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::uint64_t seed = 1;
    std::uint64_t ticks = 1000;
    std::uint64_t every = 100;
    std::string out_path;

    for (int i = 1; i < argc; ++i) {
        const bool has_value = i + 1 < argc;
        if (std::strcmp(argv[i], "--seed") == 0 && has_value) {
            seed = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--ticks") == 0 && has_value) {
            ticks = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--every") == 0 && has_value) {
            every = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--out") == 0 && has_value) {
            out_path = argv[++i];
        } else {
            usage();
            return 2;
        }
    }
    if (every == 0) every = 1;

    std::FILE* out = stdout;
    if (!out_path.empty()) {
        out = std::fopen(out_path.c_str(), "w");
        if (!out) {
            std::perror(out_path.c_str());
            return 1;
        }
    }

    evo::World world(evo::Params{}, seed);
    std::fprintf(out,
                 "tick,season,cells,births,deaths,food_a,food_b,minerals,in_cells,total,hash\n");
    std::uint64_t births = 0, deaths = 0;  // since the previous row
    auto write_row = [&] {
        const evo::MatterTotals m = world.matter();
        std::fprintf(out, "%llu,%.6f,%d,%llu,%llu,%.6f,%.6f,%.6f,%.6f,%.6f,%016llx\n",
                     static_cast<unsigned long long>(world.tick()),
                     world.climate().season(world.tick()), world.cell_count(),
                     static_cast<unsigned long long>(births),
                     static_cast<unsigned long long>(deaths), m.food_a, m.food_b, m.minerals,
                     m.cells, m.total(), static_cast<unsigned long long>(world.state_hash()));
        births = deaths = 0;
    };

    const auto start = std::chrono::steady_clock::now();
    write_row();
    while (world.tick() < ticks) {
        world.step();
        births += world.births().size();
        deaths += world.deaths().size();
        if (world.tick() % every == 0 || world.tick() == ticks) write_row();
    }
    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    if (out != stdout) std::fclose(out);
    if (world.extinct_at()) {
        std::fprintf(stderr, "extinct: last cell died in tick %llu\n",
                     static_cast<unsigned long long>(*world.extinct_at()));
    }
    std::fprintf(stderr, "seed %llu: %llu ticks in %.3f s (%.0f ticks/s)\n",
                 static_cast<unsigned long long>(seed), static_cast<unsigned long long>(ticks),
                 secs, secs > 0 ? static_cast<double>(ticks) / secs : 0.0);
    return 0;
}
