#ifndef STATE_BACKEND_FACTORY_INCLUDE
#define STATE_BACKEND_FACTORY_INCLUDE

#include <memory>
#include <string>
#include <vector>
#include "types.h"
#include "storage/IStateBackend.h"

std::unique_ptr<IStateBackend> createBackend(unsigned int numQubits, const RegisterConfig &cfg);
bool isStrategyAvailable(StorageStrategyKind kind);
std::vector<std::string> listAvailableStrategies();
std::string storageStrategyToString(StorageStrategyKind kind);

#endif // STATE_BACKEND_FACTORY_INCLUDE
