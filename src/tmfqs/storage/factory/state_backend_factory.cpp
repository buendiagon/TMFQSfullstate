#include "tmfqs/storage/factory/state_backend_factory.h"

namespace tmfqs {

std::unique_ptr<IStateBackend> StateBackendFactory::create(unsigned int numQubits, const RegisterConfig &cfg) {
	return createSelection(numQubits, cfg).backend;
}

BackendSelection StateBackendFactory::createSelection(unsigned int numQubits, const RegisterConfig &cfg) {
	return StorageStrategyRegistry::createSelection(numQubits, cfg);
}

} // namespace tmfqs
