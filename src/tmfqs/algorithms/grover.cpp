#include "tmfqs/algorithms/grover.h"

#include <cmath>
#include <stdexcept>

#include "tmfqs/algorithms/operations.h"
#include "tmfqs/core/constants.h"
#include "tmfqs/core/state_space.h"
#include "tmfqs/register/quantum_register.h"

namespace tmfqs {
namespace algorithms {

StateIndex groverSearch(const GroverConfig &config, IRandomSource &randomSource) {
	const unsigned int stateCount = checkedStateCount(config.numQubits);
	if(config.markedState >= stateCount) {
		throw std::invalid_argument("groverSearch: marked state index is out of range for numQubits");
	}

	QuantumRegister quantumRegister(config.numQubits);
	CompiledAlgorithmPlan plan;
	for(unsigned int q = 0; q < config.numQubits; ++q) {
		plan.addOperation(HadamardOp{q});
	}

	const std::vector<AlgorithmOperation> iterationOperations = {
		PhaseFlipBasisStateOp{config.markedState},
		InversionAboutMeanOp{}
	};

	const double idealIterations = (kPi / 4.0) * std::sqrt(static_cast<double>(stateCount));
	const unsigned int iterations = static_cast<unsigned int>(std::floor(idealIterations));
	plan.addRepeatBlock(iterationOperations, iterations);
	executePlan(quantumRegister, plan);

	if(config.verbose) {
		quantumRegister.printStatesVector();
	}
	return quantumRegister.measure(randomSource);
}

} // namespace algorithms
} // namespace tmfqs
