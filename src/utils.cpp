#include <cmath>
#include <random>

#include "utils.h"

namespace {
// Process-global RNG used by measurement and randomized test paths.
std::random_device randomDevice;
std::mt19937 randomNumberEngine(randomDevice());
std::uniform_real_distribution<> uniformRealDistribution(0.0, 1.0);
}

// Uniform random double in [0, 1).
double getRandomNumber() {
	return uniformRealDistribution(randomNumberEngine);
}

void setRandomSeed(uint32_t seed) {
	randomNumberEngine.seed(seed);
}

// Complex multiplication for custom Amplitude type.
Amplitude amplitudeMult(Amplitude a, Amplitude b){
	Amplitude result;
	result.real = a.real * b.real - a.imag * b.imag;
	result.imag = a.real * b.imag + a.imag * b.real;
	return result;
}

// Complex addition for custom Amplitude type.
Amplitude amplitudeAdd(Amplitude a, Amplitude b){
	Amplitude result;
	result.real = a.real + b.real;
	result.imag = a.imag + b.imag;
	return result;
}

// Computes e^(a.real + i*a.imag).
Amplitude eRaisedToComplex(Amplitude amp){
	Amplitude result;
	result.real = std::exp(amp.real) * std::cos(amp.imag);
	result.imag = std::exp(amp.real) * std::sin(amp.imag);
	return result;
}
