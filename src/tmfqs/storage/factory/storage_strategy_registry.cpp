#include "tmfqs/storage/factory/storage_strategy_registry.h"

#include <algorithm>
#include <thread>
#include <stdexcept>

#include "tmfqs/core/state_space.h"
#include "tmfqs/storage/blosc/blosc_state_backend.h"
#include "tmfqs/storage/dense/dense_state_backend.h"
#include "tmfqs/storage/zfp/zfp_state_backend.h"

namespace tmfqs {
namespace {

constexpr size_t kBytesPerState = sizeof(double) * 2u;
constexpr size_t kGenericChunkBytes = 256u * 1024u;
constexpr size_t kFullSweepChunkBytes = 512u * 1024u;
constexpr size_t kGenericCacheBudgetBytes = 16u * 1024u * 1024u;
constexpr size_t kFullSweepCacheBudgetBytes = 4u * 1024u * 1024u;
constexpr size_t kGenericFullCacheLimitBytes = 16u * 1024u * 1024u;
constexpr size_t kFullSweepFullCacheLimitBytes = 4u * 1024u * 1024u;

bool isFullSweepHint(StorageWorkloadHint hint) {
	return hint == StorageWorkloadHint::Grover || hint == StorageWorkloadHint::Qft;
}

size_t ceilDiv(size_t value, size_t divisor) {
	return divisor == 0u ? 0u : (value + divisor - 1u) / divisor;
}

size_t floorPowerOfTwo(size_t value) {
	size_t power = 1u;
	while(power <= value / 2u) {
		power <<= 1u;
	}
	return power;
}

size_t clampPowerOfTwoStates(size_t totalStates, size_t desiredStates) {
	if(totalStates <= 1u) {
		return 1u;
	}
	const size_t maxStates = floorPowerOfTwo(totalStates);
	const size_t minStates = std::min<size_t>(1024u, maxStates);
	const size_t clamped = std::max(minStates, std::min(desiredStates, maxStates));
	return floorPowerOfTwo(clamped);
}

unsigned int hardwareConcurrencyOrOne() {
	const unsigned int hw = std::thread::hardware_concurrency();
	return hw == 0u ? 1u : hw;
}

size_t chooseChunkStates(size_t totalStates, StorageWorkloadHint hint) {
	const size_t targetBytes = isFullSweepHint(hint) ? kFullSweepChunkBytes : kGenericChunkBytes;
	const size_t desiredStates = std::max<size_t>(1u, targetBytes / kBytesPerState);
	return clampPowerOfTwoStates(totalStates, desiredStates);
}

size_t chooseCacheSlots(size_t totalStates, size_t chunkStates, StorageWorkloadHint hint) {
	const size_t chunkCount = std::max<size_t>(1u, ceilDiv(totalStates, chunkStates));
	if(chunkCount <= 2u) {
		return 2u;
	}

	const size_t totalBytes = totalStates * kBytesPerState;
	const size_t chunkBytes = std::max<size_t>(1u, chunkStates * kBytesPerState);
	const size_t fullCacheLimitBytes = isFullSweepHint(hint) ? kFullSweepFullCacheLimitBytes : kGenericFullCacheLimitBytes;
	if(totalBytes <= fullCacheLimitBytes) {
		return chunkCount;
	}

	const size_t cacheBudgetBytes = isFullSweepHint(hint) ? kFullSweepCacheBudgetBytes : kGenericCacheBudgetBytes;
	const size_t slots = std::max<size_t>(2u, cacheBudgetBytes / chunkBytes);
	return std::min(chunkCount, slots);
}

int chooseBloscCompressionLevel(size_t totalBytes, StorageWorkloadHint hint) {
	if(isFullSweepHint(hint)) {
		return 1;
	}
	return totalBytes >= (128u * 1024u * 1024u) ? 2 : 1;
}

int chooseBloscThreads(size_t totalBytes, StorageWorkloadHint hint) {
	const unsigned int hw = hardwareConcurrencyOrOne();
	if(hw <= 1u || totalBytes < (8u * 1024u * 1024u)) {
		return 1;
	}
	if(isFullSweepHint(hint)) {
		return static_cast<int>(std::min<unsigned int>(hw, 4u));
	}
	return totalBytes >= (64u * 1024u * 1024u) ? static_cast<int>(std::min<unsigned int>(hw, 2u)) : 1;
}

} // namespace

/** @brief Resolves effective storage strategy from config and availability. */
StorageStrategyKind StorageStrategyRegistry::resolve(unsigned int numQubits, const RegisterConfig &cfg) {
	StorageStrategyKind selected = cfg.strategy;
	if(selected != StorageStrategyKind::Auto) {
		return selected;
	}

	// Prefer dense storage for small registers to avoid compression overhead.
	const size_t estimatedBytes = checkedAmplitudeElementCount(numQubits) * sizeof(double);
	if(estimatedBytes < cfg.autoThresholdBytes) {
		return StorageStrategyKind::Dense;
	}
	if(isBloscStateBackendAvailable()) {
		return StorageStrategyKind::Blosc;
	}
	if(isZfpStateBackendAvailable()) {
		return StorageStrategyKind::Zfp;
	}
	return StorageStrategyKind::Dense;
}

/** @brief Checks whether a strategy is available in this build. */
bool StorageStrategyRegistry::isAvailable(StorageStrategyKind kind) {
	switch(kind) {
		case StorageStrategyKind::Dense:
			return true;
		case StorageStrategyKind::Blosc:
			return isBloscStateBackendAvailable();
		case StorageStrategyKind::Zfp:
			return isZfpStateBackendAvailable();
		case StorageStrategyKind::Auto:
			return true;
	}
	return false;
}

/** @brief Lists available strategy names. */
std::vector<std::string> StorageStrategyRegistry::listAvailable() {
	std::vector<std::string> names;
	names.push_back("dense");
	if(isBloscStateBackendAvailable()) {
		names.push_back("blosc");
	}
	if(isZfpStateBackendAvailable()) {
		names.push_back("zfp");
	}
	names.push_back("auto");
	return names;
}

/** @brief Converts a strategy enum value to lower-case string name. */
std::string StorageStrategyRegistry::toString(StorageStrategyKind kind) {
	switch(kind) {
		case StorageStrategyKind::Dense:
			return "dense";
		case StorageStrategyKind::Blosc:
			return "blosc";
		case StorageStrategyKind::Zfp:
			return "zfp";
		case StorageStrategyKind::Auto:
			return "auto";
	}
	return "unknown";
}

/** @brief Applies backend-specific tuning while keeping explicit overrides intact. */
RegisterConfig StorageStrategyRegistry::tuneConfig(
	unsigned int numQubits,
	const RegisterConfig &cfg,
	StorageStrategyKind resolvedKind) {
	RegisterConfig tuned = cfg;
	const size_t totalStates = checkedStateCount(numQubits);
	const size_t totalBytes = checkedAmplitudeElementCount(numQubits) * sizeof(double);

	switch(resolvedKind) {
		case StorageStrategyKind::Dense:
		case StorageStrategyKind::Auto:
			return tuned;
		case StorageStrategyKind::Blosc: {
			const BloscConfig defaultBlosc;
			if(!tuned.bloscOverrides.chunkStates && tuned.blosc.chunkStates == defaultBlosc.chunkStates) {
				tuned.blosc.chunkStates = chooseChunkStates(totalStates, tuned.workloadHint);
			}
			if(!tuned.bloscOverrides.gateCacheSlots && tuned.blosc.gateCacheSlots == defaultBlosc.gateCacheSlots) {
				tuned.blosc.gateCacheSlots = chooseCacheSlots(totalStates, tuned.blosc.chunkStates, tuned.workloadHint);
			}
			if(!tuned.bloscOverrides.clevel && tuned.blosc.clevel == defaultBlosc.clevel) {
				tuned.blosc.clevel = chooseBloscCompressionLevel(totalBytes, tuned.workloadHint);
			}
			if(!tuned.bloscOverrides.nthreads && tuned.blosc.nthreads == defaultBlosc.nthreads) {
				tuned.blosc.nthreads = chooseBloscThreads(totalBytes, tuned.workloadHint);
			}
			return tuned;
		}
		case StorageStrategyKind::Zfp: {
			const ZfpConfig defaultZfp;
			if(!tuned.zfpOverrides.chunkStates && tuned.zfp.chunkStates == defaultZfp.chunkStates) {
				tuned.zfp.chunkStates = chooseChunkStates(totalStates, tuned.workloadHint);
			}
			if(!tuned.zfpOverrides.gateCacheSlots && tuned.zfp.gateCacheSlots == defaultZfp.gateCacheSlots) {
				tuned.zfp.gateCacheSlots = chooseCacheSlots(totalStates, tuned.zfp.chunkStates, tuned.workloadHint);
			}
			return tuned;
		}
	}
	return tuned;
}

/** @brief Resolves and constructs the backend selection object. */
BackendSelection StorageStrategyRegistry::createSelection(unsigned int numQubits, const RegisterConfig &cfg) {
	BackendSelection selection;
	selection.strategy = resolve(numQubits, cfg);
	selection.config = tuneConfig(numQubits, cfg, selection.strategy);
	selection.config.strategy = selection.strategy;

	// Resolve once, then enforce availability for the chosen backend.
	switch(selection.strategy) {
		case StorageStrategyKind::Dense:
			selection.backend = createDenseStateBackend(selection.config);
			break;
		case StorageStrategyKind::Blosc:
			selection.backend = createBloscStateBackend(selection.config);
			if(!selection.backend) {
				throw std::runtime_error("Blosc backend requested but unavailable in this build");
			}
			break;
		case StorageStrategyKind::Zfp:
			selection.backend = createZfpStateBackend(selection.config);
			if(!selection.backend) {
				throw std::runtime_error("Zfp backend requested but unavailable in this build");
			}
			break;
		case StorageStrategyKind::Auto:
			throw std::runtime_error("StorageStrategyRegistry internal error: unresolved auto strategy");
	}
	return selection;
}

} // namespace tmfqs
