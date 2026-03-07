#ifndef TMFQS_STORAGE_STATE_BACKEND_FACTORY_H
#define TMFQS_STORAGE_STATE_BACKEND_FACTORY_H

#include <memory>
#include <string>
#include <vector>

#include "tmfqs/core/types.h"
#include "tmfqs/storage/i_state_backend.h"

namespace tmfqs {

struct BackendSelection {
	StorageStrategyKind strategy = StorageStrategyKind::Dense;
	std::unique_ptr<IStateBackend> backend;
};

StorageStrategyKind resolveStorageStrategy(unsigned int numQubits, const RegisterConfig &cfg);
BackendSelection createBackendSelection(unsigned int numQubits, const RegisterConfig &cfg);
std::unique_ptr<IStateBackend> createBackend(unsigned int numQubits, const RegisterConfig &cfg);
bool isStrategyAvailable(StorageStrategyKind kind);
std::vector<std::string> listAvailableStrategies();
std::string storageStrategyToString(StorageStrategyKind kind);

} // namespace tmfqs

#endif // TMFQS_STORAGE_STATE_BACKEND_FACTORY_H
