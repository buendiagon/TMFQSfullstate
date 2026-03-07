#include "tmfqs/core/random.h"

namespace tmfqs {

Mt19937RandomSource::Mt19937RandomSource() : engine_(std::random_device{}()) {}

Mt19937RandomSource::Mt19937RandomSource(uint32_t seed) : engine_(seed) {}

double Mt19937RandomSource::nextUnitDouble() {
	return distribution_(engine_);
}

void Mt19937RandomSource::reseed(uint32_t seed) {
	engine_.seed(seed);
}

} // namespace tmfqs
