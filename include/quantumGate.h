#ifndef QUANTUM_GATE_INCLUDE
#define QUANTUM_GATE_INCLUDE

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "types.h"
#include "utils.h"

using namespace std;

class QuantumGate {

	private:
		Amplitude **matrix;

	public:
		unsigned int dimension;
		QuantumGate();
		QuantumGate(unsigned int dimension);
		Amplitude * operator[](unsigned int i);
		QuantumGate operator*(Amplitude x);
		QuantumGate operator*(QuantumGate &qg);
		void printQuantumGate();
		friend std::ostream &operator<<(std::ostream &os, QuantumGate &qg);

		static QuantumGate Identity(unsigned int dimension);
		static QuantumGate Hadamard();
		static QuantumGate PauliX();
		static QuantumGate PauliY();
		static QuantumGate PauliZ();
		static QuantumGate PhaseShift(double theta);
		static QuantumGate PiOverEight();
		static QuantumGate ControlledNot();
		static QuantumGate Toffoli();
		static QuantumGate ControlledPhaseShift(double theta);
		static QuantumGate Swap();
		static QuantumGate Ising(double theta);
		static QuantumGate QFT(unsigned int num_qubits);
		static QuantumGate IQFT(unsigned int num_qubits);

		// Matrices to be created upon initialization.
		// const static QuantumGate MHadamard;

};

// For left multiplication.
QuantumGate operator*(Amplitude x, QuantumGate &U);

#endif
