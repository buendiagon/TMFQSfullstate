#include "tmfqs/core/math.h"

#include <cmath>

namespace tmfqs {

Amplitude amplitudeMultiply(Amplitude a, Amplitude b) {
	return {
		a.real * b.real - a.imag * b.imag,
		a.real * b.imag + a.imag * b.real
	};
}

Amplitude amplitudeAdd(Amplitude a, Amplitude b) {
	return {a.real + b.real, a.imag + b.imag};
}

Amplitude complexExp(Amplitude exponent) {
	const double scale = std::exp(exponent.real);
	return {
		scale * std::cos(exponent.imag),
		scale * std::sin(exponent.imag)
	};
}

} // namespace tmfqs
