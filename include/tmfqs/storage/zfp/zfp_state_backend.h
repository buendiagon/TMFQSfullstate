#ifndef TMFQS_STORAGE_ZFP_ZFP_STATE_BACKEND_H
#define TMFQS_STORAGE_ZFP_ZFP_STATE_BACKEND_H

#include <memory>

#include "tmfqs/config/register_config.h"
#include "tmfqs/storage/i_state_backend.h"

namespace tmfqs {

std::unique_ptr<IStateBackend> createZfpStateBackend(const RegisterConfig &cfg);
bool isZfpStateBackendAvailable();

} // namespace tmfqs

#endif // TMFQS_STORAGE_ZFP_ZFP_STATE_BACKEND_H
