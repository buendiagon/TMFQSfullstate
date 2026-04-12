#include "tmfqsfs.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr unsigned int kRandomPhaseSeed = 123456u;

struct ComparisonMetrics {
	double maxAbsAmplitudeError = 0.0;
	double maxAbsComponentError = 0.0;
	double rmseAmplitude = 0.0;
	double relL2 = 0.0;
	double maxAbsProbabilityError = 0.0;
	double totalProbabilityDiff = 0.0;
	tmfqs::StateIndex worstState = 0u;
};

struct SummaryRow {
	std::string circuit;
	std::string scenario;
	unsigned int qubits = 0;
	double errorForTable = 0.0;
	double worstInstanceError = 0.0;
	double rmseAmplitude = 0.0;
	double relL2 = 0.0;
	double maxAbsProbabilityError = 0.0;
	double totalProbabilityDiff = 0.0;
};

std::vector<unsigned int> parseUnsignedCsv(const std::string &csv) {
	std::vector<unsigned int> values;
	std::string token;
	for(char ch : csv) {
		if(ch == ',') {
			if(token.empty()) {
				throw std::invalid_argument("Empty value in --qubits list");
			}
			values.push_back(static_cast<unsigned int>(std::stoul(token)));
			token.clear();
			continue;
		}
		token.push_back(ch);
	}
	if(token.empty()) {
		throw std::invalid_argument("Empty trailing value in --qubits list");
	}
	values.push_back(static_cast<unsigned int>(std::stoul(token)));
	return values;
}

tmfqs::RegisterConfig makeDenseConfig(tmfqs::StorageWorkloadHint hint) {
	tmfqs::RegisterConfig cfg;
	cfg.strategy = tmfqs::StorageStrategyKind::Dense;
	cfg.workloadHint = hint;
	return cfg;
}

tmfqs::RegisterConfig makeThesisZfpConfig(tmfqs::StorageWorkloadHint hint) {
	tmfqs::RegisterConfig cfg;
	cfg.strategy = tmfqs::StorageStrategyKind::Zfp;
	cfg.workloadHint = hint;
	cfg.zfp.mode = tmfqs::ZfpCompressionMode::FixedPrecision;
	cfg.zfp.precision = 40u;
	cfg.zfp.chunkStates = 32768u;
	cfg.zfp.gateCacheSlots = 8u;
	cfg.zfp.nthreads = 4;
	cfg.zfpOverrides.mode = true;
	cfg.zfpOverrides.precision = true;
	cfg.zfpOverrides.chunkStates = true;
	cfg.zfpOverrides.gateCacheSlots = true;
	cfg.zfpOverrides.nthreads = true;
	return cfg;
}

std::vector<tmfqs::StateIndex> choosePatternStates(tmfqs::StateIndex totalStates) {
	std::vector<tmfqs::StateIndex> states;
	for(tmfqs::StateIndex x = 0;; ++x) {
		const tmfqs::StateIndex state = 8u * x + (x % 2u);
		if(state >= totalStates) {
			break;
		}
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

tmfqs::state::QuantumState runGroverState(
	unsigned int qubits,
	const std::vector<tmfqs::StateIndex> &markedStates,
	const tmfqs::RegisterConfig &cfg) {
	tmfqs::circuit::GroverCircuitOptions circuitOptions;
	circuitOptions.markedStates = tmfqs::BasisStateList(markedStates);
	circuitOptions.materializedDiffusion = false;
	tmfqs::sim::ExecutionConfig execution;
	execution.backend = cfg;
	const tmfqs::sim::RunResult run = tmfqs::sim::Simulator(execution).run(
		tmfqs::circuit::makeGrover(qubits, std::move(circuitOptions)),
		tmfqs::state::QuantumState::basis(qubits));
	return run.state;
}

tmfqs::state::QuantumState runQftPatternState(unsigned int qubits, const tmfqs::RegisterConfig &cfg) {
	const tmfqs::StateIndex totalStates = tmfqs::checkedStateCount(qubits);
	tmfqs::sim::ExecutionConfig execution;
	execution.backend = cfg;
	const tmfqs::sim::RunResult run = tmfqs::sim::Simulator(execution).run(
		tmfqs::circuit::makeQft(qubits),
		tmfqs::state::QuantumState::uniformSubset(qubits, tmfqs::BasisStateList(choosePatternStates(totalStates))));
	return run.state;
}

tmfqs::state::QuantumState runQftRandomPhaseState(unsigned int qubits, const tmfqs::RegisterConfig &cfg) {
	tmfqs::sim::ExecutionConfig execution;
	execution.backend = cfg;
	const tmfqs::sim::RunResult run = tmfqs::sim::Simulator(execution).run(
		tmfqs::circuit::makeQft(qubits),
		tmfqs::state::QuantumState::fromAmplitudes(qubits, buildRandomPhaseInput(tmfqs::checkedStateCount(qubits))));
	return run.state;
}

ComparisonMetrics compareStates(const tmfqs::state::QuantumState &dense, const tmfqs::state::QuantumState &zfp) {
	const tmfqs::experiment::StateComparison comparison = tmfqs::experiment::compareStates(
		dense,
		zfp);
	ComparisonMetrics metrics;
	metrics.maxAbsAmplitudeError = comparison.maxAbsAmplitudeError;
	metrics.maxAbsComponentError = comparison.maxAbsComponentError;
	metrics.rmseAmplitude = comparison.rmseAmplitude;
	metrics.relL2 = comparison.relL2;
	metrics.maxAbsProbabilityError = comparison.maxAbsProbabilityError;
	metrics.totalProbabilityDiff = comparison.totalProbabilityDiff;
	metrics.worstState = comparison.worstState;
	return metrics;
}

SummaryRow averageRows(
	const std::string &circuit,
	const std::string &scenario,
	unsigned int qubits,
	const std::vector<ComparisonMetrics> &runs) {
	if(runs.empty()) {
		throw std::invalid_argument("Cannot summarize an empty run set");
	}

	SummaryRow row;
	row.circuit = circuit;
	row.scenario = scenario;
	row.qubits = qubits;
	for(const ComparisonMetrics &run : runs) {
		row.errorForTable += run.maxAbsAmplitudeError;
		row.worstInstanceError = std::max(row.worstInstanceError, run.maxAbsAmplitudeError);
		row.rmseAmplitude += run.rmseAmplitude;
		row.relL2 += run.relL2;
		row.maxAbsProbabilityError += run.maxAbsProbabilityError;
		row.totalProbabilityDiff += run.totalProbabilityDiff;
	}
	const double scale = 1.0 / static_cast<double>(runs.size());
	row.errorForTable *= scale;
	row.rmseAmplitude *= scale;
	row.relL2 *= scale;
	row.maxAbsProbabilityError *= scale;
	row.totalProbabilityDiff *= scale;
	return row;
}

void printTable(
	const std::string &title,
	const std::vector<SummaryRow> &rows,
	bool includeWorstCaseColumn) {
	std::cout << title << "\n";
	if(includeWorstCaseColumn) {
		std::cout << "| Qubits | Error | Peor caso | L2 rel. | RMSE amplitud |\n";
		std::cout << "| --- | ---: | ---: | ---: | ---: |\n";
		for(const SummaryRow &row : rows) {
			std::cout << "| " << row.qubits
			          << " | " << row.errorForTable
			          << " | " << row.worstInstanceError
			          << " | " << row.relL2
			          << " | " << row.rmseAmplitude
			          << " |\n";
		}
	} else {
		std::cout << "| Qubits | Error | L2 rel. | RMSE amplitud | Delta prob. total |\n";
		std::cout << "| --- | ---: | ---: | ---: | ---: |\n";
		for(const SummaryRow &row : rows) {
			std::cout << "| " << row.qubits
			          << " | " << row.errorForTable
			          << " | " << row.relL2
			          << " | " << row.rmseAmplitude
			          << " | " << row.totalProbabilityDiff
			          << " |\n";
		}
	}
	std::cout << "\n";
}

void writeCsv(const std::string &path, const std::vector<SummaryRow> &rows) {
	std::ofstream out(path);
	if(!out) {
		throw std::runtime_error("Failed to open CSV output: " + path);
	}
	out << "circuit,scenario,qubits,error_for_table,worst_instance_error,rel_l2,rmse_amplitude,max_abs_probability_error,total_probability_diff\n";
	out << std::setprecision(17);
	for(const SummaryRow &row : rows) {
		out << row.circuit << ','
		    << row.scenario << ','
		    << row.qubits << ','
		    << row.errorForTable << ','
		    << row.worstInstanceError << ','
		    << row.relL2 << ','
		    << row.rmseAmplitude << ','
		    << row.maxAbsProbabilityError << ','
		    << row.totalProbabilityDiff << '\n';
	}
}

} // namespace

int main(int argc, char *argv[]) {
	try {
		std::vector<unsigned int> qubits{18u, 19u, 20u};
		std::string csvPath;

		for(int i = 1; i < argc; ++i) {
			const std::string arg = argv[i];
			if(arg == "--qubits") {
				if(i + 1 >= argc) {
					throw std::invalid_argument("Missing value for --qubits");
				}
				qubits = parseUnsignedCsv(argv[++i]);
				continue;
			}
			if(arg == "--csv") {
				if(i + 1 >= argc) {
					throw std::invalid_argument("Missing value for --csv");
				}
				csvPath = argv[++i];
				continue;
			}
			throw std::invalid_argument("Unknown option: " + arg);
		}

		if(!tmfqs::StorageStrategyRegistry::isAvailable(tmfqs::StorageStrategyKind::Zfp)) {
			throw std::runtime_error("ZFP backend is not available in this build");
		}

		std::vector<SummaryRow> groverSingleRows;
		std::vector<SummaryRow> groverMultiRows;
		std::vector<SummaryRow> qftPatternRows;
		std::vector<SummaryRow> qftRandomRows;
		std::vector<SummaryRow> allRows;

		for(unsigned int qubitCount : qubits) {
			const tmfqs::StateIndex stateCount = tmfqs::checkedStateCount(qubitCount);
			const tmfqs::StateIndex s1 = stateCount / 8u;
			const tmfqs::StateIndex s2 = stateCount / 2u;
			const tmfqs::StateIndex s3 = (7u * stateCount) / 8u;

			const tmfqs::RegisterConfig denseGroverCfg = makeDenseConfig(tmfqs::StorageWorkloadHint::Grover);
			const tmfqs::RegisterConfig zfpGroverCfg = makeThesisZfpConfig(tmfqs::StorageWorkloadHint::Grover);
			std::vector<ComparisonMetrics> groverSingleRuns;
			for(tmfqs::StateIndex markedState : std::vector<tmfqs::StateIndex>{s1, s2, s3}) {
				const tmfqs::state::QuantumState denseReg = runGroverState(qubitCount, {markedState}, denseGroverCfg);
				const tmfqs::state::QuantumState zfpReg = runGroverState(qubitCount, {markedState}, zfpGroverCfg);
				groverSingleRuns.push_back(compareStates(denseReg, zfpReg));
			}
			SummaryRow groverSingle = averageRows(
				"Grover",
				"Objetivo unico (promedio N/8, N/2, 7N/8)",
				qubitCount,
				groverSingleRuns);
			groverSingleRows.push_back(groverSingle);
			allRows.push_back(groverSingle);

			const tmfqs::state::QuantumState denseGroverMultiReg = runGroverState(qubitCount, {s1, s2, s3}, denseGroverCfg);
			const tmfqs::state::QuantumState zfpGroverMultiReg = runGroverState(qubitCount, {s1, s2, s3}, zfpGroverCfg);
			SummaryRow groverMulti = averageRows(
				"Grover",
				"Multiobjetivo",
				qubitCount,
				{compareStates(denseGroverMultiReg, zfpGroverMultiReg)});
			groverMultiRows.push_back(groverMulti);
			allRows.push_back(groverMulti);

			const tmfqs::RegisterConfig denseQftCfg = makeDenseConfig(tmfqs::StorageWorkloadHint::Qft);
			const tmfqs::RegisterConfig zfpQftCfg = makeThesisZfpConfig(tmfqs::StorageWorkloadHint::Qft);
			const tmfqs::state::QuantumState denseQftPatternReg = runQftPatternState(qubitCount, denseQftCfg);
			const tmfqs::state::QuantumState zfpQftPatternReg = runQftPatternState(qubitCount, zfpQftCfg);
			SummaryRow qftPattern = averageRows(
				"QFT",
				"Superposicion periodica",
				qubitCount,
				{compareStates(denseQftPatternReg, zfpQftPatternReg)});
			qftPatternRows.push_back(qftPattern);
			allRows.push_back(qftPattern);

			const tmfqs::state::QuantumState denseQftRandomReg = runQftRandomPhaseState(qubitCount, denseQftCfg);
			const tmfqs::state::QuantumState zfpQftRandomReg = runQftRandomPhaseState(qubitCount, zfpQftCfg);
			SummaryRow qftRandom = averageRows(
				"QFT",
				"Alta entropia",
				qubitCount,
				{compareStates(denseQftRandomReg, zfpQftRandomReg)});
			qftRandomRows.push_back(qftRandom);
			allRows.push_back(qftRandom);
		}

		if(!csvPath.empty()) {
			writeCsv(csvPath, allRows);
		}

		std::cout << std::scientific << std::setprecision(6);
		std::cout << "Error metric used for the thesis column: max_i |a_zfp(i) - a_dense(i)|\n";
		std::cout << "ZFP configuration: FixedPrecision, precision=40, chunkStates=32768, gateCacheSlots=8, nthreads=4\n";
		std::cout << "Reduced qubit sweep:";
		for(unsigned int qubitCount : qubits) {
			std::cout << ' ' << qubitCount;
		}
		std::cout << "\n\n";

		printTable("Grover objetivo unico", groverSingleRows, true);
		printTable("Grover multiobjetivo", groverMultiRows, false);
		printTable("QFT superposicion periodica", qftPatternRows, false);
		printTable("QFT alta entropia", qftRandomRows, false);
		return 0;
	} catch(const std::exception &ex) {
		std::cerr << "zfp_error_analysis error: " << ex.what() << '\n';
		return 1;
	}
}
