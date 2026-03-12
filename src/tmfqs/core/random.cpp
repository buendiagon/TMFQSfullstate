#include "tmfqs/core/random.h"

namespace tmfqs {

/** @brief Constructs source seeded from non-deterministic entropy. */
Mt19937RandomSource::Mt19937RandomSource() : engine_(std::random_device{}()) {}

/** @brief Constructs source seeded with a deterministic value. */
Mt19937RandomSource::Mt19937RandomSource(uint32_t seed) : engine_(seed) {}

/** @brief Produces the next pseudo-random value in `[0, 1)`. */
double Mt19937RandomSource::nextUnitDouble() {
	return distribution_(engine_);
}

/** @brief Re-seeds the underlying pseudo-random engine. */
void Mt19937RandomSource::reseed(uint32_t seed) {
	engine_.seed(seed);
}

} // namespace tmfqs
