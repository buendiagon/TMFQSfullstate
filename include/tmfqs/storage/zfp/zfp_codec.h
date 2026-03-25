#ifndef TMFQS_STORAGE_ZFP_ZFP_CODEC_H
#define TMFQS_STORAGE_ZFP_ZFP_CODEC_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "tmfqs/config/register_config.h"

namespace tmfqs {
namespace storage {

/**
 * @brief Utility class that performs ZFP compression/decompression on chunks.
 */
class ZfpCodec {
	public:
		struct Impl;

		/**
		 * @brief Constructs a codec and validates ZFP configuration values.
		 * @param cfg Register configuration containing ZFP options.
		 */
		explicit ZfpCodec(const RegisterConfig &cfg);
		ZfpCodec(const ZfpCodec &other);
		ZfpCodec &operator=(const ZfpCodec &other);
		~ZfpCodec();

		/**
		 * @brief Validates ZFP-related configuration values.
		 * @param cfg Register configuration to validate.
		 */
		static void validateConfig(const RegisterConfig &cfg);

		/**
		 * @brief Compresses a chunk of `double` values.
		 * @param data Pointer to uncompressed input values.
		 * @param elemCount Number of `double` values in `data`.
		 * @param out Output buffer receiving compressed bytes.
		 */
		void compress(const double *data, size_t elemCount, std::vector<uint8_t> &out);
		/**
		 * @brief Decompresses a chunk into a dense `double` buffer.
		 * @param compressed Compressed byte payload.
		 * @param elemCount Number of `double` values expected in output.
		 * @param out Output buffer resized/populated with decompressed values.
		 */
		void decompress(const std::vector<uint8_t> &compressed, size_t elemCount, std::vector<double> &out) const;

	private:
		/** @brief Register configuration with ZFP compression parameters. */
		RegisterConfig cfg_;
		/** @brief Reusable temporary output buffer used during compression. */
		mutable std::vector<uint8_t> compressionScratch_;
		/** @brief Lazily initialized reusable ZFP codec state. */
		mutable std::unique_ptr<Impl> impl_;
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_ZFP_ZFP_CODEC_H
