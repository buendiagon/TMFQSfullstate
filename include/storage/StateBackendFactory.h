#ifndef STATE_BACKEND_FACTORY_INCLUDE
#define STATE_BACKEND_FACTORY_INCLUDE

#include <memory>
#include <string>
#include <vector>
#include "types.h"
#include "storage/IStateBackend.h"

struct BackendSelection {
	StorageStrategyKind strategy = StorageStrategyKind::Dense;
	std::unique_ptr<IStateBackend> backend;
};

// Resolves Auto strategy to a concrete backend kind for the given register size.
StorageStrategyKind resolveStorageStrategy(unsigned int numQubits, const RegisterConfig &cfg);
// Resolves strategy and returns both the concrete strategy and backend instance.
BackendSelection createBackendSelection(unsigned int numQubits, const RegisterConfig &cfg);
// Builds a backend according to RegisterConfig (backend object is not state-initialized).
std::unique_ptr<IStateBackend> createBackend(unsigned int numQubits, const RegisterConfig &cfg);
// Returns true when the requested strategy is compiled/registered in this build.
bool isStrategyAvailable(StorageStrategyKind kind);
// Human-readable names of all strategies currently available at runtime.
std::vector<std::string> listAvailableStrategies();
// Converts enum value to a stable string identifier.
std::string storageStrategyToString(StorageStrategyKind kind);

#endif // STATE_BACKEND_FACTORY_INCLUDE
