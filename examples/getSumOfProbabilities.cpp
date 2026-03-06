#include "tmfqsfs.h"
#include <stdlib.h>
#include <iostream>


using namespace std;

//TMFQS
int main(int argc, char *argv[]){

	if(argc != 2){
		cout << "./applyHadamard <Number of Qubits>" << endl;
		return 1;
	}
   else{
		unsigned int numberOfQubits;
		numberOfQubits = atoi(argv[1]);

		QuantumRegister qreg(numberOfQubits);
		for(unsigned int i = 0; i < numberOfQubits; i++){
			qreg.Hadamard(i);
		}
		qreg.printStatesVector();
		std::cout << qreg.probabilitySumatory() << std::endl;


		return 0;
	}
}
