#ifndef TMFQS_STORAGE_FACTORY_STORAGE_STRATEGY_REGISTRY_H
#define TMFQS_STORAGE_FACTORY_STORAGE_STRATEGY_REGISTRY_H

#include <memory>
#include <string>
#include <vector>

#include "tmfqs/config/register_config.h"
#include "tmfqs/storage/i_state_backend.h"

namespace tmfqs {

struct BackendSelection {
	StorageStrategyKind strategy = StorageStrategyKind::Dense;
	std::unique_ptr<IStateBackend> backend;
};

class StorageStrategyRegistry {
	public:
		static StorageStrategyKind resolve(unsigned int numQubits, const RegisterConfig &cfg);
		static bool isAvailable(StorageStrategyKind kind);
		static std::vector<std::string> listAvailable();
		static std::string toString(StorageStrategyKind kind);
		static BackendSelection createSelection(unsigned int numQubits, const RegisterConfig &cfg);
};

} // namespace tmfqs

#endif // TMFQS_STORAGE_FACTORY_STORAGE_STRATEGY_REGISTRY_H
