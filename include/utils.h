#ifndef UTIL_INCLUDE
#define UTIL_INCLUDE

#include <cstdint>

#include "types.h"

// Returns a pseudo-random number in [0.0, 1.0].
double getRandomNumber();
// Sets deterministic seed for process-global RNG.
void setRandomSeed(uint32_t seed);
// Basic complex helpers used by gate/register math code.
Amplitude amplitudeAdd(Amplitude a, Amplitude b);
Amplitude amplitudeMult(Amplitude a, Amplitude b);
// Returns exp(amp.real + i * amp.imag).
Amplitude eRaisedToComplex(Amplitude amp);

#endif
