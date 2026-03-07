#include "tmfqsfs.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

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

} // namespace

int main() {
	std::cout << "=== TMFQS backend benchmarks (O3, NDEBUG) ===\n";
	printResult(benchmarkDenseKernelPath(2000));
	printResult(benchmarkBlockApplyPath(1000));

	if(tmfqs::isStrategyAvailable(tmfqs::StorageStrategyKind::Blosc)) {
		printResult(benchmarkBloscWorkflow(160));
	} else {
		std::cout << "Blosc gate+block+measure workflow           skipped (Blosc unavailable)\n";
	}
	return 0;
}
