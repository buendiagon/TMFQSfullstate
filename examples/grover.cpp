#include "tmfqsfs.h"
#include <stdlib.h>
#include <iostream>


using namespace std;

//TMFQS
int main(int argc, char *argv[]){

	if(argc != 3){
		cout << "./grover <Number of Qubits> <marked state>" << endl;
		return 1;
	}
   else{
		int i, j;
		unsigned int numberOfQubits, omega;
		numberOfQubits = atoi(argv[1]);
		omega = atoi(argv[2]);

		QuantumRegister qureg(numberOfQubits);
		quatumFourierTransform(&qureg);
		Grover(omega, numberOfQubits, true);
		//qureg.printStatesVector();
	}
}
