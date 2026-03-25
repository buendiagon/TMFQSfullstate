#ifndef TMFQS_TEST_INTEGRATION_COMMON_TEST_HELPERS_H
#define TMFQS_TEST_INTEGRATION_COMMON_TEST_HELPERS_H

#include <cmath>
#include <sstream>
#include <stdexcept>

#include "tmfqsfs.h"

namespace tmfqs_test {

[[noreturn]] inline void fail(const char *expr, const char *file, int line) {
	std::ostringstream message;
	message << file << ":" << line << ": test assertion failed: " << expr;
	throw std::runtime_error(message.str());
}

inline void require(bool condition, const char *expr, const char *file, int line) {
	if(!condition) {
		fail(expr, file, line);
	}
}

inline bool approxEqual(double a, double b, double tol = 1e-10) {
	return std::fabs(a - b) < tol;
}

inline void assertAmplitudeClose(const tmfqs::Amplitude &lhs, const tmfqs::Amplitude &rhs, double tol) {
	require(approxEqual(lhs.real, rhs.real, tol), "approxEqual(lhs.real, rhs.real, tol)", __FILE__, __LINE__);
	require(approxEqual(lhs.imag, rhs.imag, tol), "approxEqual(lhs.imag, rhs.imag, tol)", __FILE__, __LINE__);
}

inline void assertRegistersClose(const tmfqs::QuantumRegister &lhs, const tmfqs::QuantumRegister &rhs, double tol) {
	require(lhs.stateCount() == rhs.stateCount(), "lhs.stateCount() == rhs.stateCount()", __FILE__, __LINE__);
	for(unsigned int s = 0; s < lhs.stateCount(); ++s) {
		assertAmplitudeClose(lhs.amplitude(s), rhs.amplitude(s), tol);
	}
	require(
		approxEqual(lhs.totalProbability(), rhs.totalProbability(), tol),
		"approxEqual(lhs.totalProbability(), rhs.totalProbability(), tol)",
		__FILE__,
		__LINE__);
}

} // namespace tmfqs_test

#define TMFQS_TEST_REQUIRE(expr) ::tmfqs_test::require((expr), #expr, __FILE__, __LINE__)

#endif // TMFQS_TEST_INTEGRATION_COMMON_TEST_HELPERS_H
