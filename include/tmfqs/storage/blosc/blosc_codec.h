#ifndef TMFQS_STORAGE_BLOSC_BLOSC_CODEC_H
#define TMFQS_STORAGE_BLOSC_BLOSC_CODEC_H

#include <cstddef>
#include <memory>
#include <vector>

#include "tmfqs/config/register_config.h"

namespace tmfqs {
namespace storage {

/**
 * @brief Thin Blosc2 wrapper for chunked compressed amplitude storage.
 *
 * This class owns the Blosc super-chunk and exposes typed helper methods used
 * by the Blosc state backend.
 */
class BloscCodec {
	public:
		/**
		 * @brief Constructs a codec using register configuration values.
		 * @param cfg Register configuration (Blosc fields are validated).
		 */
		explicit BloscCodec(const RegisterConfig &cfg);
		/** @brief Copy constructor. */
		BloscCodec(const BloscCodec &other);
		/** @brief Copy assignment operator. */
		BloscCodec &operator=(const BloscCodec &other);
		/** @brief Destructor. */
		~BloscCodec();

		/**
		 * @brief Validates Blosc-related configuration values.
		 * @param cfg Register configuration to validate.
		 */
		static void validateConfig(const RegisterConfig &cfg);
		/**
		 * @brief Indicates whether Blosc2 support is compiled in.
		 * @return `true` when Blosc-backed features are available.
		 */
		static bool available();

		/** @brief Reinitializes internal compressed storage to an empty state. */
		void resetStorage();
		/**
		 * @brief Compresses and appends one chunk.
		 * @param data Pointer to uncompressed chunk data.
		 * @param elems Number of `double` elements in `data`.
		 * @param context Context string used for diagnostics.
		 */
		void appendChunk(const double *data, size_t elems, const char *context);
		/**
		 * @brief Decompresses one stored chunk.
		 * @param chunkIndex Chunk index.
		 * @param buffer Output buffer.
		 * @param expectedElems Expected number of `double` elements.
		 */
		void readChunk(size_t chunkIndex, std::vector<double> &buffer, size_t expectedElems) const;
		/**
		 * @brief Recompresses and replaces one chunk.
		 * @param chunkIndex Chunk index.
		 * @param buffer Uncompressed source values.
		 * @param elems Number of `double` elements to encode.
		 */
		void writeChunk(size_t chunkIndex, const std::vector<double> &buffer, size_t elems);

		/**
		 * @brief Returns number of chunks currently stored.
		 * @return Chunk count.
		 */
		size_t chunkCount() const;
		/**
		 * @brief Returns whether internal storage has been initialized.
		 * @return `true` when compressed storage exists.
		 */
		bool hasStorage() const;

	private:
		struct Impl;
		/** @brief Full register configuration used for codec behavior. */
		RegisterConfig cfg_;
		/** @brief Pimpl storing Blosc-specific runtime resources. */
		std::unique_ptr<Impl> impl_;

		/**
		 * @brief Ensures storage is initialized before use.
		 * @param context Context string used for diagnostics.
		 */
		void ensureStorage(const char *context) const;
		/**
		 * @brief Deep-copies compressed chunks from another codec.
		 * @param other Source codec.
		 */
		void cloneFrom(const BloscCodec &other);
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_BLOSC_BLOSC_CODEC_H
