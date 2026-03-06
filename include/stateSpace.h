#ifndef STATE_SPACE_INCLUDE
#define STATE_SPACE_INCLUDE

#include <cstddef>
#include <limits>
#include <stdexcept>

// Maximum qubit count that still allows 2^n states in a 32-bit state index.
inline constexpr unsigned int maxSupportedQubitsForU32States() {
	return std::numeric_limits<unsigned int>::digits - 1u;
}

// Returns 2^numQubits or throws if numQubits exceeds supported range.
inline unsigned int checkedStateCount(unsigned int numQubits) {
	if(numQubits > maxSupportedQubitsForU32States()) {
		throw std::invalid_argument("Number of qubits exceeds supported range for 32-bit state indexing");
	}
	return 1u << numQubits;
}

// Returns number of scalar entries in the interleaved amplitude buffer:
// [real0, imag0, real1, imag1, ...].
inline size_t checkedAmplitudeElementCount(unsigned int numQubits) {
	return static_cast<size_t>(checkedStateCount(numQubits)) * 2u;
}

#endif // STATE_SPACE_INCLUDE
