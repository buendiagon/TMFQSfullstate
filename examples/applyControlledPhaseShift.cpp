#include "tmfqsfs.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[]) {
	using namespace tmfqs;
	if(argc != 5) {
		std::cout << "./applyControlledPhaseShift <num_qubits> <control_qubit> <target_qubit> <init_state>\n";
		return 1;
	}

	const unsigned int numQubits = static_cast<unsigned int>(std::atoi(argv[1]));
	const unsigned int controlQubit = static_cast<unsigned int>(std::atoi(argv[2]));
	const unsigned int targetQubit = static_cast<unsigned int>(std::atoi(argv[3]));
	const unsigned int initState = static_cast<unsigned int>(std::atoi(argv[4]));

	QuantumRegister registerState(numQubits, initState);
	registerState.applyControlledPhaseShift(controlQubit, targetQubit, kPi / 4.0);
	registerState.printStatesVector();
	return 0;
}
