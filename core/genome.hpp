#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace evo {

// The 20 genes, in RULES.md table order. The order is also the mutation draw order.
enum Gene : std::size_t {
    kTag,
    kTolerance,
    kHarvest,
    kDiet,
    kPhotosynthesis,
    kPreferredTemp,
    kAttack,
    kDefense,
    kResistance,
    kAdhesion,
    kShare,
    kRoleSplit,
    kMotility,
    kAppetite,
    kCaution,
    kBoldness,
    kSociability,
    kDormancy,
    kMutability,
    kMating,
    kGeneCount
};

struct GeneInfo {
    const char* name;
    double min;
    double max;
    bool circular;
};

// Gene ranges from the RULES.md genome table. These define the genome rather than
// tune it, so they live here instead of in Params.
inline constexpr std::array<GeneInfo, kGeneCount> kGeneInfo = {{
    {"tag", 0.0, 1.0, true},
    {"tolerance", 0.0, 0.5, false},
    {"harvest", 0.0, 1.0, false},
    {"diet", 0.0, 1.0, false},
    {"photosynthesis", 0.0, 1.0, false},
    {"preferred_temp", 0.0, 30.0, false},
    {"attack", 0.0, 1.0, false},
    {"defense", 0.0, 1.0, false},
    {"resistance", 0.0, 1.0, false},
    {"adhesion", 0.0, 1.0, false},
    {"share", 0.0, 1.0, false},
    {"role_split", -1.0, 1.0, false},
    {"motility", 0.0, 1.0, false},
    {"appetite", 0.0, 1.0, false},
    {"caution", 0.0, 1.0, false},
    {"boldness", 0.0, 1.0, false},
    {"sociability", -1.0, 1.0, false},
    {"dormancy", 0.0, 1.0, false},
    {"mutability", 0.0, 1.0, false},
    {"mating", 0.0, 1.0, false},
}};

using Genome = std::array<double, kGeneCount>;

// Stress range from RULES.md ("a value from 0 to 1").
inline constexpr double kStressMax = 1.0;

// Circular distance between two tags; at most 0.5.
inline double tag_distance(double a, double b) {
    const double d = std::fabs(a - b);
    return d < 0.5 ? d : 1.0 - d;
}

// Wraps a tag into [0, 1).
inline double wrap_tag(double v) {
    v -= std::floor(v);
    return v < 1.0 ? v : 0.0;  // -tiny wraps to 1.0 after rounding
}

}  // namespace evo
