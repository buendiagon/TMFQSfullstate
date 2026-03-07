#include "tmfqs/algorithms/qft.h"

#include <vector>

#include "tmfqs/algorithms/operations.h"
#include "tmfqs/core/constants.h"

namespace tmfqs {
namespace algorithms {

void qftInPlace(QuantumRegister &quantumRegister) {
	const unsigned int numQubits = quantumRegister.qubitCount();
	std::vector<AlgorithmOperation> operations;
	operations.reserve(
		static_cast<size_t>(numQubits) * (static_cast<size_t>(numQubits) + 1u) / 2u +
		static_cast<size_t>(numQubits / 2u));

	for(unsigned int target = 0; target < numQubits; ++target) {
		operations.push_back(HadamardOp{target});
		for(unsigned int distance = 1; target + distance < numQubits; ++distance) {
			operations.push_back(ControlledPhaseShiftOp{
				target + distance,
				target,
				kPi / static_cast<double>(1u << distance)
			});
		}
	}

	for(unsigned int i = 0; i < numQubits / 2u; ++i) {
		operations.push_back(SwapOp{i, numQubits - i - 1u});
	}

	executeOperations(quantumRegister, operations);
}

} // namespace algorithms
} // namespace tmfqs
