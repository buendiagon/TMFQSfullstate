#include "tmfqsfs.h"

#include <cmath>
#include <iostream>
#include <vector>

#include "../common/test_helpers.h"

namespace {

class FixedRandomSource final : public tmfqs::IRandomSource {
	public:
		explicit FixedRandomSource(double value) : value_(value) {}

		double nextUnitDouble() override {
			return value_;
		}

	private:
		double value_ = 0.0;
};

void applyParityCircuit(tmfqs::QuantumRegister &reg) {
	reg.beginOperationBatch();
	reg.applyHadamard(0);
	reg.applyHadamard(2);
	reg.applyControlledNot(0, 1);
	reg.applyControlledPhaseShift(2, 3, tmfqs::kPi / 8.0);
	reg.applySwap(1, 3);
	reg.applyPhaseFlipBasisState(6);
	reg.applyInversionAboutMean();
	reg.endOperationBatch();
}

void applyCopyCircuit(tmfqs::QuantumRegister &reg) {
	reg.beginOperationBatch();
	reg.applyHadamard(0);
	reg.applyHadamard(1);
	reg.applyControlledNot(0, 2);
	reg.applyControlledPhaseShift(1, 4, tmfqs::kPi / 9.0);
	reg.applyPhaseFlipBasisState(9);
	reg.applyInversionAboutMean();
	reg.endOperationBatch();
}

void applyThreadedCircuit(tmfqs::QuantumRegister &reg) {
	reg.beginOperationBatch();
	reg.applyHadamard(0);
	reg.applyControlledNot(0, 3);
	reg.applyControlledPhaseShift(3, 4, tmfqs::kPi / 11.0);
	reg.applyPhaseFlipBasisState(19);
	reg.applyInversionAboutMean();
	reg.endOperationBatch();
}

void testResolvedStrategyConsistency() {
	using namespace tmfqs;
	std::cout << "=== strategy resolution consistency ===\n";
	RegisterConfig cfg;
	cfg.strategy = StorageStrategyKind::Auto;
	cfg.autoThresholdBytes = 1u;
	QuantumRegister qreg(3, cfg);
	const StorageStrategyKind expected = StorageStrategyRegistry::resolve(3, cfg);
	TMFQS_TEST_REQUIRE(qreg.storageStrategy() == expected);
}

void testDenseVsBloscParity() {
	using namespace tmfqs;
	std::cout << "=== dense vs blosc parity ===\n";
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Blosc)) {
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

	applyParityCircuit(denseReg);
	applyParityCircuit(bloscReg);
	tmfqs_test::assertRegistersClose(denseReg, bloscReg, 1e-9);
}

void testApplyGateBuiltinDispatchParity() {
	using namespace tmfqs;
	std::cout << "=== applyGate built-in parity ===\n";
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Blosc)) {
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

	tmfqs_test::assertRegistersClose(denseReg, bloscReg, 1e-9);
}

void testDenseVsZfpSmoke() {
	using namespace tmfqs;
	std::cout << "=== dense vs zfp smoke ===\n";
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		std::cout << "Zfp backend unavailable in this build, skipping zfp smoke test.\n";
		return;
	}

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	RegisterConfig zfpCfg;
	zfpCfg.strategy = StorageStrategyKind::Zfp;
	zfpCfg.zfp.mode = ZfpCompressionMode::FixedRate;
	zfpCfg.zfp.rate = 32.0;
	zfpCfg.zfp.chunkStates = 8;
	zfpCfg.zfp.gateCacheSlots = 4;

	QuantumRegister denseReg(4, 0, denseCfg);
	QuantumRegister zfpReg(4, 0, zfpCfg);

	applyParityCircuit(denseReg);
	applyParityCircuit(zfpReg);
	tmfqs_test::assertRegistersClose(denseReg, zfpReg, 5e-3);
}

void testOverlayMeasurementParity() {
	using namespace tmfqs;
	std::cout << "=== overlay measurement parity ===\n";

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

	auto meanOf = [](const QuantumRegister &reg) {
		Amplitude mean{0.0, 0.0};
		for(unsigned int state = 0; state < reg.stateCount(); ++state) {
			const Amplitude amp = reg.amplitude(state);
			mean.real += amp.real;
			mean.imag += amp.imag;
		}
		mean.real /= static_cast<double>(reg.stateCount());
		mean.imag /= static_cast<double>(reg.stateCount());
		return mean;
	};

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	QuantumRegister denseReg(3, amplitudes, denseCfg);
	const Amplitude mean = meanOf(denseReg);
	denseReg.applyInversionAboutMean(mean);

	const double expectedTotalProbability = denseReg.totalProbability();
	const double measurementSample = expectedTotalProbability * 0.5;
	FixedRandomSource denseRnd(measurementSample);
	const StateIndex expected = denseReg.measure(denseRnd);

	if(StorageStrategyRegistry::isAvailable(StorageStrategyKind::Blosc)) {
		RegisterConfig bloscCfg;
		bloscCfg.strategy = StorageStrategyKind::Blosc;
		bloscCfg.blosc.chunkStates = 4;
		bloscCfg.blosc.gateCacheSlots = 4;
		QuantumRegister bloscReg(3, amplitudes, bloscCfg);
		bloscReg.applyInversionAboutMean(mean);
		FixedRandomSource bloscRnd(measurementSample);
		TMFQS_TEST_REQUIRE(bloscReg.measure(bloscRnd) == expected);
		TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(bloscReg.totalProbability(), expectedTotalProbability, 1e-9));
	}

	if(StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		RegisterConfig zfpCfg;
		zfpCfg.strategy = StorageStrategyKind::Zfp;
		zfpCfg.zfp.mode = ZfpCompressionMode::FixedRate;
		zfpCfg.zfp.rate = 64.0;
		zfpCfg.zfp.chunkStates = 4;
		zfpCfg.zfp.gateCacheSlots = 4;
		QuantumRegister zfpReg(3, amplitudes, zfpCfg);
		zfpReg.applyInversionAboutMean(mean);
		FixedRandomSource zfpRnd(measurementSample);
		TMFQS_TEST_REQUIRE(zfpReg.measure(zfpRnd) == expected);
		TMFQS_TEST_REQUIRE(tmfqs_test::approxEqual(zfpReg.totalProbability(), expectedTotalProbability, 1e-6));
	}
}

void testZfpRegisterCopySemantics() {
	using namespace tmfqs;
	std::cout << "=== zfp register copy semantics ===\n";
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		std::cout << "Zfp backend unavailable in this build, skipping zfp copy test.\n";
		return;
	}

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	RegisterConfig zfpCfg;
	zfpCfg.strategy = StorageStrategyKind::Zfp;
	zfpCfg.zfp.mode = ZfpCompressionMode::FixedRate;
	zfpCfg.zfp.rate = 24.0;
	zfpCfg.zfp.chunkStates = 4;
	zfpCfg.zfp.gateCacheSlots = 4;

	QuantumRegister denseReg(5, 0, denseCfg);
	QuantumRegister zfpReg(5, 0, zfpCfg);
	applyCopyCircuit(denseReg);
	applyCopyCircuit(zfpReg);

	QuantumRegister copiedReg(zfpReg);
	tmfqs_test::assertRegistersClose(denseReg, copiedReg, 7e-3);

	QuantumRegister assignedReg(5, 0, zfpCfg);
	assignedReg = zfpReg;
	tmfqs_test::assertRegistersClose(denseReg, assignedReg, 7e-3);

	const Amplitude copiedStateBefore = copiedReg.amplitude(9);
	zfpReg.applyPhaseFlipBasisState(9);
	const Amplitude copiedStateAfter = copiedReg.amplitude(9);
	TMFQS_TEST_REQUIRE(std::abs(copiedStateBefore.real - copiedStateAfter.real) < 1e-9);
	TMFQS_TEST_REQUIRE(std::abs(copiedStateBefore.imag - copiedStateAfter.imag) < 1e-9);
}

void testZfpThreadedRoundTripFallback() {
	using namespace tmfqs;
	std::cout << "=== zfp threaded round-trip fallback ===\n";
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		std::cout << "Zfp backend unavailable in this build, skipping threaded zfp test.\n";
		return;
	}

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	RegisterConfig zfpCfg;
	zfpCfg.strategy = StorageStrategyKind::Zfp;
	zfpCfg.zfp.mode = ZfpCompressionMode::FixedPrecision;
	zfpCfg.zfp.precision = 40u;
	zfpCfg.zfp.chunkStates = 8u;
	zfpCfg.zfp.gateCacheSlots = 4u;
	zfpCfg.zfp.nthreads = 2;
	zfpCfg.zfpOverrides.nthreads = true;

	QuantumRegister denseReg(5, 0, denseCfg);
	QuantumRegister zfpReg(5, 0, zfpCfg);
	applyThreadedCircuit(denseReg);
	applyThreadedCircuit(zfpReg);
	tmfqs_test::assertRegistersClose(denseReg, zfpReg, 5e-4);
}

void testBackendDefaultPreservation() {
	using namespace tmfqs;
	std::cout << "=== backend default preservation ===\n";

	RegisterConfig zfpCfg;
	zfpCfg.strategy = StorageStrategyKind::Zfp;
	zfpCfg.workloadHint = StorageWorkloadHint::Grover;
	const RegisterConfig tunedZfp =
		StorageStrategyRegistry::tuneConfig(20, zfpCfg, StorageStrategyKind::Zfp);
	TMFQS_TEST_REQUIRE(tunedZfp.zfp.chunkStates == 32768u);
	TMFQS_TEST_REQUIRE(tunedZfp.zfp.gateCacheSlots == 8u);
	TMFQS_TEST_REQUIRE(tunedZfp.zfp.nthreads == 4);
	TMFQS_TEST_REQUIRE(tunedZfp.zfp.mode == ZfpCompressionMode::FixedPrecision);
	TMFQS_TEST_REQUIRE(tunedZfp.zfp.precision == 40u);

	RegisterConfig explicitZfpCfg = zfpCfg;
	explicitZfpCfg.zfp.chunkStates = 16384u;
	explicitZfpCfg.zfp.nthreads = 1;
	explicitZfpCfg.zfp.gateCacheSlots = 8u;
	explicitZfpCfg.zfpOverrides.chunkStates = true;
	explicitZfpCfg.zfpOverrides.nthreads = true;
	explicitZfpCfg.zfpOverrides.gateCacheSlots = true;
	const RegisterConfig preservedZfp =
		StorageStrategyRegistry::tuneConfig(20, explicitZfpCfg, StorageStrategyKind::Zfp);
	TMFQS_TEST_REQUIRE(preservedZfp.zfp.chunkStates == 16384u);
	TMFQS_TEST_REQUIRE(preservedZfp.zfp.gateCacheSlots == 8u);
	TMFQS_TEST_REQUIRE(preservedZfp.zfp.nthreads == 1);

	RegisterConfig bloscCfg;
	bloscCfg.strategy = StorageStrategyKind::Blosc;
	bloscCfg.workloadHint = StorageWorkloadHint::Qft;
	const RegisterConfig tunedBlosc =
		StorageStrategyRegistry::tuneConfig(19, bloscCfg, StorageStrategyKind::Blosc);
	TMFQS_TEST_REQUIRE(tunedBlosc.blosc.chunkStates == 32768u);
	TMFQS_TEST_REQUIRE(tunedBlosc.blosc.gateCacheSlots == 8u);
	TMFQS_TEST_REQUIRE(tunedBlosc.blosc.clevel == 1);
	TMFQS_TEST_REQUIRE(tunedBlosc.blosc.nthreads == 4);
	TMFQS_TEST_REQUIRE(tunedBlosc.blosc.compcode == 1);
	TMFQS_TEST_REQUIRE(tunedBlosc.blosc.useShuffle);

	RegisterConfig explicitBloscCfg = bloscCfg;
	explicitBloscCfg.blosc.chunkStates = 4096u;
	explicitBloscCfg.blosc.gateCacheSlots = 3u;
	explicitBloscCfg.blosc.nthreads = 1;
	explicitBloscCfg.bloscOverrides.chunkStates = true;
	explicitBloscCfg.bloscOverrides.gateCacheSlots = true;
	explicitBloscCfg.bloscOverrides.nthreads = true;
	const RegisterConfig preservedBlosc =
		StorageStrategyRegistry::tuneConfig(19, explicitBloscCfg, StorageStrategyKind::Blosc);
	TMFQS_TEST_REQUIRE(preservedBlosc.blosc.chunkStates == 4096u);
	TMFQS_TEST_REQUIRE(preservedBlosc.blosc.gateCacheSlots == 3u);
	TMFQS_TEST_REQUIRE(preservedBlosc.blosc.nthreads == 1);
}

void testRegistryAvailabilityBehavior() {
	using namespace tmfqs;
	std::cout << "=== strategy registry availability ===\n";
	const std::vector<std::string> names = StorageStrategyRegistry::listAvailable();
	TMFQS_TEST_REQUIRE(!names.empty());
	TMFQS_TEST_REQUIRE(StorageStrategyRegistry::toString(StorageStrategyKind::Dense) == "dense");
	TMFQS_TEST_REQUIRE(StorageStrategyRegistry::toString(StorageStrategyKind::Auto) == "auto");

	RegisterConfig cfg;
	cfg.strategy = StorageStrategyKind::Blosc;
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Blosc)) {
		bool threw = false;
		try {
			(void)StateBackendFactory::create(4, cfg);
		} catch(const std::runtime_error &) {
			threw = true;
		}
		TMFQS_TEST_REQUIRE(threw);
	}

	cfg.strategy = StorageStrategyKind::Zfp;
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		bool threw = false;
		try {
			(void)StateBackendFactory::create(4, cfg);
		} catch(const std::runtime_error &) {
			threw = true;
		}
		TMFQS_TEST_REQUIRE(threw);
	}
}

} // namespace

int main() {
	testResolvedStrategyConsistency();
	testRegistryAvailabilityBehavior();
	testDenseVsBloscParity();
	testApplyGateBuiltinDispatchParity();
	testDenseVsZfpSmoke();
	testOverlayMeasurementParity();
	testZfpRegisterCopySemantics();
	testZfpThreadedRoundTripFallback();
	testBackendDefaultPreservation();
	std::cout << "Storage parity tests passed.\n";
	return 0;
}
