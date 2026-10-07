#include "history.hpp"

#include <algorithm>

namespace evo {

PopulationCounts count_population(const World& w) {
    const CellArrays& c = w.cells();
    PopulationCounts out;
    out.cells = w.cell_count();
    for (std::size_t i = 0; i < c.alive.size(); ++i) {
        if (!c.alive[i]) continue;
        out.producers += c.genes[kPhotosynthesis][i] > c.genes[kHarvest][i] ? 1 : 0;
        out.infected += c.infected[i];
    }
    return out;
}

PopulationHistory::PopulationHistory(std::size_t capacity) : capacity_(std::max<std::size_t>(capacity, 2)) {}

void PopulationHistory::clear() {
    samples_.clear();
    stride_ = 1;
    in_last_ = 0;
}

void PopulationHistory::merge_into(PopulationCounts& into, const PopulationCounts& c) {
    into.cells = std::max(into.cells, c.cells);
    into.producers = std::max(into.producers, c.producers);
    into.infected = std::max(into.infected, c.infected);
}

void PopulationHistory::add(std::uint64_t tick, const PopulationCounts& counts) {
    if (!samples_.empty() && in_last_ < stride_) {
        merge_into(samples_.back().peak, counts);
        ++in_last_;
        return;
    }
    if (samples_.size() == capacity_) halve();
    if (!samples_.empty() && in_last_ < stride_) {  // halving can leave the last sample short
        merge_into(samples_.back().peak, counts);
        ++in_last_;
        return;
    }
    samples_.push_back({tick, counts});
    in_last_ = 1;
}

void PopulationHistory::halve() {
    std::size_t out = 0;
    for (std::size_t k = 0; k < samples_.size(); k += 2) {
        Sample s = samples_[k];
        if (k + 1 < samples_.size()) merge_into(s.peak, samples_[k + 1].peak);
        samples_[out++] = s;
    }
    // The new last sample covers the ticks of one old full sample if the count was odd,
    // otherwise two.
    in_last_ = samples_.size() % 2 == 1 ? stride_ : stride_ + in_last_;
    samples_.resize(out);
    stride_ *= 2;
}

PopulationHistory::Series PopulationHistory::downsample(std::size_t max_points) const {
    Series out;
    if (samples_.empty()) return out;
    max_points = std::max<std::size_t>(max_points, 1);
    const std::size_t per = (samples_.size() + max_points - 1) / max_points;
    for (std::size_t k = 0; k < samples_.size(); k += per) {
        PopulationCounts peak = samples_[k].peak;
        for (std::size_t j = k + 1; j < std::min(k + per, samples_.size()); ++j) merge_into(peak, samples_[j].peak);
        out.tick.push_back(samples_[k].tick);
        out.cells.push_back(peak.cells);
        out.producers.push_back(peak.producers);
        out.infected.push_back(peak.infected);
    }
    return out;
}

}  // namespace evo
