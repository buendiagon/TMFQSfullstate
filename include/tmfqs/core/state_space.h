#ifndef TMFQS_CORE_STATE_SPACE_H
#define TMFQS_CORE_STATE_SPACE_H

#include <cstddef>
#include <limits>
#include <stdexcept>

#include "tmfqs/core/types.h"

namespace tmfqs {

/**
 * @brief Returns maximum qubit count supported by state indexing.
 * @return Maximum safe number of qubits.
 */
inline constexpr unsigned int maxSupportedQubitsForStateIndex() {
	return std::numeric_limits<StateIndex>::digits - 1u;
}

/** @brief Backward-compatible name for older examples. */
inline constexpr unsigned int maxSupportedQubitsForU32States() {
	return maxSupportedQubitsForStateIndex();
}

/**
 * @brief Computes state-space size `2^numQubits` with bounds checking.
 * @param numQubits Number of qubits.
 * @return Number of basis states.
 * @throws std::invalid_argument If the value exceeds supported index width.
 */
inline StateIndex checkedStateCount(unsigned int numQubits) {
	if(numQubits > maxSupportedQubitsForStateIndex()) {
		throw std::invalid_argument("Number of qubits exceeds supported range for state indexing");
	}
	return StateIndex{1u} << numQubits;
}

/**
 * @brief Computes amplitude storage length in `double` elements.
 * @param numQubits Number of qubits.
 * @return `2 * checkedStateCount(numQubits)`.
 */
inline size_t checkedAmplitudeElementCount(unsigned int numQubits) {
	return static_cast<size_t>(checkedStateCount(numQubits)) * 2u;
}

} // namespace tmfqs

#endif // TMFQS_CORE_STATE_SPACE_H
