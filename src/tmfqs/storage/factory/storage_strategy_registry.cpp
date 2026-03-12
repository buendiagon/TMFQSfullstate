#include "tmfqs/storage/factory/storage_strategy_registry.h"

#include <stdexcept>
#include <utility>

#include "tmfqs/core/state_space.h"
#include "tmfqs/storage/blosc/blosc_state_backend.h"
#include "tmfqs/storage/dense/dense_state_backend.h"
#include "tmfqs/storage/zfp/zfp_state_backend.h"

namespace tmfqs {

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

/** @brief Resolves and constructs the backend selection object. */
BackendSelection StorageStrategyRegistry::createSelection(unsigned int numQubits, const RegisterConfig &cfg) {
	BackendSelection selection;
	selection.strategy = resolve(numQubits, cfg);

	// Resolve once, then enforce availability for the chosen backend.
	switch(selection.strategy) {
		case StorageStrategyKind::Dense:
			selection.backend = createDenseStateBackend(cfg);
			break;
		case StorageStrategyKind::Blosc:
			selection.backend = createBloscStateBackend(cfg);
			if(!selection.backend) {
				throw std::runtime_error("Blosc backend requested but unavailable in this build");
			}
			break;
		case StorageStrategyKind::Zfp:
			selection.backend = createZfpStateBackend(cfg);
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
