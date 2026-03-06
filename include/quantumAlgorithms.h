#ifndef QUANTUM_ALGORITHMS_INCLUDE
#define QUANTUM_ALGORITHMS_INCLUDE

#include "quantumRegister.h"

// Shared math constant for angle-based algorithms and gates.
inline constexpr double pi = 3.14159265358979323846;

// In-place quantum Fourier transform over the whole register.
void quantumFourierTransform(QuantumRegister &qureg);
// Runs Grover search and returns the measured candidate index.
unsigned int Grover(unsigned int omega, unsigned int numBits, bool verbose);


#endif //QUANTUM_ALGORITHMS_INCLUDE
