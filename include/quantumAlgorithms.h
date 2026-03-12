#ifndef QUANTUM_ALGORITHMS_INCLUDE
#define QUANTUM_ALGORITHMS_INCLUDE

#include "quantumRegister.h"

const double pi = acos(-1.0);

void quatumFourierTransform(QuantumRegister *qureg);
unsigned int Grover(unsigned int omega, unsigned int numBits, bool verbose);


#endif //QUANTUM_ALGORITHMS_INCLUDE
