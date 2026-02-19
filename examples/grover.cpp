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

		unsigned int numberOfQubits, omega;
		numberOfQubits = atoi(argv[1]);
		omega = atoi(argv[2]);

		unsigned int result = Grover(omega, numberOfQubits, true);
		cout << "Grover search result: " << result << endl;
		return 0;
	}
}
