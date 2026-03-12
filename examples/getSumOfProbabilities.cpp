#include "tmfqsfs.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[]) {
	using namespace tmfqs;
	if(argc != 2) {
		std::cout << "./getSumOfProbabilities <num_qubits>\n";
		return 1;
	}

	const unsigned int numQubits = static_cast<unsigned int>(std::atoi(argv[1]));
	QuantumRegister registerState(numQubits);
	for(unsigned int i = 0; i < numQubits; ++i) {
		registerState.applyHadamard(i);
	}
	registerState.printStatesVector();
	std::cout << registerState.totalProbability() << "\n";
	return 0;
}
