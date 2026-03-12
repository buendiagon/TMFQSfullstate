#include "tmfqs/storage/factory/state_backend_factory.h"

namespace tmfqs {

/** @brief Builds backend instance for the resolved strategy. */
std::unique_ptr<IStateBackend> StateBackendFactory::create(unsigned int numQubits, const RegisterConfig &cfg) {
	// Convenience wrapper when callers only need the backend object.
	return createSelection(numQubits, cfg).backend;
}

/** @brief Builds backend and returns strategy metadata. */
BackendSelection StateBackendFactory::createSelection(unsigned int numQubits, const RegisterConfig &cfg) {
	return StorageStrategyRegistry::createSelection(numQubits, cfg);
}

} // namespace tmfqs
