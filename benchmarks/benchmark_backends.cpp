#include "tmfqsfs.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

double millisSince(const Clock::time_point &start) {
	return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

double benchApplyGate(unsigned int numQubits, unsigned int iterations, const RegisterConfig &cfg) {
	QuantumRegister qreg(numQubits, cfg);
	const QuantumGate h = QuantumGate::Hadamard();
	const QuantumGate cnot = QuantumGate::ControlledNot();
	const QuantumGate toffoli = QuantumGate::Toffoli();

	const auto start = Clock::now();
	for(unsigned int i = 0; i < iterations; ++i) {
		const unsigned int q0 = i % numQubits;
		qreg.applyGate(h, QubitList{q0});
		if(numQubits >= 2u) {
			const unsigned int q1 = (q0 + 1u) % numQubits;
			qreg.applyGate(cnot, QubitList{q0, q1});
		}
		if(numQubits >= 3u) {
			const unsigned int q1 = (q0 + 1u) % numQubits;
			const unsigned int q2 = (q0 + 2u) % numQubits;
			qreg.applyGate(toffoli, QubitList{q0, q1, q2});
		}
	}
	return millisSince(start);
}

double benchMeasure(unsigned int numQubits, unsigned int samples, const RegisterConfig &cfg) {
	QuantumRegister qreg(numQubits, cfg);
	for(unsigned int q = 0; q < numQubits; ++q) {
		qreg.Hadamard(q);
	}
	setRandomSeed(20260306u);
	volatile unsigned int sink = 0;
	const auto start = Clock::now();
	for(unsigned int i = 0; i < samples; ++i) {
		sink ^= qreg.measure();
	}
	(void)sink;
	return millisSince(start);
}

double benchPrint(unsigned int numQubits, const RegisterConfig &cfg) {
	QuantumRegister qreg(numQubits, cfg);
	for(unsigned int q = 0; q < numQubits; ++q) {
		qreg.Hadamard(q);
	}
	std::ostringstream os;
	const auto start = Clock::now();
	os << qreg;
	return millisSince(start);
}
} // namespace

int main() {
	const unsigned int applyQubits = 18;
	const unsigned int applyIters = 64;
	const unsigned int measureQubits = 18;
	const unsigned int measureSamples = 5000;
	const unsigned int printQubits = 14;

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;

	std::cout << "Benchmark configuration:\n";
	std::cout << "  applyGate: n=" << applyQubits << ", iterations=" << applyIters << "\n";
	std::cout << "  measure:   n=" << measureQubits << ", samples=" << measureSamples << "\n";
	std::cout << "  print:     n=" << printQubits << "\n";

	const double denseApplyMs = benchApplyGate(applyQubits, applyIters, denseCfg);
	const double denseMeasureMs = benchMeasure(measureQubits, measureSamples, denseCfg);
	const double densePrintMs = benchPrint(printQubits, denseCfg);
	std::cout << "[dense] apply_ms=" << denseApplyMs
	          << " measure_ms=" << denseMeasureMs
	          << " print_ms=" << densePrintMs << "\n";

	if(isStrategyAvailable(StorageStrategyKind::Blosc)) {
		RegisterConfig bloscCfg;
		bloscCfg.strategy = StorageStrategyKind::Blosc;
		bloscCfg.blosc.chunkStates = 16384;
		bloscCfg.blosc.clevel = 1;
		bloscCfg.blosc.nthreads = 1;
		bloscCfg.blosc.gateCacheSlots = 8;
		const double bloscApplyMs = benchApplyGate(applyQubits, applyIters, bloscCfg);
		const double bloscMeasureMs = benchMeasure(measureQubits, measureSamples, bloscCfg);
		const double bloscPrintMs = benchPrint(printQubits, bloscCfg);
		std::cout << "[blosc] apply_ms=" << bloscApplyMs
		          << " measure_ms=" << bloscMeasureMs
		          << " print_ms=" << bloscPrintMs << "\n";
	} else {
		std::cout << "[blosc] unavailable in this build\n";
	}

	return 0;
}
