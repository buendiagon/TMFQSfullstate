#include <cmath>
#include <algorithm>
#include <iostream>
#include "quantumAlgorithms.h"

void quatumFourierTransform(QuantumRegister *qureg) {
   for(unsigned int j = 0; j < qureg->numQubits; j++){
      qureg->Hadamard(j);
		for (unsigned int k = 1; k < qureg->numQubits - j; k++) {
         qureg->ControlledPhaseShift(j + k, j, M_PI/static_cast<double>(1 << k)); // 1 << j is pow(2, j)
      }
   }
	// REVERSE THE REGISTER ORDER
	for (unsigned int i = 0; i < floor((qureg->numQubits)/2.0); i++){
		qureg->Swap(i, qureg->numQubits-i-1);
	}
}


unsigned int Grover(unsigned int omega, unsigned int numBits, bool verbose) {
	/*
	Perform a Grover search to find what omega is given the black box
	operator Uomega. Of course, here we know omega and make Uomega from
	that, but the principle is that we are given Uomega such that
		Uomega |x> = |x>   if x != omega
		Uomega |x> = -|x>  if x == omega
	and we want to find omega. This is the simplest appication of the
	Grover search.

	omega must be in the range 0 <= omega < pow(2, numBits).
	If verbose is true, the register will be printed prior to the measurement.
	set_srand() must be called before calling this function.
	*/

	unsigned int N = 1 << numBits; // 1 << numBits is pow(2, numBits)
	if (omega >= N){
		std::cout << "Number of bits = " << numBits << " is not enough for omega = " << omega << std::endl;
		return 0;
	}

	// Uomega is the black box operator.
	QuantumGate Uomega = QuantumGate::Identity(N);
	Uomega[omega][omega].real = -1.0;
	Uomega[omega][omega].imag = 0.0;

	// The Grover diffusion operator, 2|s><s| - I, where |s>
	// is equal superposition of all states.
	QuantumGate D(N);
	for (unsigned int i = 0; i < D.dimension; i++) {
		for (unsigned int j = 0; j < D.dimension; j++) {
			D[i][j].real = 2.0 / double(N);
			D[i][j].imag = 0.0;
			if (i == j)
				D[i][j].real -= 1.0;
		}
	}

	// // Here I define Us such that Hadamard^n * Us * Hadamard^n is the
	// // Grover diffusion operator, where Hadamard^n means the Hadamard
	// // gate applied to each qubit.
	// QuantumGate Us = QuantumGate::Identity(N) * -1.0; Us[0][0] = 1.0;

	QuantumRegister qureg(numBits); IntegerVector v;

	// Begin with equal superposition.
	for (unsigned int i = 0; i < qureg.numQubits; i++){
		qureg.Hadamard(i); v.push_back(i);
	}

	// iterate O(sqrt(N)) times. where we stop is important!
	for (unsigned int k = 0; k < (unsigned int)round(pi / (4.0*asin(1/sqrt(N)))-1.0/2.0); k++) {
		// Uomega operator is applied to the whole system
		qureg.applyGate(Uomega, v);

		// Apply the Grover diffusion operator.
		/*
		Instead of r.apply_gate(D, v), could do the following, which is more physically realistic I think.

		for (int i = 0; i < r.num_qubits; i++) r.Hadamard(i);
		r.apply_gate(Us, v);
		for (int i = 0; i < r.num_qubits; i++) r.Hadamard(i);
		*/

		qureg.applyGate(D, v);
	}

	// When printing states, you can see that the basis element
	// corresponding to omega has a much higher amplitude than
	// the rest of the basis elements.
	if(verbose) qureg.printStatesVector();

	// Collapse the system. There is a high probability that we get
	// the basis element corresponding to omega.

	// IMPLEMENTAR MEASURE ############################################
	//string s = r.measure(); // result

	// ARREGLAR  ######################################################
	// return binary_to_base10(s);
	return 0;
}
