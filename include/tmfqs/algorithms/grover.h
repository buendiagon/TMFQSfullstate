#ifndef TMFQS_ALGORITHMS_GROVER_H
#define TMFQS_ALGORITHMS_GROVER_H

#include <utility>

#include "tmfqs/config/register_config.h"
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
	/**
	 * @brief Optional list of marked states.
	 *
	 * When non-empty, this list replaces `markedState` and the oracle marks all
	 * listed basis states in the same Grover run.
	 */
	BasisStateList markedStates;
	/** @brief Backend and compression configuration for the search register. */
	RegisterConfig registerConfig;

	GroverConfig() = default;

	GroverConfig(
		StateIndex singleMarkedState,
		unsigned int qubits,
		bool verboseOutput,
		const RegisterConfig &cfg = {})
		: markedState(singleMarkedState),
		  numQubits(qubits),
		  verbose(verboseOutput),
		  registerConfig(cfg) {}

	GroverConfig(
		BasisStateList markedStatesList,
		unsigned int qubits,
		bool verboseOutput,
		const RegisterConfig &cfg = {})
		: markedState(markedStatesList.empty() ? 0u : markedStatesList[0]),
		  numQubits(qubits),
		  verbose(verboseOutput),
		  markedStates(std::move(markedStatesList)),
		  registerConfig(cfg) {}
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
