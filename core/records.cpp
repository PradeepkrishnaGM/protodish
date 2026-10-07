#include "records.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace evo {

int body_bucket(int size) {
    if (size <= 2) return 0;
    if (size <= 4) return 1;
    if (size <= 8) return 2;
    if (size <= 16) return 3;
    if (size <= 32) return 4;
    return 5;
}

TagClusters count_tag_clusters(std::vector<double>& tags, double gap, int min_size) {
    TagClusters out;
    const std::size_t n = tags.size();
    if (n == 0) return out;
    std::sort(tags.begin(), tags.end());

    // Cut the circle at every gap wider than `gap`; the arcs between cuts are clusters.
    std::vector<std::size_t> cuts;  // cluster starts: index i where tags[i-1] -> tags[i] is a cut
    for (std::size_t i = 1; i < n; ++i) {
        if (tags[i] - tags[i - 1] > gap) cuts.push_back(i);
    }
    const bool wrap_cut = (1.0 - tags[n - 1]) + tags[0] > gap;
    if (wrap_cut) cuts.insert(cuts.begin(), 0);
    if (cuts.empty()) {  // no gap anywhere: one cluster around the whole circle
        out.all = 1;
        out.large = static_cast<int>(n) >= min_size ? 1 : 0;
        return out;
    }
    // With at least one cut, each cluster runs from one cut to the next (cyclically).
    const std::size_t k = cuts.size();
    for (std::size_t c = 0; c < k; ++c) {
        const std::size_t start = cuts[c];
        const std::size_t end = c + 1 < k ? cuts[c + 1] : cuts[0] + n;
        const std::size_t size = end - start;
        ++out.all;
        if (size >= static_cast<std::size_t>(min_size)) ++out.large;
    }
    return out;
}

void IntervalCounts::add_tick(const World& w) {
    peak_cells = std::max(peak_cells, w.cell_count());
    for (const BirthEvent& b : w.births()) ++births[static_cast<std::size_t>(b.kind)];
    for (const DeathEvent& d : w.deaths()) ++deaths[static_cast<std::size_t>(d.cause)];
}

void IntervalCounts::reset() { *this = IntervalCounts{}; }

Census take_census(const World& w, bool with_hash) {
    const Params& p = w.params();
    const CellArrays& c = w.cells();
    Census out;
    out.tick = w.tick();
    out.year = w.tick() / static_cast<std::uint64_t>(p.year_length);
    out.season = w.climate().season(w.tick());
    out.cells = w.cell_count();
    out.matter = w.matter();
    if (with_hash) out.hash = w.state_hash();

    const std::vector<int> label = w.body_labels();
    std::vector<int> group_size;
    std::vector<double> tags;
    tags.reserve(static_cast<std::size_t>(out.cells));
    int not_awake = 0;
    for (int s = 0; s < w.site_count(); ++s) {
        const auto i = static_cast<std::size_t>(s);
        if (!c.alive[i]) continue;
        const auto g = static_cast<std::size_t>(label[i]);
        if (g >= group_size.size()) group_size.resize(g + 1, 0);
        ++group_size[g];
        if (w.is_inner(s)) ++out.inner_cells;
        out.infected += c.infected[i];
        if (!c.awake[i]) ++not_awake;
        if (c.genes[kPhotosynthesis][i] > c.genes[kHarvest][i]) {
            ++out.producers;
        } else {
            ++out.consumers;
        }
        if (!(c.gross_intake[i] > 0.0)) {
            ++out.no_intake;
        } else if (c.photo_intake[i] > 0.5 * c.gross_intake[i]) {
            ++out.producers_intake;
        } else {
            ++out.consumers_intake;
        }
        tags.push_back(c.genes[kTag][i]);
        for (std::size_t k = 0; k < kGeneCount; ++k) out.gene_mean[k] += c.genes[k][i];
    }
    for (const int size : group_size) {
        if (size == 1) {
            ++out.free_cells;
            continue;
        }
        ++out.bodies;
        out.body_cells += size;
        ++out.bodies_by_size[static_cast<std::size_t>(body_bucket(size))];
        out.largest_body = std::max(out.largest_body, size);
    }
    // Daughters born in the tick just run were not sensed, so they are not awake; they are
    // not dormant either. Before the first tick, nothing has been sensed.
    out.dormant = w.tick() == 0 ? 0 : not_awake - static_cast<int>(w.births().size());
    for (auto& m : out.gene_mean) {
        m = out.cells > 0 ? m / out.cells : std::numeric_limits<double>::quiet_NaN();
    }
    out.clusters = count_tag_clusters(tags, p.cluster_gap, p.cluster_min_size);
    return out;
}

void write_census_header(std::FILE* out) {
    std::fputs("tick,year,season,cells,free_cells,body_cells,bodies,"
               "bodies_2,bodies_3_4,bodies_5_8,bodies_9_16,bodies_17_32,bodies_33up,largest_body,"
               "inner_cells,infected,dormant,producers,consumers,"
               "producers_intake,consumers_intake,no_intake,tag_clusters,tag_clusters_large,"
               "peak_cells,births_attached,births_released,births_mating,"
               "deaths_disaster,deaths_drained,deaths_starved",
               out);
    for (const GeneInfo& g : kGeneInfo) std::fprintf(out, ",mean_%s", g.name);
    std::fputs(",food_a,food_b,minerals,in_cells,total,hash\n", out);
}

void write_census_row(std::FILE* out, const Census& c, const IntervalCounts& n) {
    std::fprintf(out, "%llu,%llu,%.6f,%d,%d,%d,%d", static_cast<unsigned long long>(c.tick),
                 static_cast<unsigned long long>(c.year), c.season, c.cells, c.free_cells,
                 c.body_cells, c.bodies);
    for (const int b : c.bodies_by_size) std::fprintf(out, ",%d", b);
    std::fprintf(out, ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", c.largest_body, c.inner_cells,
                 c.infected, c.dormant, c.producers, c.consumers, c.producers_intake,
                 c.consumers_intake, c.no_intake, c.clusters.all, c.clusters.large, n.peak_cells);
    for (const auto b : n.births) std::fprintf(out, ",%llu", static_cast<unsigned long long>(b));
    for (const auto d : n.deaths) std::fprintf(out, ",%llu", static_cast<unsigned long long>(d));
    for (const double m : c.gene_mean) std::fprintf(out, ",%.6g", m);
    std::fprintf(out, ",%.6f,%.6f,%.6f,%.6f,%.6f,%016llx\n", c.matter.food_a, c.matter.food_b,
                 c.matter.minerals, c.matter.cells, c.matter.total(),
                 static_cast<unsigned long long>(c.hash));
}

// ---- Lineage log ----

namespace {

constexpr std::size_t kFlushBytes = 1 << 20;

void put_u8(std::vector<unsigned char>& b, std::uint8_t v) { b.push_back(v); }
void put_u16(std::vector<unsigned char>& b, std::uint16_t v) {
    b.push_back(static_cast<unsigned char>(v));
    b.push_back(static_cast<unsigned char>(v >> 8));
}
void put_u32(std::vector<unsigned char>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<unsigned char>(v >> (8 * i)));
}
void put_u64(std::vector<unsigned char>& b, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) b.push_back(static_cast<unsigned char>(v >> (8 * i)));
}
void put_f32(std::vector<unsigned char>& b, double v) {
    const auto f = static_cast<float>(v);
    std::uint32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    put_u32(b, bits);
}

}  // namespace

std::uint64_t params_hash(const Params& p) {
    std::ostringstream s;
    write_params(p, s);
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const char ch : s.str()) {
        h ^= static_cast<unsigned char>(ch);
        h *= 0x100000001b3ULL;
    }
    return h;
}

std::string LineageWriter::open(const std::string& path, const World& w) {
    if (w.site_count() > 65536) return "lineage log: grid too large for 16-bit site numbers";
    file_ = std::fopen(path.c_str(), "wb");
    if (!file_) return "cannot open " + path;
    buf_.reserve(kFlushBytes + 4096);
    const char magic[8] = {'E', 'V', 'O', 'L', 'I', 'N', '0', '1'};
    buf_.insert(buf_.end(), magic, magic + 8);
    put_u32(buf_, 1);  // version
    put_u32(buf_, static_cast<std::uint32_t>(kGeneCount));
    put_u32(buf_, static_cast<std::uint32_t>(kLineageBirthSize));
    put_u32(buf_, static_cast<std::uint32_t>(kLineageDeathSize));
    put_u64(buf_, w.seed());
    put_u64(buf_, params_hash(w.params()));
    buf_.resize(kLineageHeaderSize, 0);

    const CellArrays& c = w.cells();
    for (int s = 0; s < w.site_count(); ++s) {
        const auto i = static_cast<std::size_t>(s);
        if (!c.alive[i]) continue;
        put_birth(w.tick(), c.id[i], c.parent_id[i], c.parent2_id[i], s, c.genome(i), kLineageAncestor);
    }
    flush();
    return failed_ ? "write failed: " + path : "";
}

void LineageWriter::add_tick(const World& w) {
    if (!file_) return;
    for (const BirthEvent& b : w.births()) {
        put_birth(b.tick, b.id, b.parent_id, b.parent2_id, b.site, b.genome,
                  static_cast<std::uint8_t>(b.kind));
    }
    for (const DeathEvent& d : w.deaths()) put_death(d);
    if (buf_.size() >= kFlushBytes) flush();
}

void LineageWriter::require_u32(std::uint64_t v) {
    if (v > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("lineage log: value does not fit in 32 bits");
    }
}

void LineageWriter::put_birth(std::uint64_t tick, std::uint64_t id, std::uint64_t parent,
                              std::uint64_t parent2, int site, const Genome& g, std::uint8_t kind) {
    for (const auto v : {tick, id, parent, parent2}) require_u32(v);
    put_u8(buf_, 1);
    put_u8(buf_, kind);
    put_u16(buf_, static_cast<std::uint16_t>(site));
    put_u32(buf_, static_cast<std::uint32_t>(tick));
    put_u32(buf_, static_cast<std::uint32_t>(id));
    put_u32(buf_, static_cast<std::uint32_t>(parent));
    put_u32(buf_, static_cast<std::uint32_t>(parent2));
    for (const double v : g) put_f32(buf_, v);
}

void LineageWriter::put_death(const DeathEvent& d) {
    for (const auto v : {d.tick, d.id}) require_u32(v);
    put_u8(buf_, 2);
    put_u8(buf_, static_cast<std::uint8_t>(d.cause));
    put_u16(buf_, static_cast<std::uint16_t>(d.site));
    put_u32(buf_, static_cast<std::uint32_t>(d.tick));
    put_u32(buf_, static_cast<std::uint32_t>(d.id));
    put_u32(buf_, d.age);
}

void LineageWriter::flush() {
    if (!file_ || buf_.empty()) return;
    if (std::fwrite(buf_.data(), 1, buf_.size(), file_) != buf_.size()) failed_ = true;
    bytes_ += buf_.size();
    buf_.clear();
}

std::string LineageWriter::close() {
    if (!file_) return "";
    flush();
    if (std::fclose(file_) != 0) failed_ = true;
    file_ = nullptr;
    return failed_ ? "lineage log: write failed" : "";
}

LineageWriter::~LineageWriter() { close(); }

}  // namespace evo
