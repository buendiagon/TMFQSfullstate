#include "tmfqsfs.h"
#include <stdlib.h>
#include <iostream>


using namespace std;

//TMFQS
int main(int argc, char *argv[]){

	if(argc != 5){
		cout << "./applyControlledPhaseShift <Number of Qubits> <controlQubit> <targetQubit> <initState>" << endl;
		return 1;
	}
   else{
		int i, j;
		unsigned int numQubits = 0, controlQubit, targetQubit, initState;
		numQubits = atoi(argv[1]);
		controlQubit = atoi(argv[2]);
      targetQubit = atoi(argv[3]);
		initState = atoi(argv[4]);

		/*
		qreg.printStatesVector();
		for(int i=0; i<numQubits; i++){
			qreg.Hadamard(i);
		}
		*/
		QuantumRegister qreg(numQubits, initState);
		qreg.ControlledPhaseShift(controlQubit, targetQubit, pi/double(1 << 2));
		qreg.printStatesVector();
		//qreg.Hadamard(qubit);
		//qreg.printStatesVector();


		return 0;
	}
}
