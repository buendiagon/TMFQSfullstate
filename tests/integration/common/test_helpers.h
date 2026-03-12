#ifndef TMFQS_TEST_INTEGRATION_COMMON_TEST_HELPERS_H
#define TMFQS_TEST_INTEGRATION_COMMON_TEST_HELPERS_H

#include <cassert>
#include <cmath>

#include "tmfqsfs.h"

namespace tmfqs_test {

inline bool approxEqual(double a, double b, double tol = 1e-10) {
	return std::fabs(a - b) < tol;
}

inline void assertAmplitudeClose(const tmfqs::Amplitude &lhs, const tmfqs::Amplitude &rhs, double tol) {
	assert(approxEqual(lhs.real, rhs.real, tol));
	assert(approxEqual(lhs.imag, rhs.imag, tol));
}

inline void assertRegistersClose(const tmfqs::QuantumRegister &lhs, const tmfqs::QuantumRegister &rhs, double tol) {
	assert(lhs.stateCount() == rhs.stateCount());
	for(unsigned int s = 0; s < lhs.stateCount(); ++s) {
		assertAmplitudeClose(lhs.amplitude(s), rhs.amplitude(s), tol);
	}
	assert(approxEqual(lhs.totalProbability(), rhs.totalProbability(), tol));
}

} // namespace tmfqs_test

#endif // TMFQS_TEST_INTEGRATION_COMMON_TEST_HELPERS_H
