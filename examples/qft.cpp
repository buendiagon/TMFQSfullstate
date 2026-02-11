#include "tmfqsfs.h"
#include <stdlib.h>
#include <iostream>
#include "utils.h"


using namespace std;

//TMFQS
int main(int argc, char *argv[]) {

	if(argc != 3)  {
		cout << "./qft <Number of Qubits> <initialState>" << endl;
		return 1;
	} else {
		int i, j;

		unsigned int numberOfQubits = 0, initState = 0;
		numberOfQubits = atoi(argv[1]);
		initState = atoi(argv[2]);
		QuantumRegister qureg(numberOfQubits, initState);

		//cout << endl << "Initial amplitudes" << endl;
		//qureg.printStatesVector();
		quatumFourierTransform(&qureg);
		//qureg.printStatesVector();

		return 0;
	}
}
