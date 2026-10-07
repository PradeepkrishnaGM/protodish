#include "inspect.hpp"

#include <vector>

namespace evo {

void CellTracker::select(const World& w, int site) {
    clear();
    if (site < 0 || site >= w.site_count() || !w.cells().alive[static_cast<std::size_t>(site)]) return;
    state_ = State::Alive;
    id_ = w.cells().id[static_cast<std::size_t>(site)];
    site_ = site;
}

void CellTracker::update(const World& w) {
    if (state_ != State::Alive) return;
    const CellArrays& c = w.cells();
    if (c.alive[static_cast<std::size_t>(site_)] && c.id[static_cast<std::size_t>(site_)] == id_) return;
    for (const DeathEvent& d : w.deaths()) {
        if (d.id == id_) {
            state_ = State::Dead;
            site_ = d.site;
            death_tick_ = d.tick;
            cause_ = d.cause;
            return;
        }
    }
    // It moved: a free cell steps to a neighbor, so look there first.
    for (int d = 0; d < 8; ++d) {
        const int t = w.neighbor(site_, d);
        if (c.alive[static_cast<std::size_t>(t)] && c.id[static_cast<std::size_t>(t)] == id_) {
            site_ = t;
            return;
        }
    }
    for (int s = 0; s < w.site_count(); ++s) {
        if (c.alive[static_cast<std::size_t>(s)] && c.id[static_cast<std::size_t>(s)] == id_) {
            site_ = s;
            return;
        }
    }
    // Not found and no death event: only possible if the world was replaced. Treat as removed.
    state_ = State::Removed;
    death_tick_ = w.tick();
}

void CellTracker::removed(const World& w) {
    if (state_ != State::Alive) return;
    state_ = State::Removed;
    death_tick_ = w.tick();
}

const char* death_cause_name(DeathCause cause) {
    switch (cause) {
        case DeathCause::Disaster: return "disaster";
        case DeathCause::Drained: return "drained";
        case DeathCause::Starved: return "starved";
    }
    return "?";
}

int body_size(const World& w, int site) {
    const CellArrays& c = w.cells();
    if (!c.alive[static_cast<std::size_t>(site)]) return 0;
    std::vector<std::uint8_t> seen(static_cast<std::size_t>(w.site_count()), 0);
    std::vector<int> stack{site};
    seen[static_cast<std::size_t>(site)] = 1;
    int size = 1;
    while (!stack.empty()) {
        const int s = stack.back();
        stack.pop_back();
        for (int d = 0; d < 8; ++d) {
            if (!(c.bonds[static_cast<std::size_t>(s)] & (1u << d))) continue;
            const auto t = static_cast<std::size_t>(w.neighbor(s, d));
            if (seen[t]) continue;
            seen[t] = 1;
            ++size;
            stack.push_back(static_cast<int>(t));
        }
    }
    return size;
}

}  // namespace evo
