#ifndef TMFQS_ALGORITHMS_GROVER_H
#define TMFQS_ALGORITHMS_GROVER_H

#include "tmfqs/core/random.h"
#include "tmfqs/core/types.h"

namespace tmfqs {
namespace algorithms {

/**
 * @brief Configuration values for a Grover search run.
 */
struct GroverConfig {
	/** @brief Basis-state index marked by the oracle. */
	StateIndex markedState = 0;
	/** @brief Number of qubits in the search register. */
	unsigned int numQubits = 0;
	/** @brief Enables amplitude printing before measurement when `true`. */
	bool verbose = false;
};

/**
 * @brief Executes Grover search and measures the final state.
 * @param config Search configuration.
 * @param randomSource Random source used for measurement sampling.
 * @return Sampled basis-state index after Grover iterations.
 */
StateIndex groverSearch(const GroverConfig &config, IRandomSource &randomSource);

} // namespace algorithms
} // namespace tmfqs

#endif // TMFQS_ALGORITHMS_GROVER_H
