#include <cmath>
#include <algorithm>
#include <iostream>
#include "quantumAlgorithms.h"
#include "stateSpace.h"
#include "utils.h"

// In-place QFT using the standard sequence:
// Hadamard on each qubit, controlled phase rotations, then final bit-reversal swaps.
void quantumFourierTransform(QuantumRegister &qureg) {
	unsigned int numQubits = qureg.qubitCount();
   for(unsigned int j = 0; j < numQubits; j++){
      qureg.Hadamard(j);
		for (unsigned int k = 1; k < numQubits - j; k++) {
         qureg.ControlledPhaseShift(j + k, j, M_PI/static_cast<double>(1 << k)); // 1 << j is pow(2, j)
      }
   }
	// REVERSE THE REGISTER ORDER
	for (unsigned int i = 0; i < floor((numQubits)/2.0); i++){
		qureg.Swap(i, numQubits - i - 1);
	}
}


// Basic Grover search implementation over 2^numBits states.
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

	unsigned int N = checkedStateCount(numBits);
	if (omega >= N){
		std::cout << "Number of bits = " << numBits << " is not enough for omega = " << omega << std::endl;
		return 0;
	}

	// U_omega flips the phase of the marked basis state |omega>.
	QuantumGate Uomega = QuantumGate::Identity(N);
	Uomega[omega][omega].real = -1.0;
	Uomega[omega][omega].imag = 0.0;

	// Diffusion operator D = 2|s><s| - I.
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

	// Start from uniform superposition.
	for (unsigned int i = 0; i < qureg.qubitCount(); i++){
		qureg.Hadamard(i); v.push_back(i);
	}

	// Iterate the Grover operator approximately pi/4*sqrt(N) times.
	for (unsigned int k = 0; k < (unsigned int)round(M_PI / (4.0*asin(1.0/sqrt((double)N)))-0.5); k++) {
		// Apply oracle.
		qureg.applyGate(Uomega, v);

		// Apply diffusion.
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

	// Measurement: sample from |amplitude|^2 distribution.
	double rnd = getRandomNumber();
	double cumulativeProb = 0.0;
	for (unsigned int s = 0; s < N; s++) {
		Amplitude amp = qureg.amplitude(s);
		double prob = amp.real * amp.real + amp.imag * amp.imag;
		cumulativeProb += prob;
		if (rnd <= cumulativeProb) {
			return s;
		}
	}
	// Fallback: return last state (should not normally reach here)
	return N - 1;
}
