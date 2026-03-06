#include "storage/StateBackendFactory.h"
#include "stateSpace.h"

#include <stdexcept>

std::unique_ptr<IStateBackend> createDenseBackend(unsigned int numQubits, const RegisterConfig &cfg);
std::unique_ptr<IStateBackend> createBloscBackend(unsigned int numQubits, const RegisterConfig &cfg);
bool isBloscBackendCompiled();

// User-facing strategy names for CLI/help output.
std::string storageStrategyToString(StorageStrategyKind kind) {
	switch(kind) {
		case StorageStrategyKind::Dense:
			return "dense";
		case StorageStrategyKind::Blosc:
			return "blosc";
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
		case StorageStrategyKind::Auto:
			return true;
	}
	return false;
}

// Returns only strategies supported by the current build.
std::vector<std::string> listAvailableStrategies() {
	std::vector<std::string> out;
	out.push_back("dense");
	if(isBloscBackendCompiled()) out.push_back("blosc");
	out.push_back("auto");
	return out;
}

// Central strategy selection and backend construction entry point.
std::unique_ptr<IStateBackend> createBackend(unsigned int numQubits, const RegisterConfig &cfg) {
	StorageStrategyKind selected = cfg.strategy;
	if(selected == StorageStrategyKind::Auto) {
		size_t estimatedBytes = checkedAmplitudeElementCount(numQubits) * sizeof(double);
		if(estimatedBytes >= cfg.autoThresholdBytes && isBloscBackendCompiled()) {
			selected = StorageStrategyKind::Blosc;
		} else {
			selected = StorageStrategyKind::Dense;
		}
	}

	if(selected == StorageStrategyKind::Dense) {
		return createDenseBackend(numQubits, cfg);
	}

	if(selected == StorageStrategyKind::Blosc) {
		auto backend = createBloscBackend(numQubits, cfg);
		if(!backend) {
			throw std::runtime_error("Blosc backend requested but unavailable in this build");
		}
		return backend;
	}

	throw std::runtime_error("Unsupported storage strategy requested");
}
