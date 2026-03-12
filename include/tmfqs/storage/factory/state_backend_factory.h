#ifndef TMFQS_STORAGE_FACTORY_STATE_BACKEND_FACTORY_H
#define TMFQS_STORAGE_FACTORY_STATE_BACKEND_FACTORY_H

#include <memory>

#include "tmfqs/config/register_config.h"
#include "tmfqs/storage/factory/storage_strategy_registry.h"

namespace tmfqs {

/**
 * @brief Thin facade for backend factory operations.
 */
class StateBackendFactory {
	public:
		/**
		 * @brief Creates a backend for the effective strategy.
		 * @param numQubits Register size.
		 * @param cfg Register configuration.
		 * @return Owning pointer to a backend instance.
		 */
		static std::unique_ptr<IStateBackend> create(unsigned int numQubits, const RegisterConfig &cfg);
		/**
		 * @brief Creates backend and returns the resolved strategy metadata.
		 * @param numQubits Register size.
		 * @param cfg Register configuration.
		 * @return Backend selection object.
		 */
		static BackendSelection createSelection(unsigned int numQubits, const RegisterConfig &cfg);
};

} // namespace tmfqs

#endif // TMFQS_STORAGE_FACTORY_STATE_BACKEND_FACTORY_H
