#pragma once

// Population over time for the app's graph (RULES.md, "Statistics"). The app records the
// world after every tick, so a crash inside one frame still shows. Memory is bounded: once
// `capacity` samples are held, neighboring samples merge in pairs and each sample then
// covers twice as many ticks. A merged sample keeps the peak of each series.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "world.hpp"

namespace evo {

struct PopulationCounts {
    int cells = 0;
    int producers = 0;  // photosynthesis gene > harvest gene (RULES.md)
    int infected = 0;
};

PopulationCounts count_population(const World& w);

class PopulationHistory {
public:
    explicit PopulationHistory(std::size_t capacity = std::size_t{1} << 20);

    void clear();
    // Adds the counts at `tick`. Ticks must increase by 1 from the first one added.
    void add(std::uint64_t tick, const PopulationCounts& counts);
    void record(const World& w) { add(w.tick(), count_population(w)); }

    std::size_t size() const { return samples_.size(); }
    std::uint64_t ticks_per_sample() const { return stride_; }

    struct Series {
        std::vector<std::uint64_t> tick;  // first tick each point covers
        std::vector<int> cells;
        std::vector<int> producers;
        std::vector<int> infected;
    };
    // At most max_points points (max_points >= 1); each is the peak of each series over
    // the samples it covers.
    Series downsample(std::size_t max_points) const;

private:
    struct Sample {
        std::uint64_t tick;
        PopulationCounts peak;
    };
    static void merge_into(PopulationCounts& into, const PopulationCounts& c);
    void halve();

    std::size_t capacity_;
    std::uint64_t stride_ = 1;   // ticks per sample
    std::uint64_t in_last_ = 0;  // ticks already in the last sample
    std::vector<Sample> samples_;
};

}  // namespace evo
