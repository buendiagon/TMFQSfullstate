#ifndef TMFQS_STORAGE_STATE_BACKEND_FACTORY_H
#define TMFQS_STORAGE_STATE_BACKEND_FACTORY_H

#include "tmfqs/storage/factory/state_backend_factory.h"
#include "tmfqs/storage/factory/storage_strategy_registry.h"

namespace tmfqs {

/**
 * @brief Resolves the effective storage strategy for a register.
 * @param numQubits Register size.
 * @param cfg Register configuration.
 * @return Selected strategy.
 */
inline StorageStrategyKind resolveStorageStrategy(unsigned int numQubits, const RegisterConfig &cfg) {
	return StorageStrategyRegistry::resolve(numQubits, cfg);
}

/**
 * @brief Resolves strategy and constructs the backend.
 * @param numQubits Register size.
 * @param cfg Register configuration.
 * @return Selection with strategy and backend instance.
 */
inline BackendSelection createBackendSelection(unsigned int numQubits, const RegisterConfig &cfg) {
	return StateBackendFactory::createSelection(numQubits, cfg);
}

/**
 * @brief Constructs backend instance for the resolved strategy.
 * @param numQubits Register size.
 * @param cfg Register configuration.
 * @return Backend instance.
 */
inline std::unique_ptr<IStateBackend> createBackend(unsigned int numQubits, const RegisterConfig &cfg) {
	return StateBackendFactory::create(numQubits, cfg);
}

/**
 * @brief Checks whether a strategy is available in the current build.
 * @param kind Strategy to query.
 * @return `true` if available.
 */
inline bool isStrategyAvailable(StorageStrategyKind kind) {
	return StorageStrategyRegistry::isAvailable(kind);
}

/**
 * @brief Returns names of all currently available strategies.
 * @return Lower-case strategy names.
 */
inline std::vector<std::string> listAvailableStrategies() {
	return StorageStrategyRegistry::listAvailable();
}

/**
 * @brief Converts strategy enum to lower-case strategy name.
 * @param kind Strategy enum value.
 * @return Lower-case strategy name.
 */
inline std::string storageStrategyToString(StorageStrategyKind kind) {
	return StorageStrategyRegistry::toString(kind);
}

} // namespace tmfqs

#endif // TMFQS_STORAGE_STATE_BACKEND_FACTORY_H
