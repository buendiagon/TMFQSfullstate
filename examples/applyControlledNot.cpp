#include "tmfqsfs.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[]) {
	using namespace tmfqs;
	if(argc != 4) {
		std::cout << "./applyControlledNot <num_qubits> <control_qubit> <target_qubit>\n";
		return 1;
	}

	const unsigned int numQubits = static_cast<unsigned int>(std::atoi(argv[1]));
	const unsigned int controlQubit = static_cast<unsigned int>(std::atoi(argv[2]));
	const unsigned int targetQubit = static_cast<unsigned int>(std::atoi(argv[3]));

	QuantumRegister registerState(numQubits);
	for(unsigned int i = 0; i < numQubits; ++i) {
		registerState.applyHadamard(i);
	}
	registerState.applyControlledNot(controlQubit, targetQubit);
	registerState.printStatesVector();
	return 0;
}
