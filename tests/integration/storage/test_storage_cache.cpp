#include "tmfqsfs.h"

#include <cassert>
#include <iostream>

#include "../common/test_helpers.h"

namespace {

void runStressBatch(tmfqs::QuantumRegister &reg) {
	for(unsigned int q = 0; q < reg.qubitCount(); ++q) {
		reg.applyHadamard(q);
	}
	reg.beginOperationBatch();
	for(unsigned int i = 0; i < 48u; ++i) {
		const tmfqs::QubitIndex q0 = i % reg.qubitCount();
		const tmfqs::QubitIndex q1 = (q0 + 2u) % reg.qubitCount();
		const tmfqs::QubitIndex q2 = (q0 + 4u) % reg.qubitCount();
		reg.applyHadamard(q0);
		reg.applyControlledNot(q1, q0);
		reg.applyControlledPhaseShift(q2, q0, tmfqs::kPi / 17.0);
	}
	reg.endOperationBatch();
}

void testBloscCacheEvictionAndFlush() {
	using namespace tmfqs;
	std::cout << "=== blosc cache eviction + dirty flush ===\n";
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Blosc)) {
		std::cout << "Blosc backend unavailable, skipping cache stress test.\n";
		return;
	}

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	RegisterConfig bloscCfg;
	bloscCfg.strategy = StorageStrategyKind::Blosc;
	bloscCfg.blosc.chunkStates = 2;
	bloscCfg.blosc.gateCacheSlots = 2;

	QuantumRegister denseReg(6, 0, denseCfg);
	QuantumRegister bloscReg(6, 0, bloscCfg);
	runStressBatch(denseReg);
	runStressBatch(bloscReg);
	tmfqs_test::assertRegistersClose(denseReg, bloscReg, 1e-9);
}

void testZfpCacheEvictionAndFlush() {
	using namespace tmfqs;
	std::cout << "=== zfp cache eviction + dirty flush ===\n";
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		std::cout << "Zfp backend unavailable, skipping cache stress test.\n";
		return;
	}

	RegisterConfig denseCfg;
	denseCfg.strategy = StorageStrategyKind::Dense;
	RegisterConfig zfpCfg;
	zfpCfg.strategy = StorageStrategyKind::Zfp;
	zfpCfg.zfp.mode = ZfpCompressionMode::FixedRate;
	zfpCfg.zfp.rate = 24.0;
	zfpCfg.zfp.chunkStates = 2;
	zfpCfg.zfp.gateCacheSlots = 2;

	QuantumRegister denseReg(6, 0, denseCfg);
	QuantumRegister zfpReg(6, 0, zfpCfg);
	runStressBatch(denseReg);
	runStressBatch(zfpReg);
	tmfqs_test::assertRegistersClose(denseReg, zfpReg, 7e-3);
}

tmfqs::QuantumRegister runKernelModeCase(const tmfqs::RegisterConfig &cfg, tmfqs::QubitIndex targetQubit) {
	tmfqs::QuantumRegister reg(5, 0, cfg);
	for(unsigned int q = 0; q < reg.qubitCount(); ++q) {
		reg.applyHadamard(q);
	}
	reg.applyHadamard(targetQubit);
	reg.applyPauliX(targetQubit);
	reg.applyControlledNot((targetQubit + 1u) % reg.qubitCount(), targetQubit);
	reg.applyControlledPhaseShift((targetQubit + 2u) % reg.qubitCount(), targetQubit, tmfqs::kPi / 9.0);
	return reg;
}

void assertModeMatchesDense(const tmfqs::RegisterConfig &cfg, tmfqs::QubitIndex targetQubit, double tol) {
	tmfqs::RegisterConfig denseCfg;
	denseCfg.strategy = tmfqs::StorageStrategyKind::Dense;
	const tmfqs::QuantumRegister denseReg = runKernelModeCase(denseCfg, targetQubit);
	const tmfqs::QuantumRegister candidateReg = runKernelModeCase(cfg, targetQubit);
	tmfqs_test::assertRegistersClose(denseReg, candidateReg, tol);
}

void testBloscPairKernelModeEquivalence() {
	using namespace tmfqs;
	std::cout << "=== blosc pair-kernel mode equivalence ===\n";
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Blosc)) {
		std::cout << "Blosc backend unavailable, skipping pair-kernel test.\n";
		return;
	}

	RegisterConfig fallbackCfg;
	fallbackCfg.strategy = StorageStrategyKind::Blosc;
	fallbackCfg.blosc.chunkStates = 3;
	fallbackCfg.blosc.gateCacheSlots = 4;

	RegisterConfig intraCfg;
	intraCfg.strategy = StorageStrategyKind::Blosc;
	intraCfg.blosc.chunkStates = 8;
	intraCfg.blosc.gateCacheSlots = 4;

	RegisterConfig interCfg;
	interCfg.strategy = StorageStrategyKind::Blosc;
	interCfg.blosc.chunkStates = 4;
	interCfg.blosc.gateCacheSlots = 4;

	assertModeMatchesDense(fallbackCfg, 2, 1e-9);
	assertModeMatchesDense(intraCfg, 4, 1e-9);
	assertModeMatchesDense(interCfg, 2, 1e-9);
}

void testZfpPairKernelModeEquivalence() {
	using namespace tmfqs;
	std::cout << "=== zfp pair-kernel mode equivalence ===\n";
	if(!StorageStrategyRegistry::isAvailable(StorageStrategyKind::Zfp)) {
		std::cout << "Zfp backend unavailable, skipping pair-kernel test.\n";
		return;
	}

	RegisterConfig fallbackCfg;
	fallbackCfg.strategy = StorageStrategyKind::Zfp;
	fallbackCfg.zfp.mode = ZfpCompressionMode::FixedRate;
	fallbackCfg.zfp.rate = 24.0;
	fallbackCfg.zfp.chunkStates = 3;
	fallbackCfg.zfp.gateCacheSlots = 4;

	RegisterConfig intraCfg;
	intraCfg.strategy = StorageStrategyKind::Zfp;
	intraCfg.zfp.mode = ZfpCompressionMode::FixedRate;
	intraCfg.zfp.rate = 24.0;
	intraCfg.zfp.chunkStates = 8;
	intraCfg.zfp.gateCacheSlots = 4;

	RegisterConfig interCfg;
	interCfg.strategy = StorageStrategyKind::Zfp;
	interCfg.zfp.mode = ZfpCompressionMode::FixedRate;
	interCfg.zfp.rate = 24.0;
	interCfg.zfp.chunkStates = 4;
	interCfg.zfp.gateCacheSlots = 4;

	assertModeMatchesDense(fallbackCfg, 2, 7e-3);
	assertModeMatchesDense(intraCfg, 4, 7e-3);
	assertModeMatchesDense(interCfg, 2, 7e-3);
}

} // namespace

int main() {
	testBloscCacheEvictionAndFlush();
	testZfpCacheEvictionAndFlush();
	testBloscPairKernelModeEquivalence();
	testZfpPairKernelModeEquivalence();
	std::cout << "Storage cache/kernel tests passed.\n";
	return 0;
}
