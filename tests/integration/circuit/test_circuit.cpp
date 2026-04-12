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

tmfqs::StateIndex runGroverCircuit(
	unsigned int qubits,
	tmfqs::BasisStateList markedStates,
	tmfqs::IRandomSource &randomSource,
	bool materializedDiffusion = false,
	const tmfqs::RegisterConfig &backendConfig = {}) {
	tmfqs::circuit::GroverCircuitOptions options;
	options.markedStates = std::move(markedStates);
	options.materializedDiffusion = materializedDiffusion;
	const tmfqs::circuit::Circuit circuit = tmfqs::circuit::makeGrover(qubits, std::move(options));
	tmfqs::sim::ExecutionConfig execution;
	execution.backend = backendConfig;
	const tmfqs::sim::RunResult result =
		tmfqs::sim::Simulator(execution).run(circuit, tmfqs::state::QuantumState::basis(qubits));
	return result.state.measure(randomSource);
}

tmfqs::state::QuantumState runQftCircuit(unsigned int qubits, tmfqs::StateIndex initialState) {
	const tmfqs::sim::RunResult result =
		tmfqs::sim::Simulator().run(tmfqs::circuit::makeQft(qubits), tmfqs::state::QuantumState::basis(qubits, initialState));
	return result.state;
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
	int successes = 0;
	for(int t = 0; t < 20; ++t) {
		Mt19937RandomSource randomSource(static_cast<uint32_t>(100u + t));
		if(runGroverCircuit(5u, BasisStateList{3u}, randomSource) == 3u) {
			++successes;
		}
	}
	TMFQS_TEST_REQUIRE(successes > 0);
}

void testGroverSmallSpaceIterations() {
	using namespace tmfqs;
	std::cout << "=== Grover small-space iteration count ===\n";
	const int trials = 32;
	for(int t = 0; t < trials; ++t) {
		Mt19937RandomSource randomSource(static_cast<uint32_t>(200u + t));
		TMFQS_TEST_REQUIRE(runGroverCircuit(2u, BasisStateList{2u}, randomSource) == 2u);
	}
}

void testQft() {
	using namespace tmfqs;
	std::cout << "=== QFT circuit ===\n";
	const state::QuantumState qreg = runQftCircuit(3u, 0u);
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
	std::cout << "=== QFT circuit basis phases ===\n";
	const unsigned int numQubits = 3;
	const unsigned int sourceState = 5;
	const StateIndex stateCount = checkedStateCount(numQubits);
	const state::QuantumState qreg = runQftCircuit(numQubits, sourceState);

	const double amplitudeScale = 1.0 / std::sqrt(static_cast<double>(stateCount));
	for(StateIndex y = 0; y < stateCount; ++y) {
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
		(void)runGroverCircuit(3u, BasisStateList{8u}, randomSource);
	} catch(const std::invalid_argument &) {
		threw = true;
	}
	TMFQS_TEST_REQUIRE(threw);
}

void testGroverMultiMarkedFullSpace() {
	using namespace tmfqs;
	std::cout << "=== generalized Grover multi-marked full space ===\n";
	const std::vector<StateIndex> markedStates = {1u, 6u};
	for(int t = 0; t < 32; ++t) {
		Mt19937RandomSource randomSource(static_cast<uint32_t>(300u + t));
		TMFQS_TEST_REQUIRE(containsState(markedStates, runGroverCircuit(3u, BasisStateList{1u, 6u}, randomSource)));
	}
}

void testGroverSubsetSupport() {
	using namespace tmfqs;
	std::cout << "=== generalized Grover subset support ===\n";
	const std::vector<StateIndex> markedStates = {2u, 5u};
	for(int t = 0; t < 16; ++t) {
		Mt19937RandomSource randomSource(static_cast<uint32_t>(400u + t));
		TMFQS_TEST_REQUIRE(containsState(markedStates, runGroverCircuit(3u, BasisStateList{2u, 5u, 2u}, randomSource)));
	}
}

void testGroverZeroIterationCase() {
	using namespace tmfqs;
	std::cout << "=== generalized Grover zero-iteration case ===\n";
	Mt19937RandomSource randomSource(500u);
	const StateIndex measured = runGroverCircuit(2u, BasisStateList{0u, 1u, 2u, 3u}, randomSource);
	TMFQS_TEST_REQUIRE(measured < checkedStateCount(2u));
}

void testGroverBackendSelection() {
	using namespace tmfqs;
	std::cout << "=== generalized Grover arbitrary reference state ===\n";
	const std::vector<StateIndex> markedStates = {1u, 6u};

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	{
		Mt19937RandomSource randomSource(600u);
		TMFQS_TEST_REQUIRE(containsState(markedStates, runGroverCircuit(3u, BasisStateList{1u, 6u}, randomSource, false, denseCfg)));
	}

	if(StorageStrategyRegistry::isAvailable(StorageStrategyKind::Blosc)) {
		RegisterConfig bloscCfg;
		bloscCfg.strategy = StorageStrategyKind::Blosc;
		bloscCfg.blosc.chunkStates = 8;
		bloscCfg.blosc.gateCacheSlots = 4;
		Mt19937RandomSource randomSource(601u);
		TMFQS_TEST_REQUIRE(containsState(markedStates, runGroverCircuit(3u, BasisStateList{1u, 6u}, randomSource, false, bloscCfg)));
	}

	if(StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		RegisterConfig zfpCfg;
		zfpCfg.strategy = StorageStrategyKind::Zfp;
		zfpCfg.zfp.mode = ZfpCompressionMode::FixedRate;
		zfpCfg.zfp.rate = 32.0;
		zfpCfg.zfp.chunkStates = 8;
		zfpCfg.zfp.gateCacheSlots = 4;
		Mt19937RandomSource randomSource(602u);
		TMFQS_TEST_REQUIRE(containsState(markedStates, runGroverCircuit(3u, BasisStateList{1u, 6u}, randomSource, false, zfpCfg)));
	}
}

void testGroverNormalBackendSelection() {
	using namespace tmfqs;
	std::cout << "=== normal Grover backend selection ===\n";
	const std::vector<StateIndex> markedStates = {1u, 6u};

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	{
		Mt19937RandomSource randomSource(610u);
		TMFQS_TEST_REQUIRE(containsState(markedStates, runGroverCircuit(3u, BasisStateList{1u, 6u}, randomSource, true, denseCfg)));
	}

	if(StorageStrategyRegistry::isAvailable(StorageStrategyKind::Blosc)) {
		RegisterConfig bloscCfg;
		bloscCfg.strategy = StorageStrategyKind::Blosc;
		bloscCfg.blosc.chunkStates = 8;
		bloscCfg.blosc.gateCacheSlots = 4;
		Mt19937RandomSource randomSource(611u);
		TMFQS_TEST_REQUIRE(containsState(markedStates, runGroverCircuit(3u, BasisStateList{1u, 6u}, randomSource, true, bloscCfg)));
	}

	if(StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		RegisterConfig zfpCfg;
		zfpCfg.strategy = StorageStrategyKind::Zfp;
		zfpCfg.zfp.mode = ZfpCompressionMode::FixedRate;
		zfpCfg.zfp.rate = 32.0;
		zfpCfg.zfp.chunkStates = 8;
		zfpCfg.zfp.gateCacheSlots = 4;
		Mt19937RandomSource randomSource(612u);
		TMFQS_TEST_REQUIRE(containsState(markedStates, runGroverCircuit(3u, BasisStateList{1u, 6u}, randomSource, true, zfpCfg)));
	}
}

void testGroverInvalidMultiMarkedInput() {
	using namespace tmfqs;
	std::cout << "=== generalized Grover invalid input ===\n";
	Mt19937RandomSource randomSource(700u);
	bool threw = false;
	try {
		(void)runGroverCircuit(3u, BasisStateList{1u, 8u}, randomSource);
	} catch(const std::invalid_argument &) {
		threw = true;
	}
	TMFQS_TEST_REQUIRE(threw);
}

void testOperationExecutor() {
	using namespace tmfqs;
	std::cout << "=== circuit operation sequence ===\n";
	circuit::Circuit circuit(2);
	circuit.h(0).swap(0, 1).swap(0, 1);
	const sim::RunResult result = sim::Simulator().run(circuit, state::QuantumState::basis(2));
	const Amplitude a00 = result.state.amplitude(0);
	const Amplitude a10 = result.state.amplitude(2);
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(a00.real, 1.0 / std::sqrt(2.0), 1e-6));
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(a10.real, 1.0 / std::sqrt(2.0), 1e-6));
}

void testCompiledPlanRepeat() {
	using namespace tmfqs;
	std::cout << "=== repeated circuit block ===\n";
	circuit::Circuit circuit(2);
	circuit.h(0);
	for(unsigned int repeat = 0; repeat < 2u; ++repeat) {
		circuit.swap(0, 1);
	}
	const sim::RunResult result = sim::Simulator().run(circuit, state::QuantumState::basis(2));
	const Amplitude a00 = result.state.amplitude(0);
	const Amplitude a10 = result.state.amplitude(2);
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(a00.real, 1.0 / std::sqrt(2.0), 1e-6));
	TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(a10.real, 1.0 / std::sqrt(2.0), 1e-6));
}

void testDeterministicGroverSeed() {
	using namespace tmfqs;
	std::cout << "=== deterministic grover seed ===\n";
	Mt19937RandomSource rng1(12345u);
	Mt19937RandomSource rng2(12345u);
	TMFQS_TEST_REQUIRE(
		runGroverCircuit(5u, BasisStateList{3u}, rng1) ==
		runGroverCircuit(5u, BasisStateList{3u}, rng2));
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
	testGroverNormalBackendSelection();
	testGroverInvalidMultiMarkedInput();
	testOperationExecutor();
	testCompiledPlanRepeat();
	testDeterministicGroverSeed();
	std::cout << "Circuit integration tests passed.\n";
	return 0;
}
