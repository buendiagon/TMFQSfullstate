#ifndef QUANTUM_ALGORITHMS_INCLUDE
#define QUANTUM_ALGORITHMS_INCLUDE

#include "quantumRegister.h"

inline constexpr double pi = 3.14159265358979323846;

void quantumFourierTransform(QuantumRegister &qureg);
unsigned int Grover(unsigned int omega, unsigned int numBits, bool verbose);


#endif //QUANTUM_ALGORITHMS_INCLUDE
