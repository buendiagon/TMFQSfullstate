#include "tmfqsfs.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "stateSpace.h"

using namespace std;

using StateList = vector<unsigned int>;

// CLI options for qftG: qubit count plus runtime storage configuration.
struct CliOptions {
	unsigned int qubitCount = 0;
	RegisterConfig registerConfig;
};

// Strict unsigned integer parser used for command-line options.
static bool parseUnsigned(const char *text, unsigned int &valueOut) {
	char *end = nullptr;
	unsigned long value = strtoul(text, &end, 10);
	if (text == end || *end != '\0') {
		return false;
	}
	valueOut = static_cast<unsigned int>(value);
	return true;
}

// Keep only valid states, sort them, and remove duplicates.
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

// Sample one basis state according to current probability distribution.
static unsigned int measureState(QuantumRegister &qureg, unsigned int totalStates) {
	(void)totalStates;
	return qureg.measure();
}

// Demo input set used by this qftG variant (all even basis states).
static StateList chooseInputStates(unsigned int totalStates) {
	StateList states;
	for (unsigned int x = 0;; ++x) {
		unsigned int y = 2 * x;
		if (y >= totalStates) break;
		states.push_back(y);
	}
	return states;
}

static void printUsage() {
	cout << "Usage: ./qftG <num_qubits> [--strategy dense|blosc|auto] [--chunk-states N] [--cache-slots N] [--clevel N] [--nthreads N] [--threshold-mb N]" << endl;
}

// Runtime strategy parser for --strategy option.
static StorageStrategyKind parseStrategy(const std::string &value) {
	if(value == "dense") return StorageStrategyKind::Dense;
	if(value == "blosc") return StorageStrategyKind::Blosc;
	if(value == "auto") return StorageStrategyKind::Auto;
	throw std::invalid_argument("Unknown strategy: " + value);
}

// Parse all CLI flags and fill CliOptions.
static bool parseArgs(int argc, char *argv[], CliOptions &optionsOut) {
	if (argc < 2) {
		printUsage();
		return false;
	}

	if (!parseUnsigned(argv[1], optionsOut.qubitCount) || optionsOut.qubitCount == 0) {
		cout << "Number of qubits must be an integer >= 1" << endl;
		return false;
	}
	if (optionsOut.qubitCount > maxSupportedQubitsForU32States()) {
		cout << "Number of qubits is too large for this example." << endl;
		return false;
	}

	for(int i = 2; i < argc; ++i) {
		string arg = argv[i];
		if(arg == "--strategy") {
			if(i + 1 >= argc) {
				cout << "Missing value for --strategy" << endl;
				return false;
			}
			optionsOut.registerConfig.strategy = parseStrategy(argv[++i]);
			continue;
		}
		if(arg == "--chunk-states") {
			if(i + 1 >= argc) {
				cout << "Missing value for --chunk-states" << endl;
				return false;
			}
			unsigned int value = 0;
			if(!parseUnsigned(argv[++i], value) || value == 0) {
				cout << "Invalid --chunk-states value" << endl;
				return false;
			}
			optionsOut.registerConfig.blosc.chunkStates = value;
			continue;
		}
		if(arg == "--cache-slots") {
			if(i + 1 >= argc) {
				cout << "Missing value for --cache-slots" << endl;
				return false;
			}
			unsigned int value = 0;
			if(!parseUnsigned(argv[++i], value) || value == 0) {
				cout << "Invalid --cache-slots value" << endl;
				return false;
			}
			optionsOut.registerConfig.blosc.gateCacheSlots = value;
			continue;
		}
		if(arg == "--clevel") {
			if(i + 1 >= argc) {
				cout << "Missing value for --clevel" << endl;
				return false;
			}
			unsigned int value = 0;
			if(!parseUnsigned(argv[++i], value)) {
				cout << "Invalid --clevel value" << endl;
				return false;
			}
			optionsOut.registerConfig.blosc.clevel = static_cast<int>(value);
			continue;
		}
		if(arg == "--nthreads") {
			if(i + 1 >= argc) {
				cout << "Missing value for --nthreads" << endl;
				return false;
			}
			unsigned int value = 0;
			if(!parseUnsigned(argv[++i], value) || value == 0) {
				cout << "Invalid --nthreads value" << endl;
				return false;
			}
			optionsOut.registerConfig.blosc.nthreads = static_cast<int>(value);
			continue;
		}
		if(arg == "--threshold-mb") {
			if(i + 1 >= argc) {
				cout << "Missing value for --threshold-mb" << endl;
				return false;
			}
			unsigned int value = 0;
			if(!parseUnsigned(argv[++i], value)) {
				cout << "Invalid --threshold-mb value" << endl;
				return false;
			}
			optionsOut.registerConfig.autoThresholdBytes = static_cast<size_t>(value) * 1024u * 1024u;
			continue;
		}

		cout << "Unknown option: " << arg << endl;
		printUsage();
		return false;
	}

	return true;
}

// Build sanitized input state list and validate it is non-empty.
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
	CliOptions options;
	try {
		if (!parseArgs(argc, argv, options)) {
			return 1;
		}

		const unsigned int totalStates = checkedStateCount(options.qubitCount);
		StateList selectedStates;
		if (!buildInputStates(totalStates, selectedStates)) {
			return 1;
		}

			QuantumRegister qureg(options.qubitCount, options.registerConfig);
			qureg.initUniformSuperposition(BasisStateList(std::move(selectedStates)));

		// Core algorithm execution is backend-agnostic; storage strategy is runtime-selected.
		quantumFourierTransform(qureg);

		const unsigned int measuredState = measureState(qureg, totalStates);
		printResult(measuredState, totalStates);
		return 0;
	}
	catch(const std::exception &ex) {
		cout << "qftG error: " << ex.what() << endl;
		cout << "Available strategies: ";
		auto names = listAvailableStrategies();
		for(size_t i = 0; i < names.size(); ++i) {
			cout << names[i];
			if(i + 1 < names.size()) cout << ", ";
		}
		cout << endl;
		return 2;
	}
}
