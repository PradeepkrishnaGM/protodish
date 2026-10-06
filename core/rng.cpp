#include "rng.hpp"

namespace evo {

Rng::Rng(std::uint64_t seed, std::uint64_t stream) : state_(0), inc_((stream << 1u) | 1u) {
    next_u32();
    state_ += seed;
    next_u32();
}

std::uint32_t Rng::next_u32() {
    const std::uint64_t old = state_;
    state_ = old * 6364136223846793005ULL + inc_;
    const auto xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
    const auto rot = static_cast<std::uint32_t>(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
}

std::uint32_t Rng::below(std::uint32_t n) {
    std::uint64_t m = static_cast<std::uint64_t>(next_u32()) * n;
    auto low = static_cast<std::uint32_t>(m);
    if (low < n) {
        const std::uint32_t threshold = (0u - n) % n;
        while (low < threshold) {
            m = static_cast<std::uint64_t>(next_u32()) * n;
            low = static_cast<std::uint32_t>(m);
        }
    }
    return static_cast<std::uint32_t>(m >> 32u);
}

double Rng::uniform() {
    const std::uint64_t hi = next_u32();
    const std::uint64_t lo = next_u32();
    const std::uint64_t bits = ((hi << 32u) | lo) >> 11u;
    return static_cast<double>(bits) * 0x1.0p-53;
}

}  // namespace evo
