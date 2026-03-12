#ifndef TMFQS_STORAGE_FACTORY_STATE_BACKEND_FACTORY_H
#define TMFQS_STORAGE_FACTORY_STATE_BACKEND_FACTORY_H

#include <memory>

#include "tmfqs/config/register_config.h"
#include "tmfqs/storage/factory/storage_strategy_registry.h"

namespace tmfqs {

class StateBackendFactory {
	public:
		static std::unique_ptr<IStateBackend> create(unsigned int numQubits, const RegisterConfig &cfg);
		static BackendSelection createSelection(unsigned int numQubits, const RegisterConfig &cfg);
};

} // namespace tmfqs

#endif // TMFQS_STORAGE_FACTORY_STATE_BACKEND_FACTORY_H
