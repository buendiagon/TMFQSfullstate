#include "tmfqs/storage/zfp/zfp_state_backend.h"

#include <memory>

#include "tmfqs/storage/common/compressed_state_backend.h"
#include "tmfqs/storage/zfp/zfp_codec_policy.h"

namespace tmfqs {
namespace storage {

bool ZfpCodecPolicy::available() {
#ifdef HAVE_ZFP
	return true;
#else
	return false;
#endif
}

} // namespace storage

bool isZfpStateBackendAvailable() {
	return storage::ZfpCodecPolicy::available();
}

std::unique_ptr<IStateBackend> createZfpStateBackend(const RegisterConfig &cfg) {
#ifdef HAVE_ZFP
	return std::make_unique<storage::CompressedStateBackend<storage::ZfpCodecPolicy>>(cfg);
#else
	(void)cfg;
	return nullptr;
#endif
}

} // namespace tmfqs
