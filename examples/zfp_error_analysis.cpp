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

std::vector<tmfqs::StateIndex> choosePatternStates(unsigned int totalStates) {
	std::vector<tmfqs::StateIndex> states;
	for(unsigned int x = 0;; ++x) {
		const unsigned int state = 8u * x + (x % 2u);
		if(state >= totalStates) {
			break;
		}
		states.push_back(state);
	}
	return states;
}

tmfqs::AmplitudesVector buildRandomPhaseInput(unsigned int totalStates) {
	std::mt19937 randomGenerator(kRandomPhaseSeed);
	std::uniform_real_distribution<double> phaseDistribution(0.0, 2.0 * std::acos(-1.0));
	const double norm = 1.0 / std::sqrt(static_cast<double>(totalStates));
	tmfqs::AmplitudesVector amplitudes(static_cast<size_t>(totalStates) * 2u, 0.0);
	for(unsigned int state = 0; state < totalStates; ++state) {
		const double phase = phaseDistribution(randomGenerator);
		amplitudes[static_cast<size_t>(state) * 2u] = norm * std::cos(phase);
		amplitudes[static_cast<size_t>(state) * 2u + 1u] = norm * std::sin(phase);
	}
	return amplitudes;
}

unsigned int computeGroverIterations(unsigned int stateCount, size_t markedCount) {
	if(markedCount == 0u) {
		throw std::invalid_argument("Grover case requires at least one marked state");
	}
	const double idealIterations =
		(tmfqs::kPi / 4.0) *
		std::sqrt(static_cast<double>(stateCount) / static_cast<double>(markedCount));
	return static_cast<unsigned int>(std::floor(idealIterations));
}

tmfqs::QuantumRegister runGroverState(
	unsigned int qubits,
	const std::vector<tmfqs::StateIndex> &markedStates,
	const tmfqs::RegisterConfig &cfg) {
	const unsigned int stateCount = tmfqs::checkedStateCount(qubits);
	tmfqs::QuantumRegister quantumRegister(qubits, cfg);
	quantumRegister.beginOperationBatch();
	for(unsigned int q = 0; q < qubits; ++q) {
		quantumRegister.applyHadamard(q);
	}

	tmfqs::Amplitude amplitudeSum{std::sqrt(static_cast<double>(stateCount)), 0.0};
	const unsigned int iterations = computeGroverIterations(stateCount, markedStates.size());
	for(unsigned int iteration = 0; iteration < iterations; ++iteration) {
		tmfqs::Amplitude markedAmplitudeSum{0.0, 0.0};
		for(tmfqs::StateIndex markedState : markedStates) {
			const tmfqs::Amplitude amp = quantumRegister.amplitude(markedState);
			markedAmplitudeSum.real += amp.real;
			markedAmplitudeSum.imag += amp.imag;
			quantumRegister.applyPhaseFlipBasisState(markedState);
		}
		amplitudeSum.real -= 2.0 * markedAmplitudeSum.real;
		amplitudeSum.imag -= 2.0 * markedAmplitudeSum.imag;
		quantumRegister.applyInversionAboutMean({
			amplitudeSum.real / static_cast<double>(stateCount),
			amplitudeSum.imag / static_cast<double>(stateCount)
		});
	}
	quantumRegister.endOperationBatch();
	return quantumRegister;
}

tmfqs::QuantumRegister runQftPatternState(unsigned int qubits, const tmfqs::RegisterConfig &cfg) {
	const unsigned int totalStates = tmfqs::checkedStateCount(qubits);
	tmfqs::QuantumRegister quantumRegister(qubits, cfg);
	quantumRegister.initUniformSuperposition(tmfqs::BasisStateList(choosePatternStates(totalStates)));
	tmfqs::algorithms::qftInPlace(quantumRegister);
	return quantumRegister;
}

tmfqs::QuantumRegister runQftRandomPhaseState(unsigned int qubits, const tmfqs::RegisterConfig &cfg) {
	tmfqs::QuantumRegister quantumRegister(
		qubits,
		buildRandomPhaseInput(tmfqs::checkedStateCount(qubits)),
		cfg);
	tmfqs::algorithms::qftInPlace(quantumRegister);
	return quantumRegister;
}

ComparisonMetrics compareAmplitudeVectors(
	const tmfqs::AmplitudesVector &dense,
	const tmfqs::AmplitudesVector &zfp) {
	if(dense.size() != zfp.size()) {
		throw std::invalid_argument("Dense and ZFP vectors must have the same size");
	}
	if(dense.size() % 2u != 0u) {
		throw std::invalid_argument("Amplitude vectors must have even length");
	}

	ComparisonMetrics metrics;
	double diffNormSq = 0.0;
	double refNormSq = 0.0;
	double denseTotalProbability = 0.0;
	double zfpTotalProbability = 0.0;
	const size_t stateCount = dense.size() / 2u;

	for(size_t state = 0; state < stateCount; ++state) {
		const size_t elem = state * 2u;
		const double denseReal = dense[elem];
		const double denseImag = dense[elem + 1u];
		const double zfpReal = zfp[elem];
		const double zfpImag = zfp[elem + 1u];
		const double diffReal = zfpReal - denseReal;
		const double diffImag = zfpImag - denseImag;
		const double stateDiffSq = diffReal * diffReal + diffImag * diffImag;
		const double stateAbsError = std::sqrt(stateDiffSq);
		if(stateAbsError > metrics.maxAbsAmplitudeError) {
			metrics.maxAbsAmplitudeError = stateAbsError;
			metrics.worstState = static_cast<tmfqs::StateIndex>(state);
		}
		metrics.maxAbsComponentError = std::max(
			metrics.maxAbsComponentError,
			std::max(std::abs(diffReal), std::abs(diffImag)));
		diffNormSq += stateDiffSq;
		refNormSq += denseReal * denseReal + denseImag * denseImag;

		const double denseProbability = denseReal * denseReal + denseImag * denseImag;
		const double zfpProbability = zfpReal * zfpReal + zfpImag * zfpImag;
		denseTotalProbability += denseProbability;
		zfpTotalProbability += zfpProbability;
		metrics.maxAbsProbabilityError = std::max(
			metrics.maxAbsProbabilityError,
			std::abs(zfpProbability - denseProbability));
	}

	metrics.rmseAmplitude = std::sqrt(diffNormSq / static_cast<double>(stateCount));
	metrics.relL2 = refNormSq == 0.0 ? 0.0 : std::sqrt(diffNormSq / refNormSq);
	metrics.totalProbabilityDiff = std::abs(zfpTotalProbability - denseTotalProbability);
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
			const unsigned int stateCount = tmfqs::checkedStateCount(qubitCount);
			const tmfqs::StateIndex s1 = stateCount / 8u;
			const tmfqs::StateIndex s2 = stateCount / 2u;
			const tmfqs::StateIndex s3 = (7u * stateCount) / 8u;

			const tmfqs::RegisterConfig denseGroverCfg = makeDenseConfig(tmfqs::StorageWorkloadHint::Grover);
			const tmfqs::RegisterConfig zfpGroverCfg = makeThesisZfpConfig(tmfqs::StorageWorkloadHint::Grover);
			std::vector<ComparisonMetrics> groverSingleRuns;
			for(tmfqs::StateIndex markedState : std::vector<tmfqs::StateIndex>{s1, s2, s3}) {
				const tmfqs::QuantumRegister denseReg = runGroverState(qubitCount, {markedState}, denseGroverCfg);
				const tmfqs::QuantumRegister zfpReg = runGroverState(qubitCount, {markedState}, zfpGroverCfg);
				groverSingleRuns.push_back(compareAmplitudeVectors(denseReg.amplitudes(), zfpReg.amplitudes()));
			}
			SummaryRow groverSingle = averageRows(
				"Grover",
				"Objetivo unico (promedio N/8, N/2, 7N/8)",
				qubitCount,
				groverSingleRuns);
			groverSingleRows.push_back(groverSingle);
			allRows.push_back(groverSingle);

			const tmfqs::QuantumRegister denseGroverMultiReg = runGroverState(qubitCount, {s1, s2, s3}, denseGroverCfg);
			const tmfqs::QuantumRegister zfpGroverMultiReg = runGroverState(qubitCount, {s1, s2, s3}, zfpGroverCfg);
			SummaryRow groverMulti = averageRows(
				"Grover",
				"Multiobjetivo",
				qubitCount,
				{compareAmplitudeVectors(denseGroverMultiReg.amplitudes(), zfpGroverMultiReg.amplitudes())});
			groverMultiRows.push_back(groverMulti);
			allRows.push_back(groverMulti);

			const tmfqs::RegisterConfig denseQftCfg = makeDenseConfig(tmfqs::StorageWorkloadHint::Qft);
			const tmfqs::RegisterConfig zfpQftCfg = makeThesisZfpConfig(tmfqs::StorageWorkloadHint::Qft);
			const tmfqs::QuantumRegister denseQftPatternReg = runQftPatternState(qubitCount, denseQftCfg);
			const tmfqs::QuantumRegister zfpQftPatternReg = runQftPatternState(qubitCount, zfpQftCfg);
			SummaryRow qftPattern = averageRows(
				"QFT",
				"Superposicion periodica",
				qubitCount,
				{compareAmplitudeVectors(denseQftPatternReg.amplitudes(), zfpQftPatternReg.amplitudes())});
			qftPatternRows.push_back(qftPattern);
			allRows.push_back(qftPattern);

			const tmfqs::QuantumRegister denseQftRandomReg = runQftRandomPhaseState(qubitCount, denseQftCfg);
			const tmfqs::QuantumRegister zfpQftRandomReg = runQftRandomPhaseState(qubitCount, zfpQftCfg);
			SummaryRow qftRandom = averageRows(
				"QFT",
				"Alta entropia",
				qubitCount,
				{compareAmplitudeVectors(denseQftRandomReg.amplitudes(), zfpQftRandomReg.amplitudes())});
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
