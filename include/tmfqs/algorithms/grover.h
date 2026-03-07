#ifndef TMFQS_ALGORITHMS_GROVER_H
#define TMFQS_ALGORITHMS_GROVER_H

#include "tmfqs/core/random.h"
#include "tmfqs/core/types.h"

namespace tmfqs {
namespace algorithms {

struct GroverConfig {
	StateIndex markedState = 0;
	unsigned int numQubits = 0;
	bool verbose = false;
};

StateIndex groverSearch(const GroverConfig &config, IRandomSource &randomSource);

} // namespace algorithms
} // namespace tmfqs

#endif // TMFQS_ALGORITHMS_GROVER_H
