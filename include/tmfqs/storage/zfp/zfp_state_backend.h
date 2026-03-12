#ifndef TMFQS_STORAGE_ZFP_ZFP_STATE_BACKEND_H
#define TMFQS_STORAGE_ZFP_ZFP_STATE_BACKEND_H

#include <memory>

#include "tmfqs/config/register_config.h"
#include "tmfqs/storage/i_state_backend.h"

namespace tmfqs {

/**
 * @brief Creates a ZFP-compressed state backend.
 * @param cfg Register configuration including ZFP options.
 * @return Backend instance, or `nullptr` when ZFP is unavailable.
 */
std::unique_ptr<IStateBackend> createZfpStateBackend(const RegisterConfig &cfg);
/**
 * @brief Indicates whether ZFP backend support is available.
 * @return `true` if ZFP support is compiled in.
 */
bool isZfpStateBackendAvailable();

} // namespace tmfqs

#endif // TMFQS_STORAGE_ZFP_ZFP_STATE_BACKEND_H
