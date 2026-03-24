#include "tmfqs/algorithms/grover.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "tmfqs/algorithms/operations.h"
#include "tmfqs/core/constants.h"
#include "tmfqs/core/state_space.h"
#include "tmfqs/register/quantum_register.h"

namespace tmfqs {
namespace algorithms {
namespace {

std::vector<StateIndex> resolveMarkedStates(const GroverConfig &config, unsigned int stateCount) {
	std::vector<StateIndex> resolved;
	if(config.markedStates.empty()) {
		resolved.push_back(config.markedState);
	} else {
		resolved = config.markedStates.values();
	}

	if(resolved.empty()) {
		throw std::invalid_argument("groverSearch: requires at least one marked state");
	}
	for(StateIndex markedState : resolved) {
		if(markedState >= stateCount) {
			throw std::invalid_argument("groverSearch: marked state index is out of range for numQubits");
		}
	}
	std::sort(resolved.begin(), resolved.end());
	resolved.erase(std::unique(resolved.begin(), resolved.end()), resolved.end());
	return resolved;
}

unsigned int computeGroverIterations(unsigned int stateCount, size_t markedCount) {
	if(markedCount == 0u) {
		throw std::invalid_argument("groverSearch: requires at least one marked state");
	}
	// Integer floor keeps iteration count conservative to avoid over-rotation.
	const double idealIterations =
		(kPi / 4.0) *
		std::sqrt(static_cast<double>(stateCount) / static_cast<double>(markedCount));
	return static_cast<unsigned int>(std::floor(idealIterations));
}

} // namespace

/**
 * @brief Runs Grover's search procedure for a marked basis state.
 *
 * This routine prepares a uniform superposition, applies the oracle/diffusion
 * pair for approximately the optimal number of iterations, and measures.
 */
StateIndex groverSearch(const GroverConfig &config, IRandomSource &randomSource) {
	const unsigned int stateCount = checkedStateCount(config.numQubits);
	const std::vector<StateIndex> markedStates = resolveMarkedStates(config, stateCount);

	QuantumRegister quantumRegister(config.numQubits, config.registerConfig);
	CompiledAlgorithmPlan plan;
	for(unsigned int q = 0; q < config.numQubits; ++q) {
		plan.addOperation(HadamardOp{q});
	}

	std::vector<AlgorithmOperation> iterationOperations;
	iterationOperations.reserve(markedStates.size() + 1u);
	for(StateIndex markedState : markedStates) {
		iterationOperations.push_back(PhaseFlipBasisStateOp{markedState});
	}
	iterationOperations.push_back(InversionAboutMeanOp{});

	const unsigned int iterations = computeGroverIterations(stateCount, markedStates.size());
	plan.addRepeatBlock(iterationOperations, iterations);
	executePlan(quantumRegister, plan);

	if(config.verbose) {
		quantumRegister.printStatesVector();
	}
	return quantumRegister.measure(randomSource);
}

} // namespace algorithms
} // namespace tmfqs
