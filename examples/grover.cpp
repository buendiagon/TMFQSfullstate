#include "tmfqsfs.h"
#include <stdlib.h>
#include <iostream>
#include <stdexcept>


using namespace std;

//TMFQS
int main(int argc, char *argv[]){

	if(argc != 3){
		cout << "./grover <Number of Qubits> <marked state>" << endl;
		return 1;
	}
	else{

		unsigned int numberOfQubits;
		unsigned int omega;
		numberOfQubits = atoi(argv[1]);
		omega = atoi(argv[2]);

		try {
			unsigned int result = Grover(omega, numberOfQubits, true);
			cout << "Grover search result: " << result << endl;
			return 0;
		} catch(const std::exception &ex) {
			cout << "Grover error: " << ex.what() << endl;
			return 2;
		}
	}
}
