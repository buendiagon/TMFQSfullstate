#include "tmfqsfs.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr unsigned int kRandomPhaseSeed = 123456u;

enum class WorkloadKind {
	QftPattern,
	QftHighEntropy,
	GroverSingleN8,
	GroverSingleN2,
	GroverSingle7N8,
	GroverMulti,
	GroverLazySingleN8,
	GroverLazySingleN2,
	GroverLazySingle7N8,
	GroverLazyMulti
};

struct CliOptions {
	unsigned int qubits = 0u;
	WorkloadKind workload = WorkloadKind::QftPattern;
	tmfqs::StorageStrategyKind strategy = tmfqs::StorageStrategyKind::Zfp;
};

bool parseUnsigned(const std::string &value, unsigned int &out) {
	try {
		size_t consumed = 0u;
		unsigned long parsed = std::stoul(value, &consumed);
		if(consumed != value.size()) return false;
		out = static_cast<unsigned int>(parsed);
		return true;
	} catch(...) {
		return false;
	}
}

tmfqs::StorageStrategyKind parseStrategy(const std::string &value) {
	if(value == "dense") return tmfqs::StorageStrategyKind::Dense;
	if(value == "blosc") return tmfqs::StorageStrategyKind::Blosc;
	if(value == "zfp") return tmfqs::StorageStrategyKind::Zfp;
	throw std::invalid_argument("Unknown strategy: " + value);
}

WorkloadKind parseWorkload(const std::string &value) {
	if(value == "qft_pattern") return WorkloadKind::QftPattern;
	if(value == "qft_high_entropy") return WorkloadKind::QftHighEntropy;
	if(value == "grover_single_n8") return WorkloadKind::GroverSingleN8;
	if(value == "grover_single_n2") return WorkloadKind::GroverSingleN2;
	if(value == "grover_single_7n8") return WorkloadKind::GroverSingle7N8;
	if(value == "grover_multi") return WorkloadKind::GroverMulti;
	if(value == "grover_lazy_single_n8") return WorkloadKind::GroverLazySingleN8;
	if(value == "grover_lazy_single_n2") return WorkloadKind::GroverLazySingleN2;
	if(value == "grover_lazy_single_7n8") return WorkloadKind::GroverLazySingle7N8;
	if(value == "grover_lazy_multi") return WorkloadKind::GroverLazyMulti;
	throw std::invalid_argument("Unknown workload: " + value);
}

const char *workloadName(WorkloadKind workload) {
	switch(workload) {
		case WorkloadKind::QftPattern: return "qft_pattern";
		case WorkloadKind::QftHighEntropy: return "qft_high_entropy";
		case WorkloadKind::GroverSingleN8: return "grover_single_n8";
		case WorkloadKind::GroverSingleN2: return "grover_single_n2";
		case WorkloadKind::GroverSingle7N8: return "grover_single_7n8";
		case WorkloadKind::GroverMulti: return "grover_multi";
		case WorkloadKind::GroverLazySingleN8: return "grover_lazy_single_n8";
		case WorkloadKind::GroverLazySingleN2: return "grover_lazy_single_n2";
		case WorkloadKind::GroverLazySingle7N8: return "grover_lazy_single_7n8";
		case WorkloadKind::GroverLazyMulti: return "grover_lazy_multi";
	}
	return "unknown";
}

bool isGroverWorkload(WorkloadKind workload) {
	return workload != WorkloadKind::QftPattern && workload != WorkloadKind::QftHighEntropy;
}

bool isLazyGroverWorkload(WorkloadKind workload) {
	return workload == WorkloadKind::GroverLazySingleN8 ||
	       workload == WorkloadKind::GroverLazySingleN2 ||
	       workload == WorkloadKind::GroverLazySingle7N8 ||
	       workload == WorkloadKind::GroverLazyMulti;
}

void printUsage() {
	std::cout
		<< "Usage: ./experiment_compare --qubits N --workload NAME --strategy blosc|zfp\n"
		<< "Workloads: qft_pattern, qft_high_entropy, grover_single_n8, grover_single_n2, "
		<< "grover_single_7n8, grover_multi, grover_lazy_single_n8, grover_lazy_single_n2, "
		<< "grover_lazy_single_7n8, grover_lazy_multi\n";
}

bool parseArgs(int argc, char *argv[], CliOptions &optionsOut) {
	for(int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if(arg == "--qubits") {
			if(i + 1 >= argc || !parseUnsigned(argv[++i], optionsOut.qubits) || optionsOut.qubits == 0u) {
				return false;
			}
			continue;
		}
		if(arg == "--workload") {
			if(i + 1 >= argc) return false;
			optionsOut.workload = parseWorkload(argv[++i]);
			continue;
		}
		if(arg == "--strategy") {
			if(i + 1 >= argc) return false;
			optionsOut.strategy = parseStrategy(argv[++i]);
			continue;
		}
		return false;
	}
	return optionsOut.qubits > 0u;
}

tmfqs::RegisterConfig makeConfig(tmfqs::StorageStrategyKind strategy, tmfqs::StorageWorkloadHint hint) {
	tmfqs::RegisterConfig cfg;
	cfg.strategy = strategy;
	cfg.workloadHint = hint;

	cfg.blosc.chunkStates = 32768u;
	cfg.blosc.gateCacheSlots = 8u;
	cfg.blosc.clevel = 1;
	cfg.blosc.nthreads = 4;
	cfg.blosc.compcode = 1;
	cfg.blosc.useShuffle = true;

	cfg.zfp.mode = tmfqs::ZfpCompressionMode::FixedPrecision;
	cfg.zfp.precision = 40u;
	cfg.zfp.chunkStates = 32768u;
	cfg.zfp.gateCacheSlots = 8u;
	cfg.zfp.nthreads = 4;
	return cfg;
}

std::vector<tmfqs::StateIndex> choosePatternStates(tmfqs::StateIndex totalStates) {
	std::vector<tmfqs::StateIndex> states;
	for(tmfqs::StateIndex x = 0;; ++x) {
		const tmfqs::StateIndex state = 8u * x + (x % 2u);
		if(state >= totalStates) break;
		states.push_back(state);
	}
	return states;
}

tmfqs::AmplitudesVector buildRandomPhaseInput(tmfqs::StateIndex totalStates) {
	std::mt19937 randomGenerator(kRandomPhaseSeed);
	std::uniform_real_distribution<double> phaseDistribution(0.0, 2.0 * std::acos(-1.0));
	const double norm = 1.0 / std::sqrt(static_cast<double>(totalStates));
	tmfqs::AmplitudesVector amplitudes(static_cast<size_t>(totalStates) * 2u, 0.0);
	for(tmfqs::StateIndex state = 0; state < totalStates; ++state) {
		const double phase = phaseDistribution(randomGenerator);
		amplitudes[static_cast<size_t>(state) * 2u] = norm * std::cos(phase);
		amplitudes[static_cast<size_t>(state) * 2u + 1u] = norm * std::sin(phase);
	}
	return amplitudes;
}

tmfqs::BasisStateList markedStatesFor(WorkloadKind workload, tmfqs::StateIndex stateCount) {
	const tmfqs::StateIndex n8 = stateCount / 8u;
	const tmfqs::StateIndex n2 = stateCount / 2u;
	const tmfqs::StateIndex n7 = (7u * stateCount) / 8u;
	switch(workload) {
		case WorkloadKind::GroverSingleN8:
		case WorkloadKind::GroverLazySingleN8:
			return tmfqs::BasisStateList({n8});
		case WorkloadKind::GroverSingleN2:
		case WorkloadKind::GroverLazySingleN2:
			return tmfqs::BasisStateList({n2});
		case WorkloadKind::GroverSingle7N8:
		case WorkloadKind::GroverLazySingle7N8:
			return tmfqs::BasisStateList({n7});
		case WorkloadKind::GroverMulti:
		case WorkloadKind::GroverLazyMulti:
			return tmfqs::BasisStateList({n8, n2, n7});
		default:
			throw std::invalid_argument("Marked states requested for non-Grover workload");
	}
}

tmfqs::state::QuantumState runWorkload(unsigned int qubits, WorkloadKind workload, const tmfqs::RegisterConfig &cfg) {
	tmfqs::sim::ExecutionConfig execution;
	execution.backend = cfg;
	const tmfqs::StateIndex stateCount = tmfqs::checkedStateCount(qubits);

	if(isGroverWorkload(workload)) {
		tmfqs::circuit::GroverCircuitOptions options;
		options.markedStates = markedStatesFor(workload, stateCount);
		options.materializedDiffusion = !isLazyGroverWorkload(workload);
		const tmfqs::circuit::Circuit circuit = tmfqs::circuit::makeGrover(qubits, std::move(options));
		return tmfqs::sim::Simulator(execution).run(circuit, tmfqs::state::QuantumState::basis(qubits)).state;
	}

	const tmfqs::circuit::Circuit circuit = tmfqs::circuit::makeQft(qubits);
	if(workload == WorkloadKind::QftHighEntropy) {
		return tmfqs::sim::Simulator(execution)
			.run(circuit, tmfqs::state::QuantumState::fromAmplitudes(qubits, buildRandomPhaseInput(stateCount)))
			.state;
	}
	return tmfqs::sim::Simulator(execution)
		.run(circuit, tmfqs::state::QuantumState::uniformSubset(qubits, tmfqs::BasisStateList(choosePatternStates(stateCount))))
		.state;
}

const char *accuracyKind(tmfqs::StorageStrategyKind strategy) {
	switch(strategy) {
		case tmfqs::StorageStrategyKind::Dense: return "reference";
		case tmfqs::StorageStrategyKind::Blosc: return "lossless";
		case tmfqs::StorageStrategyKind::Zfp: return "lossy";
		case tmfqs::StorageStrategyKind::Auto: return "auto";
	}
	return "unknown";
}

} // namespace

int main(int argc, char *argv[]) {
	try {
		CliOptions options;
		if(!parseArgs(argc, argv, options)) {
			printUsage();
			return 1;
		}
		if(options.strategy == tmfqs::StorageStrategyKind::Dense) {
			throw std::invalid_argument("experiment_compare compares dense reference against blosc or zfp");
		}

		const tmfqs::StorageWorkloadHint hint =
			isGroverWorkload(options.workload) ? tmfqs::StorageWorkloadHint::Grover : tmfqs::StorageWorkloadHint::Qft;
		const tmfqs::state::QuantumState reference =
			runWorkload(options.qubits, options.workload, makeConfig(tmfqs::StorageStrategyKind::Dense, hint));
		const tmfqs::state::QuantumState candidate =
			runWorkload(options.qubits, options.workload, makeConfig(options.strategy, hint));
		const tmfqs::experiment::StateComparison comparison = tmfqs::experiment::compareStates(reference, candidate);

		std::cout << std::setprecision(17)
		          << "qubits,workload,strategy,accuracy_kind,bitwise_equal,max_abs_amplitude_error,"
		          << "max_abs_component_error,rel_l2,rmse_amplitude,max_abs_probability_error,"
		          << "total_probability_diff,worst_state\n"
		          << options.qubits << ','
		          << workloadName(options.workload) << ','
		          << tmfqs::StorageStrategyRegistry::toString(options.strategy) << ','
		          << accuracyKind(options.strategy) << ','
		          << (comparison.bitwiseEqual ? "true" : "false") << ','
		          << comparison.maxAbsAmplitudeError << ','
		          << comparison.maxAbsComponentError << ','
		          << comparison.relL2 << ','
		          << comparison.rmseAmplitude << ','
		          << comparison.maxAbsProbabilityError << ','
		          << comparison.totalProbabilityDiff << ','
		          << comparison.worstState << '\n';
		return 0;
	} catch(const std::exception &ex) {
		std::cerr << "experiment_compare error: " << ex.what() << '\n';
		return 2;
	}
}
