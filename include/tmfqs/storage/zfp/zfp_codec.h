#ifndef TMFQS_STORAGE_ZFP_ZFP_CODEC_H
#define TMFQS_STORAGE_ZFP_ZFP_CODEC_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tmfqs/config/register_config.h"

namespace tmfqs {
namespace storage {

class ZfpCodec {
	public:
		explicit ZfpCodec(const RegisterConfig &cfg);

		static void validateConfig(const RegisterConfig &cfg);

		void compress(const double *data, size_t elemCount, std::vector<uint8_t> &out);
		void decompress(const std::vector<uint8_t> &compressed, size_t elemCount, std::vector<double> &out) const;

	private:
		RegisterConfig cfg_;
		mutable std::vector<uint8_t> compressionScratch_;
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_ZFP_ZFP_CODEC_H
