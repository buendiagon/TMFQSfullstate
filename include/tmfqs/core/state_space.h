#ifndef TMFQS_CORE_STATE_SPACE_H
#define TMFQS_CORE_STATE_SPACE_H

#include <cstddef>
#include <limits>
#include <stdexcept>

namespace tmfqs {

/**
 * @brief Returns maximum qubit count supported by 32-bit state indexing.
 * @return Maximum safe number of qubits.
 */
inline constexpr unsigned int maxSupportedQubitsForU32States() {
	return std::numeric_limits<unsigned int>::digits - 1u;
}

/**
 * @brief Computes state-space size `2^numQubits` with bounds checking.
 * @param numQubits Number of qubits.
 * @return Number of basis states.
 * @throws std::invalid_argument If the value exceeds supported index width.
 */
inline unsigned int checkedStateCount(unsigned int numQubits) {
	if(numQubits > maxSupportedQubitsForU32States()) {
		throw std::invalid_argument("Number of qubits exceeds supported range for 32-bit state indexing");
	}
	return 1u << numQubits;
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
