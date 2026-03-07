#ifndef TMFQS_CORE_VALIDATION_H
#define TMFQS_CORE_VALIDATION_H

#include <stdexcept>
#include <string>
#include <vector>

#include "tmfqs/core/state_space.h"
#include "tmfqs/core/types.h"

namespace tmfqs {

inline void validateStateIndex(const char *scopeName, StateIndex state, unsigned int stateCount) {
	if(state >= stateCount) {
		throw std::out_of_range(std::string(scopeName) + " state index out of range");
	}
}

inline void validateQubitIndex(const char *scopeName, QubitIndex qubit, unsigned int numQubits) {
	if(qubit >= numQubits) {
		throw std::out_of_range(std::string(scopeName) + " qubit index out of range");
	}
}

inline void validateDistinctQubits(const char *scopeName, QubitIndex q0, QubitIndex q1) {
	if(q0 == q1) {
		throw std::invalid_argument(std::string(scopeName) + " requires distinct qubits");
	}
}

inline void validateGateTargets(
	const char *scopeName,
	const QubitList &qubits,
	unsigned int numQubits,
	unsigned int gateDimension) {
	if(qubits.size() > maxSupportedQubitsForU32States()) {
		throw std::invalid_argument(std::string(scopeName) + " qubit list exceeds supported index width");
	}
	if(qubits.size() > numQubits) {
		throw std::invalid_argument(std::string(scopeName) + " gate acts on more qubits than available");
	}
	if(gateDimension != checkedStateCount(static_cast<unsigned int>(qubits.size()))) {
		throw std::invalid_argument(std::string(scopeName) + " gate dimension does not match qubit list");
	}
	std::vector<bool> seen(numQubits, false);
	for(QubitIndex qubit : qubits) {
		validateQubitIndex(scopeName, qubit, numQubits);
		if(seen[qubit]) {
			throw std::invalid_argument(std::string(scopeName) + " duplicate qubit index");
		}
		seen[qubit] = true;
	}
}

} // namespace tmfqs

#endif // TMFQS_CORE_VALIDATION_H
