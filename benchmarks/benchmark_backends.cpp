#include "tmfqsfs.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
	std::string name;
	double seconds = 0.0;
	unsigned int iterations = 0;
};

void printResult(const BenchmarkResult &result) {
	const double iterPerSecond = result.seconds > 0.0
		? static_cast<double>(result.iterations) / result.seconds
		: 0.0;
	std::cout << std::left << std::setw(42) << result.name
	          << "  " << std::fixed << std::setprecision(4) << result.seconds << " s"
	          << "  (" << std::setprecision(1) << iterPerSecond << " it/s)\n";
}

void writeCsv(const std::string &path, const std::vector<BenchmarkResult> &results) {
	std::ofstream out(path);
	if(!out.is_open()) {
		throw std::runtime_error("Failed to open benchmark CSV output: " + path);
	}
	out << "name,seconds,iterations\n";
	for(const BenchmarkResult &result : results) {
		out << result.name << "," << result.seconds << "," << result.iterations << "\n";
	}
}

BenchmarkResult benchmarkDenseKernelPath(unsigned int iterations) {
	tmfqs::RegisterConfig cfg;
	cfg.strategy = tmfqs::StorageStrategyKind::Dense;
	tmfqs::QuantumRegister reg(12, 0, cfg);
	for(unsigned int q = 0; q < reg.qubitCount(); ++q) {
		reg.applyHadamard(q);
	}

	const auto start = Clock::now();
	for(unsigned int i = 0; i < iterations; ++i) {
		const tmfqs::QubitIndex q0 = i % reg.qubitCount();
		const tmfqs::QubitIndex q1 = (q0 + 3u) % reg.qubitCount();
		reg.applyHadamard(q0);
		reg.applyControlledNot(q0, q1);
		reg.applyControlledPhaseShift(q1, q0, tmfqs::kPi / 32.0);
	}
	const auto end = Clock::now();
	(void)reg.totalProbability();

	return {
		"Dense single/two-qubit kernels",
		std::chrono::duration<double>(end - start).count(),
		iterations
	};
}

BenchmarkResult benchmarkBlockApplyPath(unsigned int iterations) {
	tmfqs::RegisterConfig cfg;
	cfg.strategy = tmfqs::StorageStrategyKind::Dense;
	tmfqs::QuantumRegister reg(11, 0, cfg);
	tmfqs::QuantumGate toffoli = tmfqs::QuantumGate::Toffoli();
	for(unsigned int q = 0; q < reg.qubitCount(); ++q) {
		reg.applyHadamard(q);
	}

	const auto start = Clock::now();
	for(unsigned int i = 0; i < iterations; ++i) {
		const tmfqs::QubitIndex a = (i + 0u) % reg.qubitCount();
		const tmfqs::QubitIndex b = (i + 4u) % reg.qubitCount();
		const tmfqs::QubitIndex c = (i + 8u) % reg.qubitCount();
		reg.applyGate(toffoli, tmfqs::QubitList{a, b, c});
	}
	const auto end = Clock::now();
	(void)reg.totalProbability();

	return {
		"Generic block-apply path (3-qubit gate)",
		std::chrono::duration<double>(end - start).count(),
		iterations
	};
}

BenchmarkResult benchmarkBloscWorkflow(unsigned int iterations) {
	tmfqs::RegisterConfig cfg;
	cfg.strategy = tmfqs::StorageStrategyKind::Blosc;
	cfg.blosc.chunkStates = 8192;
	cfg.blosc.gateCacheSlots = 16;
	tmfqs::QuantumRegister reg(12, 0, cfg);
	tmfqs::QuantumGate toffoli = tmfqs::QuantumGate::Toffoli();
	tmfqs::Mt19937RandomSource rng(123456u);
	for(unsigned int q = 0; q < reg.qubitCount(); ++q) {
		reg.applyHadamard(q);
	}

	const auto start = Clock::now();
	for(unsigned int i = 0; i < iterations; ++i) {
		const tmfqs::QubitIndex q0 = (i + 1u) % reg.qubitCount();
		const tmfqs::QubitIndex q1 = (i + 6u) % reg.qubitCount();
		reg.applyControlledNot(q0, q1);
		reg.applyGate(toffoli, tmfqs::QubitList{0, 1, 2});
		(void)reg.measure(rng);
	}
	const auto end = Clock::now();

	return {
		"Blosc gate+block+measure workflow",
		std::chrono::duration<double>(end - start).count(),
		iterations
	};
}

BenchmarkResult benchmarkZfpWorkflow(unsigned int iterations) {
	tmfqs::RegisterConfig cfg;
	cfg.strategy = tmfqs::StorageStrategyKind::Zfp;
	cfg.zfp.mode = tmfqs::ZfpCompressionMode::FixedRate;
	cfg.zfp.rate = 24.0;
	tmfqs::QuantumRegister reg(12, 0, cfg);
	tmfqs::QuantumGate toffoli = tmfqs::QuantumGate::Toffoli();
	tmfqs::Mt19937RandomSource rng(123456u);
	for(unsigned int q = 0; q < reg.qubitCount(); ++q) {
		reg.applyHadamard(q);
	}

	const auto start = Clock::now();
	for(unsigned int i = 0; i < iterations; ++i) {
		const tmfqs::QubitIndex q0 = (i + 1u) % reg.qubitCount();
		const tmfqs::QubitIndex q1 = (i + 6u) % reg.qubitCount();
		reg.applyControlledNot(q0, q1);
		reg.applyGate(toffoli, tmfqs::QubitList{0, 1, 2});
		(void)reg.measure(rng);
	}
	const auto end = Clock::now();

	return {
		"Zfp gate+block+measure workflow",
		std::chrono::duration<double>(end - start).count(),
		iterations
	};
}

} // namespace

int main(int argc, char *argv[]) {
	std::string csvOutputPath;
	for(int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if(arg == "--csv") {
			if(i + 1 >= argc) {
				std::cerr << "Missing value for --csv\n";
				return 2;
			}
			csvOutputPath = argv[++i];
			continue;
		}
		std::cerr << "Unknown option: " << arg << "\n";
		return 2;
	}

	std::vector<BenchmarkResult> results;
	std::cout << "=== TMFQS backend benchmarks (O3, NDEBUG) ===\n";
	results.push_back(benchmarkDenseKernelPath(2000));
	results.push_back(benchmarkBlockApplyPath(1000));
	printResult(results[0]);
	printResult(results[1]);

	if(tmfqs::StorageStrategyRegistry::isAvailable(tmfqs::StorageStrategyKind::Blosc)) {
		results.push_back(benchmarkBloscWorkflow(160));
		printResult(results.back());
	} else {
		std::cout << "Blosc gate+block+measure workflow           skipped (Blosc unavailable)\n";
	}

	if(tmfqs::StorageStrategyRegistry::isAvailable(tmfqs::StorageStrategyKind::Zfp)) {
		results.push_back(benchmarkZfpWorkflow(160));
		printResult(results.back());
	} else {
		std::cout << "Zfp gate+block+measure workflow             skipped (zfp unavailable)\n";
	}

	if(!csvOutputPath.empty()) {
		writeCsv(csvOutputPath, results);
		std::cout << "Wrote benchmark CSV: " << csvOutputPath << "\n";
	}
	return 0;
}
