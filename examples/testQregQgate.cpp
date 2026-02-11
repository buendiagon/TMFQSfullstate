#include "tmfqsfs.h"
#include <stdlib.h>
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
		Amplitude amp1;
		QuantumGate g1, g2, h1(2), h2(2), cps1, cps2;
		/*
		g2 = g1.Identity(numberOfQubits);
		g2.printQuantumGate();
		cout << endl << endl;
		h2 = h1.Hadamard();
		h2.printQuantumGate();
		*/
		/*
		cps2 = cps1.ControlledPhaseShift(90);
		cps2.printQuantumGate();
		*/

		QuantumRegister qreg1;
		qreg1.setSize(numberOfQubits);
		//QuantumRegister qreg1(numberOfQubits);
		qreg1.fillStatesVector();
		cout << qreg1.getSize() << endl;
		qreg1.printStatesVector();
		//amp1 = qreg1.amplitude(index);
		//cout << amp1.real << amp1.imag << endl;

		/*
		cout << "Instantiating by copy" << endl;
		QuantumRegister qreg2(qreg1);
		cout << qreg2.getSize() << endl;
		cout << "Printing qreg2" << endl;
		qreg2.printStatesVector();
		*/


		return 0;
	}
}
