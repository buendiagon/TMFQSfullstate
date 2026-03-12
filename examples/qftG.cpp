#include "tmfqsfs.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using StateList = std::vector<unsigned int>;

struct CliOptions {
	unsigned int qubitCount = 0;
	tmfqs::RegisterConfig registerConfig;
};

static bool parseUnsigned(const char *text, unsigned int &valueOut) {
	char *end = nullptr;
	const unsigned long value = std::strtoul(text, &end, 10);
	if(text == end || *end != '\0') {
		return false;
	}
	valueOut = static_cast<unsigned int>(value);
	return true;
}

static bool parseDouble(const char *text, double &valueOut) {
	char *end = nullptr;
	const double value = std::strtod(text, &end);
	if(text == end || *end != '\0') {
		return false;
	}
	valueOut = value;
	return true;
}

static StateList sanitizeStates(const StateList &input, unsigned int totalStates) {
	StateList states;
	for(unsigned int state : input) {
		if(state < totalStates) {
			states.push_back(state);
		}
	}
	std::sort(states.begin(), states.end());
	states.erase(std::unique(states.begin(), states.end()), states.end());
	return states;
}

static StateList chooseInputStates(unsigned int totalStates) {
	StateList states;
	for(unsigned int x = 0;; ++x) {
		const unsigned int y = 8u * x + (x % 2u);
		if(y >= totalStates) break;
		states.push_back(y);
	}
	return states;
}

static tmfqs::StorageStrategyKind parseStrategy(const std::string &value) {
	if(value == "dense") return tmfqs::StorageStrategyKind::Dense;
	if(value == "blosc") return tmfqs::StorageStrategyKind::Blosc;
	if(value == "zfp") return tmfqs::StorageStrategyKind::Zfp;
	if(value == "auto") return tmfqs::StorageStrategyKind::Auto;
	throw std::invalid_argument("Unknown strategy: " + value);
}

static tmfqs::ZfpCompressionMode parseZfpMode(const std::string &value) {
	if(value == "rate") return tmfqs::ZfpCompressionMode::FixedRate;
	if(value == "precision") return tmfqs::ZfpCompressionMode::FixedPrecision;
	if(value == "accuracy") return tmfqs::ZfpCompressionMode::FixedAccuracy;
	throw std::invalid_argument("Unknown zfp mode: " + value);
}

static void printUsage() {
	std::cout
		<< "Usage: ./qftG <num_qubits> [--strategy dense|blosc|zfp|auto] [--chunk-states N] [--cache-slots N] [--clevel N] [--nthreads N] [--threshold-mb N] [--zfp-mode rate|precision|accuracy] [--zfp-rate R] [--zfp-precision B] [--zfp-accuracy A] [--zfp-chunk-states N] [--zfp-cache-slots N]\n";
}

static bool parseArgs(int argc, char *argv[], CliOptions &optionsOut) {
	if(argc < 2) {
		printUsage();
		return false;
	}

	if(!parseUnsigned(argv[1], optionsOut.qubitCount) || optionsOut.qubitCount == 0) {
		std::cout << "Number of qubits must be an integer >= 1\n";
		return false;
	}
	if(optionsOut.qubitCount > tmfqs::maxSupportedQubitsForU32States()) {
		std::cout << "Number of qubits is too large for this example.\n";
		return false;
	}

	for(int i = 2; i < argc; ++i) {
		const std::string arg = argv[i];
		if(arg == "--strategy") {
			if(i + 1 >= argc) return false;
			optionsOut.registerConfig.strategy = parseStrategy(argv[++i]);
			continue;
		}
		if(arg == "--chunk-states") {
			unsigned int value = 0;
			if(i + 1 >= argc || !parseUnsigned(argv[++i], value) || value == 0) return false;
			optionsOut.registerConfig.blosc.chunkStates = value;
			continue;
		}
		if(arg == "--cache-slots") {
			unsigned int value = 0;
			if(i + 1 >= argc || !parseUnsigned(argv[++i], value) || value == 0) return false;
			optionsOut.registerConfig.blosc.gateCacheSlots = value;
			continue;
		}
		if(arg == "--clevel") {
			unsigned int value = 0;
			if(i + 1 >= argc || !parseUnsigned(argv[++i], value)) return false;
			optionsOut.registerConfig.blosc.clevel = static_cast<int>(value);
			continue;
		}
		if(arg == "--nthreads") {
			unsigned int value = 0;
			if(i + 1 >= argc || !parseUnsigned(argv[++i], value) || value == 0) return false;
			optionsOut.registerConfig.blosc.nthreads = static_cast<int>(value);
			continue;
		}
		if(arg == "--threshold-mb") {
			unsigned int value = 0;
			if(i + 1 >= argc || !parseUnsigned(argv[++i], value)) return false;
			optionsOut.registerConfig.autoThresholdBytes = static_cast<size_t>(value) * 1024u * 1024u;
			continue;
		}
		if(arg == "--zfp-mode") {
			if(i + 1 >= argc) return false;
			optionsOut.registerConfig.zfp.mode = parseZfpMode(argv[++i]);
			continue;
		}
		if(arg == "--zfp-rate") {
			double value = 0.0;
			if(i + 1 >= argc || !parseDouble(argv[++i], value) || value <= 0.0) return false;
			optionsOut.registerConfig.zfp.rate = value;
			continue;
		}
		if(arg == "--zfp-precision") {
			unsigned int value = 0;
			if(i + 1 >= argc || !parseUnsigned(argv[++i], value) || value == 0u) return false;
			optionsOut.registerConfig.zfp.precision = value;
			continue;
		}
		if(arg == "--zfp-accuracy") {
			double value = 0.0;
			if(i + 1 >= argc || !parseDouble(argv[++i], value) || value <= 0.0) return false;
			optionsOut.registerConfig.zfp.accuracy = value;
			continue;
		}
		if(arg == "--zfp-chunk-states") {
			unsigned int value = 0;
			if(i + 1 >= argc || !parseUnsigned(argv[++i], value) || value == 0u) return false;
			optionsOut.registerConfig.zfp.chunkStates = value;
			continue;
		}
		if(arg == "--zfp-cache-slots") {
			unsigned int value = 0;
			if(i + 1 >= argc || !parseUnsigned(argv[++i], value) || value == 0u) return false;
			optionsOut.registerConfig.zfp.gateCacheSlots = value;
			continue;
		}
		std::cout << "Unknown option: " << arg << "\n";
		printUsage();
		return false;
	}
	return true;
}

static bool buildInputStates(unsigned int totalStates, StateList &selectedStatesOut) {
	selectedStatesOut = sanitizeStates(chooseInputStates(totalStates), totalStates);
	if(selectedStatesOut.empty()) {
		std::cout << "No valid selected states were generated.\n";
		return false;
	}
	return true;
}

int main(int argc, char *argv[]) {
	try {
		CliOptions options;
		if(!parseArgs(argc, argv, options)) {
			return 1;
		}

		const unsigned int totalStates = tmfqs::checkedStateCount(options.qubitCount);
		StateList selectedStates;
		if(!buildInputStates(totalStates, selectedStates)) {
			return 1;
		}

		tmfqs::QuantumRegister quantumRegister(options.qubitCount, options.registerConfig);
		quantumRegister.initUniformSuperposition(tmfqs::BasisStateList(std::move(selectedStates)));
		tmfqs::algorithms::qftInPlace(quantumRegister);

		tmfqs::Mt19937RandomSource randomSource;
		const unsigned int measuredState = quantumRegister.measure(randomSource);
		std::cout << "Measured state (k): " << measuredState << "\n";
		std::cout << "Calculated r (N/k): ";
		if(measuredState == 0) {
			std::cout << "undefined (division by zero)\n";
		} else {
			std::cout << (static_cast<double>(totalStates) / static_cast<double>(measuredState)) << "\n";
		}
		return 0;
	} catch(const std::exception &ex) {
		std::cout << "qftG error: " << ex.what() << "\n";
		std::cout << "Available strategies: ";
		auto names = tmfqs::StorageStrategyRegistry::listAvailable();
		for(size_t i = 0; i < names.size(); ++i) {
			std::cout << names[i];
			if(i + 1 < names.size()) std::cout << ", ";
		}
		std::cout << "\n";
		return 2;
	}
}
