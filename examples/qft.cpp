#include "tmfqsfs.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[]) {
	using namespace tmfqs;
	if(argc != 3) {
		std::cout << "./qft <num_qubits> <initial_state>\n";
		return 1;
	}

	const unsigned int numQubits = static_cast<unsigned int>(std::atoi(argv[1]));
	const unsigned int initState = static_cast<unsigned int>(std::atoi(argv[2]));
	QuantumRegister registerState(numQubits, initState);
	algorithms::qftInPlace(registerState);
	return 0;
}
