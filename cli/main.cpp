// Headless runner: runs N ticks from a seed, writes the census CSV and, optionally, the
// binary lineage log (RULES.md "Lineage record"; formats in core/records.hpp).
// Each census row is the state at the start of tick T, with the season of tick T.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "records.hpp"
#include "world.hpp"

namespace {

void usage() {
    std::fprintf(stderr,
                 "usage: evolve [--seed N] [--ticks N] [--every N] [--census FILE]\n"
                 "              [--lineage FILE] [--params FILE] [--dump-params]\n"
                 "  --seed         RNG seed (default 1)\n"
                 "  --ticks        ticks to run (default 1000); the run ends early at extinction\n"
                 "  --every        census interval in ticks (default: census_interval, 100)\n"
                 "  --census       census CSV path (default stdout); --out is the same\n"
                 "  --lineage      write the binary lineage log to FILE (default: none)\n"
                 "  --params       key = value file overriding Params (defaults: RULES.md)\n"
                 "  --dump-params  print the effective params in the same format and exit\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::uint64_t seed = 1;
    std::uint64_t ticks = 1000;
    std::uint64_t every = 0;
    std::string census_path;
    std::string lineage_path;
    std::string params_path;
    bool dump_params = false;

    for (int i = 1; i < argc; ++i) {
        const bool has_value = i + 1 < argc;
        if (std::strcmp(argv[i], "--seed") == 0 && has_value) {
            seed = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--ticks") == 0 && has_value) {
            ticks = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--every") == 0 && has_value) {
            every = std::strtoull(argv[++i], nullptr, 10);
        } else if ((std::strcmp(argv[i], "--census") == 0 || std::strcmp(argv[i], "--out") == 0) &&
                   has_value) {
            census_path = argv[++i];
        } else if (std::strcmp(argv[i], "--lineage") == 0 && has_value) {
            lineage_path = argv[++i];
        } else if (std::strcmp(argv[i], "--params") == 0 && has_value) {
            params_path = argv[++i];
        } else if (std::strcmp(argv[i], "--dump-params") == 0) {
            dump_params = true;
        } else {
            usage();
            return 2;
        }
    }

    evo::Params params;
    if (!params_path.empty()) {
        if (const std::string err = evo::load_params_file(params, params_path); !err.empty()) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 2;
        }
    }
    if (const std::string err = evo::validate_params(params); !err.empty()) {
        std::fprintf(stderr, "error: invalid params: %s\n", err.c_str());
        return 2;
    }
    if (dump_params) {
        evo::write_params(params, std::cout);
        return 0;
    }
    if (every == 0) every = static_cast<std::uint64_t>(params.census_interval);

    std::FILE* out = stdout;
    if (!census_path.empty()) {
        out = std::fopen(census_path.c_str(), "w");
        if (!out) {
            std::perror(census_path.c_str());
            return 1;
        }
    }

    evo::World world(params, seed);
    evo::LineageWriter lineage;
    if (!lineage_path.empty()) {
        if (const std::string err = lineage.open(lineage_path, world); !err.empty()) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
    }

    evo::write_census_header(out);
    evo::IntervalCounts counts;
    counts.peak_cells = world.cell_count();
    auto write_row = [&] {
        evo::write_census_row(out, evo::take_census(world), counts);
        counts.reset();
    };

    const auto start = std::chrono::steady_clock::now();
    write_row();
    while (world.tick() < ticks) {
        world.step();
        counts.add_tick(world);
        lineage.add_tick(world);
        if (world.extinct_at()) {  // extinction ends the run (RULES.md open question 8)
            write_row();
            break;
        }
        if (world.tick() % every == 0 || world.tick() == ticks) write_row();
    }
    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    if (out != stdout && std::fclose(out) != 0) {
        std::perror(census_path.c_str());
        return 1;
    }
    if (const std::string err = lineage.close(); !err.empty()) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    if (world.extinct_at()) {
        std::fprintf(stderr, "extinct: last cell died in tick %llu\n",
                     static_cast<unsigned long long>(*world.extinct_at()));
    }
    const auto ran = static_cast<double>(world.tick());
    std::fprintf(stderr, "seed %llu: %llu ticks in %.3f s (%.0f ticks/s)\n",
                 static_cast<unsigned long long>(seed), static_cast<unsigned long long>(world.tick()),
                 secs, secs > 0 ? ran / secs : 0.0);
    if (!lineage_path.empty()) {
        std::fprintf(stderr, "lineage: %.1f MB\n", static_cast<double>(lineage.bytes_written()) / 1e6);
    }
    return 0;
}
