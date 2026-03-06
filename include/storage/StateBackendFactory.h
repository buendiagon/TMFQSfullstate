#ifndef STATE_BACKEND_FACTORY_INCLUDE
#define STATE_BACKEND_FACTORY_INCLUDE

#include <memory>
#include <string>
#include <vector>
#include "types.h"
#include "storage/IStateBackend.h"

// Builds a backend according to RegisterConfig and initializes it for numQubits.
std::unique_ptr<IStateBackend> createBackend(unsigned int numQubits, const RegisterConfig &cfg);
// Returns true when the requested strategy is compiled/registered in this build.
bool isStrategyAvailable(StorageStrategyKind kind);
// Human-readable names of all strategies currently available at runtime.
std::vector<std::string> listAvailableStrategies();
// Converts enum value to a stable string identifier.
std::string storageStrategyToString(StorageStrategyKind kind);

#endif // STATE_BACKEND_FACTORY_INCLUDE
