#ifndef TMFQS_STORAGE_BACKEND_COMMON_H
#define TMFQS_STORAGE_BACKEND_COMMON_H

#include <stdexcept>
#include <string>

#include "tmfqs/core/validation.h"
#include "tmfqs/storage/gate_block_apply.h"

namespace tmfqs {

inline void ensureBackendInitialized(bool initialized, const char *backendName, const char *operation) {
	if(!initialized) {
		throw std::logic_error(std::string(backendName) + " is not initialized for " + operation);
	}
}

inline void validateBackendStateIndex(const char *scopeName, StateIndex state, unsigned int stateCount) {
	validateStateIndex(scopeName, state, stateCount);
}

inline void validateBackendSingleQubit(const char *scopeName, QubitIndex qubit, unsigned int numQubits) {
	validateQubitIndex(scopeName, qubit, numQubits);
}

inline void validateBackendTwoQubits(const char *scopeName, QubitIndex q0, QubitIndex q1, unsigned int numQubits) {
	validateQubitIndex(scopeName, q0, numQubits);
	validateQubitIndex(scopeName, q1, numQubits);
	validateDistinctQubits(scopeName, q0, q1);
}

inline unsigned int qubitMaskFromMsbIndex(QubitIndex qubit, unsigned int numQubits) {
	return 1u << (numQubits - qubit - 1u);
}

template <typename PairFn>
inline void forEachStatePairByMask(unsigned int numStates, unsigned int targetMask, PairFn pairFn) {
	if(targetMask == 0u) {
		throw std::logic_error("forEachStatePairByMask requires a non-zero target mask");
	}
	const unsigned int stride = targetMask << 1u;
	for(unsigned int base = 0; base < numStates; base += stride) {
		for(unsigned int offset = 0; offset < targetMask; ++offset) {
			const unsigned int state0 = base + offset;
			pairFn(state0, state0 + targetMask);
		}
	}
}

template <typename LoadAmplitudeFn, typename StoreAmplitudeFn>
inline void applyGateThroughBlocks(
	const char *scopeName,
	const QuantumGate &gate,
	const QubitList &qubits,
	unsigned int numQubits,
	LoadAmplitudeFn loadAmplitude,
	StoreAmplitudeFn storeAmplitude,
	GateBlockWorkspace &workspace) {
	validateGateTargets(scopeName, qubits, numQubits, gate.dimension());
	const GateBlockLayout layout = makeGateBlockLayout(qubits, numQubits);
	applyGateByBlocks(gate, layout, loadAmplitude, storeAmplitude, workspace);
}

} // namespace tmfqs

#endif // TMFQS_STORAGE_BACKEND_COMMON_H
