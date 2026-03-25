#include "tmfqsfs.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "../common/test_helpers.h"

namespace {

bool containsState(const std::vector<tmfqs::StateIndex> &states, tmfqs::StateIndex value) {
	return std::find(states.begin(), states.end(), value) != states.end();
}

void testComplexExp() {
	using namespace tmfqs;
	std::cout << "=== complexExp precision ===\n";
	const Amplitude result1 = complexExp({0.0, 0.0});
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(result1.real, 1.0) && tmfqs_test::approxEqual(result1.imag, 0.0));

	const Amplitude result2 = complexExp({0.0, kPi});
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(result2.real, -1.0) && tmfqs_test::approxEqual(result2.imag, 0.0));

	const Amplitude result3 = complexExp({1.0, 0.0});
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(result3.real, std::exp(1.0)));
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
	TMFQS_TEST_REQUIRE(successes > 0);
}

void testGroverSmallSpaceIterations() {
	using namespace tmfqs;
	std::cout << "=== Grover small-space iteration count ===\n";
	const algorithms::GroverConfig config{2u, 2u, false};
	const int trials = 32;
	for(int t = 0; t < trials; ++t) {
		Mt19937RandomSource randomSource(static_cast<uint32_t>(200u + t));
		TMFQS_TEST_REQUIRE(algorithms::groverSearch(config, randomSource) == config.markedState);
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
		TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(amp.real, expectedAmp, 1e-6));
		TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(amp.imag, 0.0, 1e-6));
	}
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(qreg.totalProbability(), 1.0, 1e-6));
}

void testQftBasisPhases() {
	using namespace tmfqs;
	std::cout << "=== qftInPlace basis phases ===\n";
	const unsigned int numQubits = 3;
	const unsigned int sourceState = 5;
	const unsigned int stateCount = checkedStateCount(numQubits);
	QuantumRegister qreg(numQubits, sourceState);
	algorithms::qftInPlace(qreg);

	const double amplitudeScale = 1.0 / std::sqrt(static_cast<double>(stateCount));
	for(unsigned int y = 0; y < stateCount; ++y) {
		const double angle =
			(2.0 * kPi * static_cast<double>(sourceState) * static_cast<double>(y)) /
			static_cast<double>(stateCount);
		const Amplitude amp = qreg.amplitude(y);
		TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(amp.real, amplitudeScale * std::cos(angle), 1e-6));
		TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(amp.imag, amplitudeScale * std::sin(angle), 1e-6));
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
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(after.real, -before.real, 1e-9));
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(after.imag, -before.imag, 1e-9));

	qreg.applyInversionAboutMean();
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(qreg.totalProbability(), 1.0, 1e-6));
}

void testMeanAwareInversionMatchesDefault() {
	using namespace tmfqs;
	std::cout << "=== mean-aware inversion about mean ===\n";
	const AmplitudesVector amplitudes = {
		0.20, 0.10,
		-0.15, 0.05,
		0.30, -0.20,
		0.00, 0.10,
		-0.25, -0.15,
		0.18, 0.12,
		-0.08, 0.04,
		0.22, -0.06
	};

	auto verifyConfig = [&](const RegisterConfig &cfg, double tol) {
		QuantumRegister baseline(3, amplitudes, cfg);
		QuantumRegister optimized(3, amplitudes, cfg);
		Amplitude mean{0.0, 0.0};
		for(unsigned int state = 0; state < optimized.stateCount(); ++state) {
			const Amplitude amp = optimized.amplitude(state);
			mean.real += amp.real;
			mean.imag += amp.imag;
		}
		mean.real /= static_cast<double>(optimized.stateCount());
		mean.imag /= static_cast<double>(optimized.stateCount());
		baseline.applyInversionAboutMean();
		optimized.applyInversionAboutMean(mean);
		tmfqs_test::assertRegistersClose(baseline, optimized, tol);
	};

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	verifyConfig(denseCfg, 1e-12);

	if(StorageStrategyRegistry::isAvailable(StorageStrategyKind::Blosc)) {
		RegisterConfig bloscCfg;
		bloscCfg.strategy = StorageStrategyKind::Blosc;
		bloscCfg.blosc.chunkStates = 8;
		bloscCfg.blosc.gateCacheSlots = 4;
		verifyConfig(bloscCfg, 1e-12);
	}

	if(StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		RegisterConfig zfpCfg;
		zfpCfg.strategy = StorageStrategyKind::Zfp;
		zfpCfg.zfp.mode = ZfpCompressionMode::FixedRate;
		zfpCfg.zfp.rate = 64.0;
		zfpCfg.zfp.chunkStates = 8;
		zfpCfg.zfp.gateCacheSlots = 4;
		verifyConfig(zfpCfg, 1e-9);
	}
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
	TMFQS_TEST_REQUIRE(threw);
}

void testGroverMultiMarkedFullSpace() {
	using namespace tmfqs;
	std::cout << "=== generalized Grover multi-marked full space ===\n";
	const std::vector<StateIndex> markedStates = {1u, 6u};
	const algorithms::GroverConfig config{BasisStateList{1u, 6u}, 3u, false};
	for(int t = 0; t < 32; ++t) {
		Mt19937RandomSource randomSource(static_cast<uint32_t>(300u + t));
		TMFQS_TEST_REQUIRE(containsState(markedStates, algorithms::groverSearch(config, randomSource)));
	}
}

void testGroverSubsetSupport() {
	using namespace tmfqs;
	std::cout << "=== generalized Grover subset support ===\n";
	const std::vector<StateIndex> markedStates = {2u, 5u};
	const algorithms::GroverConfig config{BasisStateList{2u, 5u, 2u}, 3u, false};
	for(int t = 0; t < 16; ++t) {
		Mt19937RandomSource randomSource(static_cast<uint32_t>(400u + t));
		TMFQS_TEST_REQUIRE(containsState(markedStates, algorithms::groverSearch(config, randomSource)));
	}
}

void testGroverZeroIterationCase() {
	using namespace tmfqs;
	std::cout << "=== generalized Grover zero-iteration case ===\n";
	const algorithms::GroverConfig config{BasisStateList{0u, 1u, 2u, 3u}, 2u, false};
	Mt19937RandomSource randomSource(500u);
	const StateIndex measured = algorithms::groverSearch(config, randomSource);
	TMFQS_TEST_REQUIRE(measured < checkedStateCount(config.numQubits));
}

void testGroverBackendSelection() {
	using namespace tmfqs;
	std::cout << "=== generalized Grover arbitrary reference state ===\n";
	const std::vector<StateIndex> markedStates = {1u, 6u};

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	{
		const algorithms::GroverConfig denseConfig{BasisStateList{1u, 6u}, 3u, false, denseCfg};
		Mt19937RandomSource randomSource(600u);
		TMFQS_TEST_REQUIRE(containsState(markedStates, algorithms::groverSearch(denseConfig, randomSource)));
	}

	if(StorageStrategyRegistry::isAvailable(StorageStrategyKind::Blosc)) {
		RegisterConfig bloscCfg;
		bloscCfg.strategy = StorageStrategyKind::Blosc;
		bloscCfg.blosc.chunkStates = 8;
		bloscCfg.blosc.gateCacheSlots = 4;
		const algorithms::GroverConfig bloscConfig{BasisStateList{1u, 6u}, 3u, false, bloscCfg};
		Mt19937RandomSource randomSource(601u);
		TMFQS_TEST_REQUIRE(containsState(markedStates, algorithms::groverSearch(bloscConfig, randomSource)));
	}

	if(StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		RegisterConfig zfpCfg;
		zfpCfg.strategy = StorageStrategyKind::Zfp;
		zfpCfg.zfp.mode = ZfpCompressionMode::FixedRate;
		zfpCfg.zfp.rate = 32.0;
		zfpCfg.zfp.chunkStates = 8;
		zfpCfg.zfp.gateCacheSlots = 4;
		const algorithms::GroverConfig zfpConfig{BasisStateList{1u, 6u}, 3u, false, zfpCfg};
		Mt19937RandomSource randomSource(602u);
		TMFQS_TEST_REQUIRE(containsState(markedStates, algorithms::groverSearch(zfpConfig, randomSource)));
	}
}

void testGroverInvalidMultiMarkedInput() {
	using namespace tmfqs;
	std::cout << "=== generalized Grover invalid input ===\n";
	Mt19937RandomSource randomSource(700u);
	bool threw = false;
	try {
		(void)algorithms::groverSearch({BasisStateList{1u, 8u}, 3u, false}, randomSource);
	} catch(const std::invalid_argument &) {
		threw = true;
	}
	TMFQS_TEST_REQUIRE(threw);
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
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(a00.real, 1.0 / std::sqrt(2.0), 1e-6));
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(a10.real, 1.0 / std::sqrt(2.0), 1e-6));
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
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(a00.real, 1.0 / std::sqrt(2.0), 1e-6));
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(a10.real, 1.0 / std::sqrt(2.0), 1e-6));
}

void testDeterministicGroverSeed() {
	using namespace tmfqs;
	std::cout << "=== deterministic grover seed ===\n";
	const algorithms::GroverConfig config{5u, 3u, false};
	Mt19937RandomSource rng1(12345u);
	Mt19937RandomSource rng2(12345u);
	TMFQS_TEST_REQUIRE(algorithms::groverSearch(config, rng1) == algorithms::groverSearch(config, rng2));
}

void testScalarMultiplication() {
	using namespace tmfqs;
	std::cout << "=== QuantumGate scalar multiplication ===\n";
	QuantumGate hadamard = QuantumGate::Hadamard();
	QuantumGate scaled = hadamard * Amplitude{2.0, 0.0};
	const double expected = 2.0 / std::sqrt(2.0);
	TMFQS_TEST_REQUIRE(!tmfqs_test::approxEqual(scaled[0][0].real, 0.0));
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(scaled[0][0].real, expected));
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(scaled[1][1].real, -expected));
}

} // namespace

int main() {
	testComplexExp();
	testGroverMeasure();
	testGroverSmallSpaceIterations();
	testScalarMultiplication();
	testQft();
	testQftBasisPhases();
	testGroverPrimitives();
	testMeanAwareInversionMatchesDefault();
	testGroverInvalidInputThrows();
	testGroverMultiMarkedFullSpace();
	testGroverSubsetSupport();
	testGroverZeroIterationCase();
	testGroverBackendSelection();
	testGroverInvalidMultiMarkedInput();
	testOperationExecutor();
	testCompiledPlanRepeat();
	testDeterministicGroverSeed();
	std::cout << "Algorithm integration tests passed.\n";
	return 0;
}
