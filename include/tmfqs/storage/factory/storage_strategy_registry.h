#ifndef TMFQS_STORAGE_FACTORY_STORAGE_STRATEGY_REGISTRY_H
#define TMFQS_STORAGE_FACTORY_STORAGE_STRATEGY_REGISTRY_H

#include <memory>
#include <string>
#include <vector>

#include "tmfqs/config/register_config.h"
#include "tmfqs/storage/i_state_backend.h"

namespace tmfqs {

/**
 * @brief Result object containing a resolved strategy and its backend instance.
 */
struct BackendSelection {
	/** @brief Selected strategy kind. */
	StorageStrategyKind strategy = StorageStrategyKind::Dense;
	/** @brief Effective backend configuration after auto-tuning. */
	RegisterConfig config;
	/** @brief Backend object implementing the selected strategy. */
	std::unique_ptr<IStateBackend> backend;
};

/**
 * @brief Resolves strategies and exposes backend availability helpers.
 */
class StorageStrategyRegistry {
	public:
		/**
		 * @brief Resolves effective strategy from config and runtime availability.
		 * @param numQubits Register size.
		 * @param cfg Register configuration.
		 * @return Strategy that should be used.
		 */
		static StorageStrategyKind resolve(unsigned int numQubits, const RegisterConfig &cfg);
		/**
		 * @brief Checks whether a strategy is available in the current build.
		 * @param kind Strategy to query.
		 * @return `true` if the strategy can be instantiated.
		 */
		static bool isAvailable(StorageStrategyKind kind);
		/**
		 * @brief Returns names of strategies available in this build.
		 * @return Lower-case strategy names.
		 */
		static std::vector<std::string> listAvailable();
		/**
		 * @brief Converts a strategy enum to a user-facing lower-case string.
		 * @param kind Strategy enum value.
		 * @return Lower-case strategy name.
		 */
		static std::string toString(StorageStrategyKind kind);
		/**
		 * @brief Applies backend-specific auto-tuning while preserving explicit overrides.
		 * @param numQubits Register size.
		 * @param cfg User-supplied configuration.
		 * @param resolvedKind Strategy chosen for the backend.
		 * @return Effective configuration passed to the backend.
		 */
		static RegisterConfig tuneConfig(unsigned int numQubits, const RegisterConfig &cfg, StorageStrategyKind resolvedKind);
		/**
		 * @brief Resolves and constructs a backend selection object.
		 * @param numQubits Register size.
		 * @param cfg Register configuration.
		 * @return Populated selection with strategy and backend instance.
		 */
		static BackendSelection createSelection(unsigned int numQubits, const RegisterConfig &cfg);
};

} // namespace tmfqs

#endif // TMFQS_STORAGE_FACTORY_STORAGE_STRATEGY_REGISTRY_H
