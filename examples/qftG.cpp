#include "tmfqsfs.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

using StateList = vector<unsigned int>;

// Parse unsigned integer from argv token.
static bool parseUnsigned(const char *text, unsigned int &valueOut) {
	char *end = nullptr;
	unsigned long value = strtoul(text, &end, 10);
	if (text == end || *end != '\0') {
		return false;
	}
	valueOut = static_cast<unsigned int>(value);
	return true;
}

// Keep only valid basis states and remove duplicates.
static StateList sanitizeStates(const StateList &input, unsigned int totalStates) {
	StateList states;
	for (unsigned int state : input) {
		if (state < totalStates) {
			states.push_back(state);
		}
	}
	sort(states.begin(), states.end());
	states.erase(unique(states.begin(), states.end()), states.end());
	return states;
}

// Sample one basis state according to probabilities and collapse the register.
static unsigned int measureStateAndCollapse(QuantumRegister &qureg, unsigned int totalStates) {
	double randomValue = getRandomNumber();
	double cumulativeProbability = 0.0;
	unsigned int measuredState = totalStates - 1;

	for (unsigned int basisState = 0; basisState < totalStates; ++basisState) {
		cumulativeProbability += qureg.probability(basisState);
		if (randomValue <= cumulativeProbability) {
			measuredState = basisState;
			break;
		}
	}

	fill(qureg.amplitudes.begin(), qureg.amplitudes.end(), 0.0);
	qureg.amplitudes[2 * measuredState] = 1.0;
	return measuredState;
}

// Prepare |psi> as a uniform superposition over selected basis states.
static void loadUniformSuperposition(QuantumRegister &qureg, const StateList &basisStates) {
	fill(qureg.amplitudes.begin(), qureg.amplitudes.end(), 0.0);
	const double realAmplitude = 1.0 / sqrt(static_cast<double>(basisStates.size()));
	for (unsigned int state : basisStates) {
		// amplitudes layout: [real0, imag0, real1, imag1, ...]
		qureg.amplitudes[2 * state] = realAmplitude;
		qureg.amplitudes[2 * state + 1] = 0.0;
	}
}

// Edit this function to define the input state set for qftG.
static StateList chooseInputStates(unsigned int totalStates) {
	StateList states;

	// Default example: all even states.
	for (unsigned int x = 0;; ++x) {
		unsigned int y = 8u * x + (x % 2u);
		if (y >= totalStates) break;
		states.push_back(y);
	}
	return states;
}

static void printUsage() {
	cout << "Usage: ./qftG <num_qubits>" << endl;
}

static bool parseQubitCount(int argc, char *argv[], unsigned int &qubitCountOut) {
	if (argc != 2) {
		printUsage();
		return false;
	}

	unsigned int qubitCount = 0;
	if (!parseUnsigned(argv[1], qubitCount) || qubitCount == 0) {
		cout << "Number of qubits must be an integer >= 1" << endl;
		return false;
	}
	if (qubitCount >= (sizeof(unsigned int) * 8)) {
		cout << "Number of qubits is too large for this example." << endl;
		return false;
	}

	qubitCountOut = qubitCount;
	return true;
}

static bool buildInputStates(unsigned int totalStates, StateList &selectedStatesOut) {
	selectedStatesOut = sanitizeStates(chooseInputStates(totalStates), totalStates);
	if (selectedStatesOut.empty()) {
		cout << "chooseInputStates(totalStates) returned no valid states in [0, "
		     << (totalStates - 1) << "]." << endl;
		return false;
	}
	return true;
}

static void printResult(unsigned int measuredState, unsigned int totalStates) {
	cout << "Measured state (k): " << measuredState << endl;
	cout << "Calculated r (N/k): ";
	if (measuredState == 0) {
		cout << "undefined (division by zero)" << endl;
	} else {
		cout << (static_cast<double>(totalStates) / static_cast<double>(measuredState)) << endl;
	}
}

int main(int argc, char *argv[]) {
	unsigned int qubitCount = 0;
	if (!parseQubitCount(argc, argv, qubitCount)) {
		return 1;
	}

	const unsigned int totalStates = 1u << qubitCount;

	StateList selectedStates;
	if (!buildInputStates(totalStates, selectedStates)) {
		return 1;
	}

	QuantumRegister qureg(qubitCount);
	loadUniformSuperposition(qureg, selectedStates);
	quantumFourierTransform(&qureg);

	const unsigned int measuredState = measureStateAndCollapse(qureg, totalStates);
	printResult(measuredState, totalStates);
	return 0;
}
