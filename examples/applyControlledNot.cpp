#include "tmfqsfs.h"
#include <stdlib.h>
#include <iostream>


using namespace std;

//TMFQS
int main(int argc, char *argv[]){

	if(argc != 4){
		cout << "./applyHadamard <Number of Qubits> <controlQubit> <targetQubit>" << endl;
		return 1;
	}
   else{
		int i, j;
		unsigned int numberOfQubits, targetQubit, controlQubit;
		numberOfQubits = atoi(argv[1]);
		controlQubit = atoi(argv[2]);
		targetQubit = atoi(argv[3]);

		QuantumRegister qureg(numberOfQubits);
		for(int i=0; i<numberOfQubits; i++){
			qureg.Hadamard(i);
		}
		qureg.ControlledNot(controlQubit,targetQubit);
		qureg.printStatesVector();


		return 0;
	}
}
