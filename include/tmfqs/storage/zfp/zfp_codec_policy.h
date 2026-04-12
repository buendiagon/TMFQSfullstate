#ifndef TMFQS_STORAGE_ZFP_ZFP_CODEC_POLICY_H
#define TMFQS_STORAGE_ZFP_ZFP_CODEC_POLICY_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tmfqs/config/register_config.h"
#include "tmfqs/storage/zfp/zfp_codec.h"

namespace tmfqs {
namespace storage {

class ZfpCodecPolicy {
	public:
		explicit ZfpCodecPolicy(const RegisterConfig &cfg) : codec_(cfg) {}

		static bool available();
		static const char *backendName() { return "ZfpStateBackend"; }
		static const char *metricsName() { return "zfp"; }
		static size_t chunkStates(const RegisterConfig &cfg) { return cfg.zfp.chunkStates; }
		static size_t cacheSlots(const RegisterConfig &cfg) { return cfg.zfp.gateCacheSlots; }

		void compress(const double *data, size_t elemCount, std::vector<uint8_t> &out) {
			codec_.compress(data, elemCount, out);
		}

		void decompress(const std::vector<uint8_t> &compressed, size_t elemCount, std::vector<double> &out) const {
			codec_.decompress(compressed, elemCount, out);
		}

	private:
		ZfpCodec codec_;
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_ZFP_ZFP_CODEC_POLICY_H
