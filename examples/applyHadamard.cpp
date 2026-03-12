#include "tmfqsfs.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[]) {
	using namespace tmfqs;
	if(argc != 4) {
		std::cout << "./applyHadamard <num_qubits> <qubit> <init_state>\n";
		return 1;
	}

	const unsigned int numQubits = static_cast<unsigned int>(std::atoi(argv[1]));
	const unsigned int qubit = static_cast<unsigned int>(std::atoi(argv[2]));
	const unsigned int initState = static_cast<unsigned int>(std::atoi(argv[3]));

	QuantumRegister registerState(numQubits, initState, {1.0, 0.0});
	registerState.applyHadamard(qubit);
	registerState.printStatesVector();
	return 0;
}
