#include "tmfqsfs.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(int argc, char *argv[]) {
	using namespace tmfqs;
	if(argc != 3) {
		std::cout << "./grover <num_qubits> <marked_state>\n";
		return 1;
	}

	const unsigned int numQubits = static_cast<unsigned int>(std::atoi(argv[1]));
	const unsigned int markedState = static_cast<unsigned int>(std::atoi(argv[2]));

	try {
		Mt19937RandomSource randomSource;
		const algorithms::GroverConfig config{markedState, numQubits, true};
		const unsigned int result = algorithms::groverSearch(config, randomSource);
		std::cout << "Grover search result: " << result << "\n";
		return 0;
	} catch(const std::exception &ex) {
		std::cout << "Grover error: " << ex.what() << "\n";
		return 2;
	}
}
