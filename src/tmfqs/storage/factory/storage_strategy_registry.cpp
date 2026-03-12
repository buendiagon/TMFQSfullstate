#include "tmfqs/storage/factory/storage_strategy_registry.h"

#include <stdexcept>
#include <utility>

#include "tmfqs/core/state_space.h"
#include "tmfqs/storage/blosc/blosc_state_backend.h"
#include "tmfqs/storage/dense/dense_state_backend.h"
#include "tmfqs/storage/zfp/zfp_state_backend.h"

namespace tmfqs {

StorageStrategyKind StorageStrategyRegistry::resolve(unsigned int numQubits, const RegisterConfig &cfg) {
	StorageStrategyKind selected = cfg.strategy;
	if(selected != StorageStrategyKind::Auto) {
		return selected;
	}

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

BackendSelection StorageStrategyRegistry::createSelection(unsigned int numQubits, const RegisterConfig &cfg) {
	BackendSelection selection;
	selection.strategy = resolve(numQubits, cfg);

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
