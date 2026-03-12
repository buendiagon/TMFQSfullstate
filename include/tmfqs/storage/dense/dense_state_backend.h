#ifndef TMFQS_STORAGE_DENSE_DENSE_STATE_BACKEND_H
#define TMFQS_STORAGE_DENSE_DENSE_STATE_BACKEND_H

#include <memory>

#include "tmfqs/config/register_config.h"
#include "tmfqs/storage/i_state_backend.h"

namespace tmfqs {

/**
 * @brief Creates a dense in-memory state backend.
 * @param cfg Register configuration (currently unused for dense backend).
 * @return Backend instance implementing dense storage.
 */
std::unique_ptr<IStateBackend> createDenseStateBackend(const RegisterConfig &cfg);

} // namespace tmfqs

#endif // TMFQS_STORAGE_DENSE_DENSE_STATE_BACKEND_H
