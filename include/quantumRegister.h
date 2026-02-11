#ifndef QUANTUM_REGISTER_INCLUDE
#define QUANTUM_REGISTER_INCLUDE

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <algorithm>
#include "types.h"
#include "quantumGate.h"
#include <iomanip>

using namespace std;

class QuantumRegister {

	public:
		unsigned int numQubits, numStates;
		AmplitudesVector amplitudes;
		StatesVector states;

		//Constructors ###################################
		QuantumRegister();
		QuantumRegister(unsigned int numQubits);
		QuantumRegister(unsigned int numQubits, unsigned int initState);
		QuantumRegister(unsigned int numQubits, unsigned int initState, Amplitude amp);
		//QuantumRegister(unsigned int numQubits, bool isRandom);
		QuantumRegister(unsigned int numQubits, string filePath);
		QuantumRegister(const QuantumRegister&);


		//Accessors ######################################
		//
		int getSize();

		//Get the Magnitud or Modulus of the element i-th
		double probability(unsigned int state);

		//Get the sum of magnitudes of the statesVector
		double probabilitySumatory();


		//Get the amplitude of specific state
		Amplitude amplitude(unsigned int state);

		static StatesVector allStates();

		//Set methods ####################################
		//
		void setSize(unsigned int);

		// Fill the states vector ramdonly
		void fillStatesVector();

		//Miscelaneous methods ###########################
		//Print states vector
		void printStatesVector();
		friend std::ostream &operator << (std::ostream &os, QuantumRegister &reg);
		string getNthBit(unsigned int state, unsigned int qubit);
		int findState(unsigned int state);
		double getProbability(unsigned int state);

		//Destructor #####################################
		~QuantumRegister();


		//Quantum Gates operations
		void applyGate(QuantumGate g, IntegerVector v);
		void Hadamard(unsigned int qubit);
		void ControlledPhaseShift(unsigned int controlQubit, unsigned int targetQubit, double theta);
		void ControlledNot(unsigned int controlQubit, unsigned int targetQubit);
		void Swap(unsigned int qubit1, unsigned int qubit2);
};

#endif //QUANTUM_REGISTER_INCLUDE
