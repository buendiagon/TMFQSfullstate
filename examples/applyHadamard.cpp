#include "tmfqsfs.h"
#include <stdlib.h>
#include <iostream>


using namespace std;

//TMFQS
int main(int argc, char *argv[]){

	if(argc != 4){
		cout << "./applyHadamard <Number of Qubits> <qubit> <init state>" << endl;
		return 1;
	}
   else{
		int i, j;
		unsigned int numQubits = 0, qubit, initState = 9;
		numQubits = atoi(argv[1]);
		qubit = atoi(argv[2]);
		initState = atoi(argv[3]);
		Amplitude amp;

		/*
		amp.real = 0.653281;
		amp.imag = 0.270598;
		qreg.printStatesVector();
		std::cout << std::endl;
		qreg.Hadamard(qubit);
		*/
		amp.real = 1.0;
		amp.imag = 0.0;
		QuantumRegister qreg(numQubits, initState, amp);
		for(int i=0; i<numQubits; i++){
			qreg.Hadamard(i);
		}
		//qreg.printStatesVector();
		//qreg.ControlledPhaseShift(controlQubit, targetQubit, theta);
		//qreg.printStatesVector();


		return 0;
	}
}
