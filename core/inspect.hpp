#pragma once

// Click-to-inspect for the app (RULES.md, "Statistics": clicking a cell shows its genes,
// stores, age and stress). The tracker follows one cell by ID as it moves and records the
// tick and cause of its death. It only reads the world.

#include <cstdint>

#include "world.hpp"

namespace evo {

class CellTracker {
public:
    enum class State { None, Alive, Dead, Removed };  // Removed: cleared by End world

    // Selects the cell on `site`, or nothing if the site is empty.
    void select(const World& w, int site);
    void clear() { *this = CellTracker{}; }
    // Call after every step(): finds the cell again if it moved, or records its death.
    void update(const World& w);
    // Call when End world clears the cells.
    void removed(const World& w);

    State state() const { return state_; }
    std::uint64_t id() const { return id_; }
    int site() const { return site_; }  // current site, or where it died
    std::uint64_t death_tick() const { return death_tick_; }
    DeathCause death_cause() const { return cause_; }

private:
    State state_ = State::None;
    std::uint64_t id_ = 0;
    int site_ = -1;
    std::uint64_t death_tick_ = 0;
    DeathCause cause_ = DeathCause::Starved;
};

const char* death_cause_name(DeathCause cause);

// Number of cells in the body of the cell on `site` (1 for a free cell).
int body_size(const World& w, int site);

}  // namespace evo
