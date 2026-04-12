#ifndef TMFQS_STORAGE_BLOSC_BLOSC_CODEC_POLICY_H
#define TMFQS_STORAGE_BLOSC_BLOSC_CODEC_POLICY_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tmfqs/config/register_config.h"
#include "tmfqs/storage/blosc/blosc_codec.h"

namespace tmfqs {
namespace storage {

class BloscCodecPolicy {
	public:
		explicit BloscCodecPolicy(const RegisterConfig &cfg) : codec_(cfg) {}

		static bool available() { return BloscCodec::available(); }
		static const char *backendName() { return "BloscStateBackend"; }
		static const char *metricsName() { return "blosc"; }
		static size_t chunkStates(const RegisterConfig &cfg) { return cfg.blosc.chunkStates; }
		static size_t cacheSlots(const RegisterConfig &cfg) { return cfg.blosc.gateCacheSlots; }

		void compress(const double *data, size_t elemCount, std::vector<uint8_t> &out) {
			codec_.compress(data, elemCount, out);
		}

		void decompress(const std::vector<uint8_t> &compressed, size_t elemCount, std::vector<double> &out) const {
			codec_.decompress(compressed, elemCount, out);
		}

	private:
		BloscCodec codec_;
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_BLOSC_BLOSC_CODEC_POLICY_H
