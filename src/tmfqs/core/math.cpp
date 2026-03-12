#include "tmfqs/core/math.h"

#include <cmath>

namespace tmfqs {

/** @brief Multiplies two complex amplitudes. */
Amplitude amplitudeMultiply(Amplitude a, Amplitude b) {
	// (ar + i*ai) * (br + i*bi)
	return {
		a.real * b.real - a.imag * b.imag,
		a.real * b.imag + a.imag * b.real
	};
}

/** @brief Adds two complex amplitudes. */
Amplitude amplitudeAdd(Amplitude a, Amplitude b) {
	return {a.real + b.real, a.imag + b.imag};
}

/** @brief Computes complex exponential `e^(x + i*y)`. */
Amplitude complexExp(Amplitude exponent) {
	// e^(x + i*y) = e^x * (cos(y) + i*sin(y))
	const double scale = std::exp(exponent.real);
	return {
		scale * std::cos(exponent.imag),
		scale * std::sin(exponent.imag)
	};
}

} // namespace tmfqs
