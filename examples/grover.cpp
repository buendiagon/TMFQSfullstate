#include "tmfqsfs.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using StateList = std::vector<unsigned int>;

struct CliOptions {
	unsigned int qubitCount = 0;
	StateList markedStates;
	bool verbose = false;
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

static bool parseMarkedStatesCsv(const char *text, StateList &markedStatesOut) {
	std::string token;
	for(const char ch : std::string(text)) {
		if(ch == ',') {
			unsigned int state = 0;
			if(token.empty() || !parseUnsigned(token.c_str(), state)) {
				return false;
			}
			markedStatesOut.push_back(state);
			token.clear();
			continue;
		}
		token.push_back(ch);
	}

	unsigned int state = 0;
	if(token.empty() || !parseUnsigned(token.c_str(), state)) {
		return false;
	}
	markedStatesOut.push_back(state);
	return true;
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
		<< "Usage: ./grover <num_qubits> <marked_state[,marked_state...]> "
		<< "[--verbose] [--strategy dense|blosc|zfp|auto] [--chunk-states N] "
		<< "[--cache-slots N] [--clevel N] [--nthreads N] [--threshold-mb N] "
		<< "[--zfp-mode rate|precision|accuracy] [--zfp-rate R] "
		<< "[--zfp-precision B] [--zfp-accuracy A] [--zfp-chunk-states N] "
		<< "[--zfp-cache-slots N]\n";
}

static bool parseArgs(int argc, char *argv[], CliOptions &optionsOut) {
	if(argc < 3) {
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
	if(!parseMarkedStatesCsv(argv[2], optionsOut.markedStates) || optionsOut.markedStates.empty()) {
		std::cout << "Marked states must be a comma-separated list of integers.\n";
		return false;
	}

	for(int i = 3; i < argc; ++i) {
		const std::string arg = argv[i];
		if(arg == "--verbose") {
			optionsOut.verbose = true;
			continue;
		}
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

int main(int argc, char *argv[]) {
	using namespace tmfqs;
	try {
		CliOptions options;
		if(!parseArgs(argc, argv, options)) {
			return 1;
		}

		Mt19937RandomSource randomSource;
		const algorithms::GroverConfig config{
			BasisStateList(std::move(options.markedStates)),
			options.qubitCount,
			options.verbose,
			options.registerConfig
		};
		const unsigned int result = algorithms::groverSearch(config, randomSource);
		std::cout << "Resolved strategy: "
		          << StorageStrategyRegistry::toString(
		                 StorageStrategyRegistry::resolve(options.qubitCount, options.registerConfig))
		          << "\n";
		std::cout << "Grover search result: " << result << "\n";
		return 0;
	} catch(const std::exception &ex) {
		std::cout << "Grover error: " << ex.what() << "\n";
		std::cout << "Available strategies: ";
		const auto names = tmfqs::StorageStrategyRegistry::listAvailable();
		for(size_t i = 0; i < names.size(); ++i) {
			std::cout << names[i];
			if(i + 1 < names.size()) std::cout << ", ";
		}
		std::cout << "\n";
		return 2;
	}
}
