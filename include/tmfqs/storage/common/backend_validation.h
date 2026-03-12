#ifndef TMFQS_STORAGE_COMMON_BACKEND_VALIDATION_H
#define TMFQS_STORAGE_COMMON_BACKEND_VALIDATION_H

#include <stdexcept>
#include <string>

#include "tmfqs/core/validation.h"

namespace tmfqs {
namespace storage {

/** @brief Ensures backend storage exists before an operation runs. */
inline void ensureBackendInitialized(bool initialized, const char *backendName, const char *operation) {
	if(!initialized) {
		throw std::logic_error(std::string(backendName) + " is not initialized for " + operation);
	}
}

/** @brief Validates a basis-state index for backend state size. */
inline void validateBackendStateIndex(const char *scopeName, StateIndex state, unsigned int stateCount) {
	validateStateIndex(scopeName, state, stateCount);
}

/** @brief Validates one qubit index for backend state size. */
inline void validateBackendSingleQubit(const char *scopeName, QubitIndex qubit, unsigned int numQubits) {
	validateQubitIndex(scopeName, qubit, numQubits);
}

/** @brief Validates two distinct qubit indices for backend state size. */
inline void validateBackendTwoQubits(const char *scopeName, QubitIndex q0, QubitIndex q1, unsigned int numQubits) {
	validateQubitIndex(scopeName, q0, numQubits);
	validateQubitIndex(scopeName, q1, numQubits);
	validateDistinctQubits(scopeName, q0, q1);
}

/** @brief Converts user-facing qubit index to bit mask under MSB-first qubit ordering. */
inline unsigned int qubitMaskFromMsbIndex(QubitIndex qubit, unsigned int numQubits) {
	return 1u << (numQubits - qubit - 1u);
}

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_COMMON_BACKEND_VALIDATION_H
