#include "tmfqsfs.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "../common/test_helpers.h"

namespace {

void testComplexExp() {
	using namespace tmfqs;
	std::cout << "=== complexExp precision ===\n";
	const Amplitude result1 = complexExp({0.0, 0.0});
	assert(tmfqs_test::approxEqual(result1.real, 1.0) && tmfqs_test::approxEqual(result1.imag, 0.0));

	const Amplitude result2 = complexExp({0.0, kPi});
	assert(tmfqs_test::approxEqual(result2.real, -1.0) && tmfqs_test::approxEqual(result2.imag, 0.0));

	const Amplitude result3 = complexExp({1.0, 0.0});
	assert(tmfqs_test::approxEqual(result3.real, std::exp(1.0)));
}

void testGroverMeasure() {
	using namespace tmfqs;
	std::cout << "=== Grover measurement ===\n";
	const algorithms::GroverConfig config{5u, 3u, false};
	int successes = 0;
	for(int t = 0; t < 20; ++t) {
		Mt19937RandomSource randomSource(static_cast<uint32_t>(100u + t));
		if(algorithms::groverSearch(config, randomSource) == config.markedState) {
			++successes;
		}
	}
	assert(successes > 0);
}

void testGroverSmallSpaceIterations() {
	using namespace tmfqs;
	std::cout << "=== Grover small-space iteration count ===\n";
	const algorithms::GroverConfig config{2u, 2u, false};
	const int trials = 32;
	for(int t = 0; t < trials; ++t) {
		Mt19937RandomSource randomSource(static_cast<uint32_t>(200u + t));
		assert(algorithms::groverSearch(config, randomSource) == config.markedState);
	}
}

void testQft() {
	using namespace tmfqs;
	std::cout << "=== qftInPlace ===\n";
	QuantumRegister qreg(3, 0);
	algorithms::qftInPlace(qreg);
	const double expectedAmp = 1.0 / std::sqrt(8.0);
	for(unsigned int s = 0; s < 8; ++s) {
		const Amplitude amp = qreg.amplitude(s);
		assert(tmfqs_test::approxEqual(amp.real, expectedAmp, 1e-6));
	}
	assert(tmfqs_test::approxEqual(qreg.totalProbability(), 1.0, 1e-6));
}

void testQftIfftRoundTrip() {
	using namespace tmfqs;
	std::cout << "=== QFT/IQFT round-trip ===\n";
	QuantumRegister qreg(3, 5);
	qreg.applyGate(QuantumGate::QFT(3), QubitList{0, 1, 2});
	qreg.applyGate(QuantumGate::IQFT(3), QubitList{0, 1, 2});
	for(unsigned int s = 0; s < 8; ++s) {
		const Amplitude amp = qreg.amplitude(s);
		if(s == 5u) {
			assert(tmfqs_test::approxEqual(amp.real, 1.0, 1e-6));
			assert(tmfqs_test::approxEqual(amp.imag, 0.0, 1e-6));
		} else {
			assert(tmfqs_test::approxEqual(amp.real, 0.0, 1e-6));
			assert(tmfqs_test::approxEqual(amp.imag, 0.0, 1e-6));
		}
	}
}

void testGroverPrimitives() {
	using namespace tmfqs;
	std::cout << "=== register primitive ops ===\n";
	QuantumRegister qreg(2, 0);
	qreg.applyHadamard(0);
	qreg.applyHadamard(1);

	const Amplitude before = qreg.amplitude(3);
	qreg.applyPhaseFlipBasisState(3);
	const Amplitude after = qreg.amplitude(3);
	assert(tmfqs_test::approxEqual(after.real, -before.real, 1e-9));
	assert(tmfqs_test::approxEqual(after.imag, -before.imag, 1e-9));

	qreg.applyInversionAboutMean();
	assert(tmfqs_test::approxEqual(qreg.totalProbability(), 1.0, 1e-6));
}

void testGroverInvalidInputThrows() {
	using namespace tmfqs;
	std::cout << "=== Grover invalid input ===\n";
	Mt19937RandomSource randomSource(42u);
	bool threw = false;
	try {
		(void)algorithms::groverSearch({8u, 3u, false}, randomSource);
	} catch(const std::invalid_argument &) {
		threw = true;
	}
	assert(threw);
}

void testOperationExecutor() {
	using namespace tmfqs;
	using namespace tmfqs::algorithms;
	std::cout << "=== operation executor ===\n";
	QuantumRegister qreg(2, 0);
	std::vector<AlgorithmOperation> ops = {
		HadamardOp{0},
		SwapOp{0, 1},
		SwapOp{0, 1}
	};
	executeOperations(qreg, ops);
	const Amplitude a00 = qreg.amplitude(0);
	const Amplitude a10 = qreg.amplitude(2);
	assert(tmfqs_test::approxEqual(a00.real, 1.0 / std::sqrt(2.0), 1e-6));
	assert(tmfqs_test::approxEqual(a10.real, 1.0 / std::sqrt(2.0), 1e-6));
}

void testCompiledPlanRepeat() {
	using namespace tmfqs;
	using namespace tmfqs::algorithms;
	std::cout << "=== compiled plan repeat block ===\n";
	QuantumRegister qreg(2, 0);
	CompiledAlgorithmPlan plan;
	plan.addOperation(HadamardOp{0});
	plan.addRepeatBlock({SwapOp{0, 1}}, 2);
	executePlan(qreg, plan);
	const Amplitude a00 = qreg.amplitude(0);
	const Amplitude a10 = qreg.amplitude(2);
	assert(tmfqs_test::approxEqual(a00.real, 1.0 / std::sqrt(2.0), 1e-6));
	assert(tmfqs_test::approxEqual(a10.real, 1.0 / std::sqrt(2.0), 1e-6));
}

void testDeterministicGroverSeed() {
	using namespace tmfqs;
	std::cout << "=== deterministic grover seed ===\n";
	const algorithms::GroverConfig config{5u, 3u, false};
	Mt19937RandomSource rng1(12345u);
	Mt19937RandomSource rng2(12345u);
	assert(algorithms::groverSearch(config, rng1) == algorithms::groverSearch(config, rng2));
}

void testScalarMultiplication() {
	using namespace tmfqs;
	std::cout << "=== QuantumGate scalar multiplication ===\n";
	QuantumGate hadamard = QuantumGate::Hadamard();
	QuantumGate scaled = hadamard * Amplitude{2.0, 0.0};
	const double expected = 2.0 / std::sqrt(2.0);
	assert(!tmfqs_test::approxEqual(scaled[0][0].real, 0.0));
	assert(tmfqs_test::approxEqual(scaled[0][0].real, expected));
	assert(tmfqs_test::approxEqual(scaled[1][1].real, -expected));
}

} // namespace

int main() {
	testComplexExp();
	testGroverMeasure();
	testGroverSmallSpaceIterations();
	testScalarMultiplication();
	testQft();
	testQftIfftRoundTrip();
	testGroverPrimitives();
	testGroverInvalidInputThrows();
	testOperationExecutor();
	testCompiledPlanRepeat();
	testDeterministicGroverSeed();
	std::cout << "Algorithm integration tests passed.\n";
	return 0;
}
