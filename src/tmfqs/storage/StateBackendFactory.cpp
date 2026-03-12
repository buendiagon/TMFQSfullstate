#include "tmfqs/storage/state_backend_factory.h"

#include <stdexcept>
#include <utility>

#include "tmfqs/core/state_space.h"

namespace tmfqs {

std::unique_ptr<IStateBackend> createDenseBackend(unsigned int numQubits, const RegisterConfig &cfg);
std::unique_ptr<IStateBackend> createBloscBackend(unsigned int numQubits, const RegisterConfig &cfg);
std::unique_ptr<IStateBackend> createZfpBackend(unsigned int numQubits, const RegisterConfig &cfg);
bool isBloscBackendCompiled();
bool isZfpBackendCompiled();

std::string storageStrategyToString(StorageStrategyKind kind) {
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

bool isStrategyAvailable(StorageStrategyKind kind) {
	switch(kind) {
		case StorageStrategyKind::Dense:
			return true;
		case StorageStrategyKind::Blosc:
			return isBloscBackendCompiled();
		case StorageStrategyKind::Zfp:
			return isZfpBackendCompiled();
		case StorageStrategyKind::Auto:
			return true;
	}
	return false;
}

std::vector<std::string> listAvailableStrategies() {
	std::vector<std::string> strategies;
	strategies.push_back("dense");
	if(isBloscBackendCompiled()) {
		strategies.push_back("blosc");
	}
	if(isZfpBackendCompiled()) {
		strategies.push_back("zfp");
	}
	strategies.push_back("auto");
	return strategies;
}

StorageStrategyKind resolveStorageStrategy(unsigned int numQubits, const RegisterConfig &cfg) {
	StorageStrategyKind selected = cfg.strategy;
	if(selected == StorageStrategyKind::Auto) {
		const size_t estimatedBytes = checkedAmplitudeElementCount(numQubits) * sizeof(double);
		if(estimatedBytes >= cfg.autoThresholdBytes) {
			if(isBloscBackendCompiled()) {
				selected = StorageStrategyKind::Blosc;
			} else if(isZfpBackendCompiled()) {
				selected = StorageStrategyKind::Zfp;
			} else {
				selected = StorageStrategyKind::Dense;
			}
		} else {
			selected = StorageStrategyKind::Dense;
		}
	}
	return selected;
}

std::unique_ptr<IStateBackend> createBackend(unsigned int numQubits, const RegisterConfig &cfg) {
	return createBackendSelection(numQubits, cfg).backend;
}

BackendSelection createBackendSelection(unsigned int numQubits, const RegisterConfig &cfg) {
	BackendSelection selection;
	selection.strategy = resolveStorageStrategy(numQubits, cfg);

	if(selection.strategy == StorageStrategyKind::Dense) {
		selection.backend = createDenseBackend(numQubits, cfg);
		return selection;
	}

	if(selection.strategy == StorageStrategyKind::Blosc) {
		auto backend = createBloscBackend(numQubits, cfg);
		if(!backend) {
			throw std::runtime_error("Blosc backend requested but unavailable in this build");
		}
		selection.backend = std::move(backend);
		return selection;
	}

	if(selection.strategy == StorageStrategyKind::Zfp) {
		auto backend = createZfpBackend(numQubits, cfg);
		if(!backend) {
			throw std::runtime_error("Zfp backend requested but unavailable in this build");
		}
		selection.backend = std::move(backend);
		return selection;
	}

	throw std::runtime_error("Unsupported storage strategy requested");
}

} // namespace tmfqs
