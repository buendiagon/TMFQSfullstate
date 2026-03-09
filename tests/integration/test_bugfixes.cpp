#include "tmfqsfs.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

bool approxEqual(double a, double b, double tol = 1e-10) {
	return std::fabs(a - b) < tol;
}

void testComplexExp() {
	using namespace tmfqs;
	std::cout << "=== complexExp precision ===\n";
	const Amplitude result1 = complexExp({0.0, 0.0});
	assert(approxEqual(result1.real, 1.0) && approxEqual(result1.imag, 0.0));

	const Amplitude result2 = complexExp({0.0, kPi});
	assert(approxEqual(result2.real, -1.0) && approxEqual(result2.imag, 0.0));

	const Amplitude result3 = complexExp({1.0, 0.0});
	assert(approxEqual(result3.real, std::exp(1.0)));
}

void testGroverMeasure() {
	using namespace tmfqs;
	std::cout << "=== Grover measurement ===\n";

	const algorithms::GroverConfig config{5u, 3u, false};
	int successes = 0;
	for(int t = 0; t < 20; ++t) {
		Mt19937RandomSource randomSource(static_cast<uint32_t>(100u + t));
		const unsigned int result = algorithms::groverSearch(config, randomSource);
		if(result == config.markedState) {
			++successes;
		}
	}
	assert(successes > 0);
}

void testGroverSmallSpaceIterations() {
	using namespace tmfqs;
	std::cout << "=== Grover small-space iteration count ===\n";

	const algorithms::GroverConfig config{2u, 2u, false};
	int successes = 0;
	const int trials = 32;
	for(int t = 0; t < trials; ++t) {
		Mt19937RandomSource randomSource(static_cast<uint32_t>(200u + t));
		const unsigned int result = algorithms::groverSearch(config, randomSource);
		if(result == config.markedState) {
			++successes;
		}
	}
	assert(successes == trials);
}

void testScalarMultiplication() {
	using namespace tmfqs;
	std::cout << "=== QuantumGate scalar multiplication ===\n";
	QuantumGate hadamard = QuantumGate::Hadamard();
	QuantumGate scaled = hadamard * Amplitude{2.0, 0.0};
	const double expected = 2.0 / std::sqrt(2.0);
	assert(!approxEqual(scaled[0][0].real, 0.0));
	assert(approxEqual(scaled[0][0].real, expected));
	assert(approxEqual(scaled[1][1].real, -expected));
}

void testQft() {
	using namespace tmfqs;
	std::cout << "=== qftInPlace ===\n";
	QuantumRegister qreg(3, 0);
	algorithms::qftInPlace(qreg);
	const double expectedAmp = 1.0 / std::sqrt(8.0);
	for(unsigned int s = 0; s < 8; ++s) {
		const Amplitude amp = qreg.amplitude(s);
		assert(approxEqual(amp.real, expectedAmp, 1e-6));
	}
	assert(approxEqual(qreg.totalProbability(), 1.0, 1e-6));
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
			assert(approxEqual(amp.real, 1.0, 1e-6));
			assert(approxEqual(amp.imag, 0.0, 1e-6));
		} else {
			assert(approxEqual(amp.real, 0.0, 1e-6));
			assert(approxEqual(amp.imag, 0.0, 1e-6));
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
	assert(approxEqual(after.real, -before.real, 1e-9));
	assert(approxEqual(after.imag, -before.imag, 1e-9));

	qreg.applyInversionAboutMean();
	assert(approxEqual(qreg.totalProbability(), 1.0, 1e-6));
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
	assert(approxEqual(a00.real, 1.0 / std::sqrt(2.0), 1e-6));
	assert(approxEqual(a10.real, 1.0 / std::sqrt(2.0), 1e-6));
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
	assert(approxEqual(a00.real, 1.0 / std::sqrt(2.0), 1e-6));
	assert(approxEqual(a10.real, 1.0 / std::sqrt(2.0), 1e-6));
}

void testDeterministicGroverSeed() {
	using namespace tmfqs;
	std::cout << "=== deterministic grover seed ===\n";
	const algorithms::GroverConfig config{5u, 3u, false};
	Mt19937RandomSource rng1(12345u);
	Mt19937RandomSource rng2(12345u);
	const unsigned int r1 = algorithms::groverSearch(config, rng1);
	const unsigned int r2 = algorithms::groverSearch(config, rng2);
	assert(r1 == r2);
}

void testDefaultConstructedRegisterIsValid() {
	using namespace tmfqs;
	std::cout << "=== default register validity ===\n";
	QuantumRegister qreg;
	assert(qreg.qubitCount() == 0u);
	assert(qreg.stateCount() == 1u);
	assert(qreg.amplitudeElementCount() == 2u);
	const Amplitude amp = qreg.amplitude(0);
	assert(approxEqual(amp.real, 1.0, 1e-12));
	assert(approxEqual(amp.imag, 0.0, 1e-12));
	assert(approxEqual(qreg.totalProbability(), 1.0, 1e-12));
	Mt19937RandomSource randomSource(1u);
	assert(qreg.measure(randomSource) == 0u);

	bool threw = false;
	try {
		qreg.applyHadamard(0);
	} catch(const std::out_of_range &) {
		threw = true;
	}
	assert(threw);
}

void testOutOfRangeQueriesThrow() {
	using namespace tmfqs;
	std::cout << "=== out-of-range queries ===\n";
	QuantumRegister qreg(2, 0);

	bool ampThrew = false;
	try {
		(void)qreg.amplitude(4);
	} catch(const std::out_of_range &) {
		ampThrew = true;
	}
	assert(ampThrew);

	bool probThrew = false;
	try {
		(void)qreg.probability(4);
	} catch(const std::out_of_range &) {
		probThrew = true;
	}
	assert(probThrew);
}

void testResolvedStrategyConsistency() {
	using namespace tmfqs;
	std::cout << "=== strategy resolution consistency ===\n";
	RegisterConfig cfg;
	cfg.strategy = StorageStrategyKind::Auto;
	cfg.autoThresholdBytes = 1u;
	QuantumRegister qreg(3, cfg);
	const StorageStrategyKind expected = resolveStorageStrategy(3, cfg);
	assert(qreg.storageStrategy() == expected);
}

void testDenseVsBloscParity() {
	using namespace tmfqs;
	std::cout << "=== dense vs blosc parity ===\n";
	if(!isStrategyAvailable(StorageStrategyKind::Blosc)) {
		std::cout << "Blosc backend unavailable in this build, skipping parity test.\n";
		return;
	}

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	RegisterConfig bloscCfg;
	bloscCfg.strategy = StorageStrategyKind::Blosc;
	bloscCfg.blosc.chunkStates = 8;
	bloscCfg.blosc.gateCacheSlots = 4;

	QuantumRegister denseReg(4, 0, denseCfg);
	QuantumRegister bloscReg(4, 0, bloscCfg);

	const std::vector<algorithms::AlgorithmOperation> ops = {
		algorithms::HadamardOp{0},
		algorithms::HadamardOp{2},
		algorithms::ControlledNotOp{0, 1},
		algorithms::ControlledPhaseShiftOp{2, 3, kPi / 8.0},
		algorithms::SwapOp{1, 3},
		algorithms::PhaseFlipBasisStateOp{6},
		algorithms::InversionAboutMeanOp{}
	};
	algorithms::executeOperations(denseReg, ops);
	algorithms::executeOperations(bloscReg, ops);

	for(unsigned int s = 0; s < denseReg.stateCount(); ++s) {
		const Amplitude a = denseReg.amplitude(s);
		const Amplitude b = bloscReg.amplitude(s);
		assert(approxEqual(a.real, b.real, 1e-9));
		assert(approxEqual(a.imag, b.imag, 1e-9));
	}
	assert(approxEqual(denseReg.totalProbability(), bloscReg.totalProbability(), 1e-9));
}

void testPublicGateValidation() {
	using namespace tmfqs;
	std::cout << "=== public gate validation ===\n";
	QuantumRegister qreg(2, 0);

	bool hadamardThrew = false;
	try { qreg.applyHadamard(2); } catch(const std::out_of_range &) { hadamardThrew = true; }
	assert(hadamardThrew);

	bool pauliXThrew = false;
	try { qreg.applyPauliX(2); } catch(const std::out_of_range &) { pauliXThrew = true; }
	assert(pauliXThrew);

	bool cnotRangeThrew = false;
	try { qreg.applyControlledNot(0, 2); } catch(const std::out_of_range &) { cnotRangeThrew = true; }
	assert(cnotRangeThrew);

	bool cnotDistinctThrew = false;
	try { qreg.applyControlledNot(1, 1); } catch(const std::invalid_argument &) { cnotDistinctThrew = true; }
	assert(cnotDistinctThrew);

	bool swapDistinctThrew = false;
	try { qreg.applySwap(0, 0); } catch(const std::invalid_argument &) { swapDistinctThrew = true; }
	assert(swapDistinctThrew);
}

void testApplyGateValidation() {
	using namespace tmfqs;
	std::cout << "=== applyGate validation ===\n";
	QuantumRegister qreg(2, 0);

	bool duplicateThrew = false;
	try {
		qreg.applyGate(QuantumGate::ControlledNot(), QubitList{1, 1});
	} catch(const std::invalid_argument &) {
		duplicateThrew = true;
	}
	assert(duplicateThrew);

	bool rangeThrew = false;
	try {
		qreg.applyGate(QuantumGate::Hadamard(), QubitList{2});
	} catch(const std::out_of_range &) {
		rangeThrew = true;
	}
	assert(rangeThrew);
}

void testUniformSuperpositionValidation() {
	using namespace tmfqs;
	std::cout << "=== uniform superposition validation ===\n";
	QuantumRegister qreg(3, 0);

	bool emptyThrew = false;
	try {
		qreg.initUniformSuperposition(BasisStateList{});
	} catch(const std::invalid_argument &) {
		emptyThrew = true;
	}
	assert(emptyThrew);

	bool outOfRangeThrew = false;
	try {
		qreg.initUniformSuperposition(BasisStateList{0, 8});
	} catch(const std::out_of_range &) {
		outOfRangeThrew = true;
	}
	assert(outOfRangeThrew);
}

void testApplyGateBuiltinDispatchParity() {
	using namespace tmfqs;
	std::cout << "=== applyGate built-in parity ===\n";
	if(!isStrategyAvailable(StorageStrategyKind::Blosc)) {
		std::cout << "Blosc backend unavailable in this build, skipping applyGate built-in parity test.\n";
		return;
	}

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	RegisterConfig bloscCfg;
	bloscCfg.strategy = StorageStrategyKind::Blosc;
	bloscCfg.blosc.chunkStates = 8;
	bloscCfg.blosc.gateCacheSlots = 4;

	QuantumRegister denseReg(4, 0, denseCfg);
	QuantumRegister bloscReg(4, 0, bloscCfg);

	const QuantumGate hadamard = QuantumGate::Hadamard();
	const QuantumGate pauliX = QuantumGate::PauliX();
	const QuantumGate cnot = QuantumGate::ControlledNot();
	const QuantumGate cps = QuantumGate::ControlledPhaseShift(kPi / 7.0);

	denseReg.applyGate(hadamard, QubitList{0});
	denseReg.applyGate(pauliX, QubitList{3});
	denseReg.applyGate(cnot, QubitList{0, 2});
	denseReg.applyGate(cps, QubitList{2, 1});

	bloscReg.applyGate(hadamard, QubitList{0});
	bloscReg.applyGate(pauliX, QubitList{3});
	bloscReg.applyGate(cnot, QubitList{0, 2});
	bloscReg.applyGate(cps, QubitList{2, 1});

	for(unsigned int s = 0; s < denseReg.stateCount(); ++s) {
		const Amplitude a = denseReg.amplitude(s);
		const Amplitude b = bloscReg.amplitude(s);
		assert(approxEqual(a.real, b.real, 1e-9));
		assert(approxEqual(a.imag, b.imag, 1e-9));
	}
	assert(approxEqual(denseReg.totalProbability(), bloscReg.totalProbability(), 1e-9));
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
	testDefaultConstructedRegisterIsValid();
	testOutOfRangeQueriesThrow();
	testResolvedStrategyConsistency();
	testDenseVsBloscParity();
	testPublicGateValidation();
	testApplyGateValidation();
	testApplyGateBuiltinDispatchParity();
	testUniformSuperpositionValidation();

	std::cout << "All integration tests passed.\n";
	return 0;
}
