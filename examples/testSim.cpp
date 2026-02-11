#include "tmfqsfs.h"
#include <stdlib.h>
#include <mpi.h>
#include <zfp.h>
#include <zfp/array2.hpp>
#include <iostream>


using namespace std;

int main(int argc, char *argv[]){

	if(argc != 3){
		cout << "testSim <Number of Qubits> <index>" << endl;
		return 1;
	}
   else{
		int i, j;
		unsigned int numberOfQubits, index;
		numberOfQubits = atoi(argv[1]);
		index = atoi(argv[2]);

		QuantumRegister qreg(numberOfQubits);
		qreg.printStatesVector();
		qreg.Hadamard(index);


		return 0;
	}
}
