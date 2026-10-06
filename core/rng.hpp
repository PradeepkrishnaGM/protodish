#pragma once

#include <cstdint>

namespace evo {

// PCG32 (XSH-RR). Hand-written mappings so the output does not depend on the
// standard library. See DECISIONS.md.
class Rng {
public:
    explicit Rng(std::uint64_t seed, std::uint64_t stream = 0xda3e39cb94b95bdbULL);

    std::uint32_t next_u32();

    // Uniform integer in [0, n). n must be > 0. Lemire's unbiased method.
    std::uint32_t below(std::uint32_t n);

    // Uniform double in [0, 1) with 53 random bits.
    double uniform();

    // True with probability p.
    bool chance(double p) { return uniform() < p; }

    std::uint64_t state() const { return state_; }
    std::uint64_t increment() const { return inc_; }

private:
    std::uint64_t state_;
    std::uint64_t inc_;
};

}  // namespace evo
