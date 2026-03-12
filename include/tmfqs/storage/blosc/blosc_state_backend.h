#ifndef TMFQS_STORAGE_BLOSC_BLOSC_STATE_BACKEND_H
#define TMFQS_STORAGE_BLOSC_BLOSC_STATE_BACKEND_H

#include <memory>

#include "tmfqs/config/register_config.h"
#include "tmfqs/storage/i_state_backend.h"

namespace tmfqs {

/**
 * @brief Creates a Blosc-compressed state backend.
 * @param cfg Register configuration including Blosc options.
 * @return Backend instance, or `nullptr` when Blosc is unavailable.
 */
std::unique_ptr<IStateBackend> createBloscStateBackend(const RegisterConfig &cfg);
/**
 * @brief Indicates whether Blosc backend support is available.
 * @return `true` if Blosc support is compiled in.
 */
bool isBloscStateBackendAvailable();

} // namespace tmfqs

#endif // TMFQS_STORAGE_BLOSC_BLOSC_STATE_BACKEND_H
