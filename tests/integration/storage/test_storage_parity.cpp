#include "tmfqsfs.h"

#include <cassert>
#include <iostream>
#include <vector>

#include "../common/test_helpers.h"

namespace {

void testResolvedStrategyConsistency() {
	using namespace tmfqs;
	std::cout << "=== strategy resolution consistency ===\n";
	RegisterConfig cfg;
	cfg.strategy = StorageStrategyKind::Auto;
	cfg.autoThresholdBytes = 1u;
	QuantumRegister qreg(3, cfg);
	const StorageStrategyKind expected = StorageStrategyRegistry::resolve(3, cfg);
	assert(qreg.storageStrategy() == expected);
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
	algorithms::executeOperations(zfpReg, ops);
	tmfqs_test::assertRegistersClose(denseReg, zfpReg, 5e-3);
}

void testRegistryAvailabilityBehavior() {
	using namespace tmfqs;
	std::cout << "=== strategy registry availability ===\n";
	const std::vector<std::string> names = StorageStrategyRegistry::listAvailable();
	assert(!names.empty());
	assert(StorageStrategyRegistry::toString(StorageStrategyKind::Dense) == "dense");
	assert(StorageStrategyRegistry::toString(StorageStrategyKind::Auto) == "auto");

	RegisterConfig cfg;
	cfg.strategy = StorageStrategyKind::Blosc;
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Blosc)) {
		bool threw = false;
		try {
			(void)StateBackendFactory::create(4, cfg);
		} catch(const std::runtime_error &) {
			threw = true;
		}
		assert(threw);
	}

	cfg.strategy = StorageStrategyKind::Zfp;
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		bool threw = false;
		try {
			(void)StateBackendFactory::create(4, cfg);
		} catch(const std::runtime_error &) {
			threw = true;
		}
		assert(threw);
	}
}

} // namespace

int main() {
	testResolvedStrategyConsistency();
	testRegistryAvailabilityBehavior();
	testDenseVsBloscParity();
	testApplyGateBuiltinDispatchParity();
	testDenseVsZfpSmoke();
	std::cout << "Storage parity tests passed.\n";
	return 0;
}
