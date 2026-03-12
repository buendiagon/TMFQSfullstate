#ifndef TMFQS_STORAGE_BLOSC_BLOSC_CODEC_H
#define TMFQS_STORAGE_BLOSC_BLOSC_CODEC_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "tmfqs/config/register_config.h"

namespace tmfqs {
namespace storage {

class BloscCodec {
	public:
		explicit BloscCodec(const RegisterConfig &cfg);
		BloscCodec(const BloscCodec &other);
		BloscCodec &operator=(const BloscCodec &other);
		~BloscCodec();

		static void validateConfig(const RegisterConfig &cfg);
		static bool available();

		void resetStorage();
		void appendChunk(const double *data, size_t elems, const char *context);
		void readChunk(size_t chunkIndex, std::vector<double> &buffer, size_t expectedElems) const;
		void writeChunk(size_t chunkIndex, const std::vector<double> &buffer, size_t elems);

		size_t chunkCount() const;
		bool hasStorage() const;

	private:
		struct Impl;
		RegisterConfig cfg_;
		std::unique_ptr<Impl> impl_;

		void ensureStorage(const char *context) const;
		void cloneFrom(const BloscCodec &other);
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_BLOSC_BLOSC_CODEC_H
