#ifndef VALIDATION_INCLUDE
#define VALIDATION_INCLUDE

#include <stdexcept>
#include <string>
#include <vector>

#include "stateSpace.h"
#include "types.h"

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
		if(qubit >= numQubits) {
			throw std::out_of_range(std::string(scopeName) + " qubit index out of range");
		}
		if(seen[qubit]) {
			throw std::invalid_argument(std::string(scopeName) + " duplicate qubit index");
		}
		seen[qubit] = true;
	}
}

#endif // VALIDATION_INCLUDE
