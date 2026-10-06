#pragma once

// Records from RULES.md "Lineage record": the census and the binary lineage log.
// The World only collects each tick's births and deaths; these classes turn them into
// files. Formats are described in DECISIONS.md (M6).

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "world.hpp"

namespace evo {

// Body-size buckets of the census: 2, 3-4, 5-8, 9-16, 17-32, 33 or more cells.
inline constexpr int kBodyBuckets = 6;
int body_bucket(int size);  // bucket index for a body of `size` >= 2 cells

struct TagClusters {
    int all = 0;    // every cluster (RULES.md definition)
    int large = 0;  // clusters of at least cluster_min_size cells (the diversity target)
};

// Splits tags on the circle at gaps wider than `gap`. A group of living cells whose tags
// are more than `gap` from every cell outside it is one cluster. `tags` is reordered.
TagClusters count_tag_clusters(std::vector<double>& tags, double gap, int min_size);

// Births, deaths and the peak population over the ticks since the previous census row.
struct IntervalCounts {
    int peak_cells = 0;
    std::array<std::uint64_t, 3> births{};  // by BirthKind
    std::array<std::uint64_t, 3> deaths{};  // by DeathCause

    void add_tick(const World& w);  // call after each step()
    void reset();
};

// The state of the world at the start of tick `tick` (after `tick` ticks have run).
struct Census {
    std::uint64_t tick = 0;
    std::uint64_t year = 0;
    double season = 0.0;
    int cells = 0;
    int free_cells = 0;
    int body_cells = 0;
    int bodies = 0;
    std::array<int, kBodyBuckets> bodies_by_size{};
    int largest_body = 0;
    int inner_cells = 0;
    int infected = 0;
    int dormant = 0;    // dormant in the tick just run
    int producers = 0;  // photosynthesis gene > harvest gene
    int consumers = 0;
    TagClusters clusters;
    std::array<double, kGeneCount> gene_mean{};
    MatterTotals matter;
    std::uint64_t hash = 0;
};

Census take_census(const World& w);

// CSV with one row per census. The interval columns cover the ticks since the previous row.
void write_census_header(std::FILE* out);
void write_census_row(std::FILE* out, const Census& c, const IntervalCounts& n);

// Binary lineage log. Little-endian, fixed-size records after a 64-byte header:
//   header: "EVOLIN01", u32 version, u32 gene count, u32 birth record size,
//           u32 death record size, u64 seed, u64 params hash, 24 bytes zero
//   birth (100 bytes): u8 type = 1, u8 kind, u16 site, u32 tick, u32 id, u32 parent,
//           u32 parent2, f32 genes[20]
//   death (16 bytes):  u8 type = 2, u8 cause, u16 site, u32 tick, u32 id, u32 age
// Birth kinds: 0 attached clone, 1 released clone, 2 mating, 3 ancestor (tick 0).
// Death causes: 0 disaster, 1 drained, 2 starved.
inline constexpr std::uint8_t kLineageAncestor = 3;
inline constexpr std::size_t kLineageHeaderSize = 64;
inline constexpr std::size_t kLineageBirthSize = 100;
inline constexpr std::size_t kLineageDeathSize = 16;

class LineageWriter {
public:
    // Opens `path` and writes the header and one ancestor record per living cell.
    // Returns an empty string on success, otherwise an error message.
    std::string open(const std::string& path, const World& w);
    // Appends the births and deaths of the tick just run.
    void add_tick(const World& w);
    // Flushes and closes the file. Returns an empty string unless a write failed.
    std::string close();
    ~LineageWriter();

    std::uint64_t bytes_written() const { return bytes_; }

private:
    void put_birth(std::uint64_t tick, std::uint64_t id, std::uint64_t parent, std::uint64_t parent2,
                   int site, const Genome& g, std::uint8_t kind);
    void put_death(const DeathEvent& d);
    void flush();
    void require_u32(std::uint64_t v);

    std::FILE* file_ = nullptr;
    std::vector<unsigned char> buf_;
    std::uint64_t bytes_ = 0;
    bool failed_ = false;
};

// FNV-1a of the params in write_params format; identifies a run's settings.
std::uint64_t params_hash(const Params& p);

}  // namespace evo
