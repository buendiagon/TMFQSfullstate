#ifndef TMFQS_STORAGE_STATE_BACKEND_FACTORY_H
#define TMFQS_STORAGE_STATE_BACKEND_FACTORY_H

#include "tmfqs/storage/factory/state_backend_factory.h"
#include "tmfqs/storage/factory/storage_strategy_registry.h"

namespace tmfqs {

inline StorageStrategyKind resolveStorageStrategy(unsigned int numQubits, const RegisterConfig &cfg) {
	return StorageStrategyRegistry::resolve(numQubits, cfg);
}

inline BackendSelection createBackendSelection(unsigned int numQubits, const RegisterConfig &cfg) {
	return StateBackendFactory::createSelection(numQubits, cfg);
}

inline std::unique_ptr<IStateBackend> createBackend(unsigned int numQubits, const RegisterConfig &cfg) {
	return StateBackendFactory::create(numQubits, cfg);
}

inline bool isStrategyAvailable(StorageStrategyKind kind) {
	return StorageStrategyRegistry::isAvailable(kind);
}

inline std::vector<std::string> listAvailableStrategies() {
	return StorageStrategyRegistry::listAvailable();
}

inline std::string storageStrategyToString(StorageStrategyKind kind) {
	return StorageStrategyRegistry::toString(kind);
}

} // namespace tmfqs

#endif // TMFQS_STORAGE_STATE_BACKEND_FACTORY_H
