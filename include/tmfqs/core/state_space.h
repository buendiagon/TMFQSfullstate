#ifndef TMFQS_CORE_STATE_SPACE_H
#define TMFQS_CORE_STATE_SPACE_H

#include <cstddef>
#include <limits>
#include <stdexcept>

namespace tmfqs {

inline constexpr unsigned int maxSupportedQubitsForU32States() {
	return std::numeric_limits<unsigned int>::digits - 1u;
}

inline unsigned int checkedStateCount(unsigned int numQubits) {
	if(numQubits > maxSupportedQubitsForU32States()) {
		throw std::invalid_argument("Number of qubits exceeds supported range for 32-bit state indexing");
	}
	return 1u << numQubits;
}

inline size_t checkedAmplitudeElementCount(unsigned int numQubits) {
	return static_cast<size_t>(checkedStateCount(numQubits)) * 2u;
}

} // namespace tmfqs

#endif // TMFQS_CORE_STATE_SPACE_H
