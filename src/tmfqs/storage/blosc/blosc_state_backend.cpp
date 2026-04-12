#include "tmfqs/storage/blosc/blosc_state_backend.h"

#include <memory>

#include "tmfqs/storage/blosc/blosc_codec_policy.h"
#include "tmfqs/storage/common/compressed_state_backend.h"

namespace tmfqs {

bool isBloscStateBackendAvailable() {
	return storage::BloscCodecPolicy::available();
}

std::unique_ptr<IStateBackend> createBloscStateBackend(const RegisterConfig &cfg) {
#ifdef HAVE_BLOSC2
	return std::make_unique<storage::CompressedStateBackend<storage::BloscCodecPolicy>>(cfg);
#else
	(void)cfg;
	return nullptr;
#endif
}

} // namespace tmfqs
