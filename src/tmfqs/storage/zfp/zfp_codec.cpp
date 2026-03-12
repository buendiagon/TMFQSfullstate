#include "tmfqs/storage/zfp/zfp_codec.h"

#include <cmath>
#include <stdexcept>

#ifdef HAVE_ZFP
#include <memory>

#include <zfp.h>
#endif

namespace tmfqs {
namespace storage {

void ZfpCodec::validateConfig(const RegisterConfig &cfg) {
	if(!std::isfinite(cfg.zfp.rate) || cfg.zfp.rate <= 0.0) {
		throw std::invalid_argument("ZfpCodec: rate must be a finite value > 0");
	}
	if(cfg.zfp.precision == 0u || cfg.zfp.precision > 64u) {
		throw std::invalid_argument("ZfpCodec: precision must be in [1, 64]");
	}
	if(!std::isfinite(cfg.zfp.accuracy) || cfg.zfp.accuracy <= 0.0) {
		throw std::invalid_argument("ZfpCodec: accuracy must be a finite value > 0");
	}
	if(cfg.zfp.chunkStates == 0u) {
		throw std::invalid_argument("ZfpCodec: chunkStates must be >= 1");
	}
	if(cfg.zfp.gateCacheSlots < 2u) {
		throw std::invalid_argument("ZfpCodec: gateCacheSlots must be >= 2");
	}
}

ZfpCodec::ZfpCodec(const RegisterConfig &cfg) : cfg_(cfg) {
	validateConfig(cfg_);
}

void ZfpCodec::compress(const double *data, size_t elemCount, std::vector<uint8_t> &out) {
#ifndef HAVE_ZFP
	(void)data;
	(void)elemCount;
	(void)out;
	throw std::runtime_error("ZfpCodec: zfp support is not available in this build");
#else
	if(elemCount == 0u) {
		throw std::invalid_argument("ZfpCodec: cannot compress empty buffer");
	}

	struct FieldDeleter {
		void operator()(zfp_field *field) const noexcept {
			if(field) zfp_field_free(field);
		}
	};
	struct StreamDeleter {
		void operator()(zfp_stream *stream) const noexcept {
			if(stream) zfp_stream_close(stream);
		}
	};
	struct BitstreamDeleter {
		void operator()(bitstream *stream) const noexcept {
			if(stream) stream_close(stream);
		}
	};

	using FieldPtr = std::unique_ptr<zfp_field, FieldDeleter>;
	using StreamPtr = std::unique_ptr<zfp_stream, StreamDeleter>;
	using BitstreamPtr = std::unique_ptr<bitstream, BitstreamDeleter>;

	FieldPtr field(zfp_field_1d(const_cast<double *>(data), zfp_type_double, elemCount));
	if(!field) {
		throw std::runtime_error("ZfpCodec: failed creating zfp field");
	}

	StreamPtr stream(zfp_stream_open(nullptr));
	if(!stream) {
		throw std::runtime_error("ZfpCodec: failed creating zfp stream");
	}

	switch(cfg_.zfp.mode) {
		case ZfpCompressionMode::FixedRate:
			if(zfp_stream_set_rate(stream.get(), cfg_.zfp.rate, zfp_type_double, 1u, 0u) <= 0.0) {
				throw std::runtime_error("ZfpCodec: failed to configure fixed-rate mode");
			}
			break;
		case ZfpCompressionMode::FixedPrecision:
			if(zfp_stream_set_precision(stream.get(), cfg_.zfp.precision) == 0u) {
				throw std::runtime_error("ZfpCodec: failed to configure fixed-precision mode");
			}
			break;
		case ZfpCompressionMode::FixedAccuracy:
			if(zfp_stream_set_accuracy(stream.get(), cfg_.zfp.accuracy) <= 0.0) {
				throw std::runtime_error("ZfpCodec: failed to configure fixed-accuracy mode");
			}
			break;
	}

	const size_t maxSize = zfp_stream_maximum_size(stream.get(), field.get());
	if(maxSize == 0u) {
		throw std::runtime_error("ZfpCodec: invalid maximum compressed size");
	}
	if(compressionScratch_.size() < maxSize) {
		compressionScratch_.resize(maxSize);
	}

	BitstreamPtr bitstream(stream_open(compressionScratch_.data(), maxSize));
	if(!bitstream) {
		throw std::runtime_error("ZfpCodec: failed creating bitstream");
	}

	zfp_stream_set_bit_stream(stream.get(), bitstream.get());
	zfp_stream_rewind(stream.get());
	const size_t compressedSize = zfp_compress(stream.get(), field.get());
	if(compressedSize == 0u) {
		throw std::runtime_error("ZfpCodec: compression failed");
	}
	out.assign(compressionScratch_.begin(), compressionScratch_.begin() + compressedSize);
#endif
}

void ZfpCodec::decompress(const std::vector<uint8_t> &compressed, size_t elemCount, std::vector<double> &out) const {
#ifndef HAVE_ZFP
	(void)compressed;
	(void)elemCount;
	(void)out;
	throw std::runtime_error("ZfpCodec: zfp support is not available in this build");
#else
	if(elemCount == 0u || compressed.empty()) {
		throw std::invalid_argument("ZfpCodec: invalid chunk while decompressing");
	}

	if(out.size() != elemCount) {
		out.resize(elemCount);
	}

	struct FieldDeleter {
		void operator()(zfp_field *field) const noexcept {
			if(field) zfp_field_free(field);
		}
	};
	struct StreamDeleter {
		void operator()(zfp_stream *stream) const noexcept {
			if(stream) zfp_stream_close(stream);
		}
	};
	struct BitstreamDeleter {
		void operator()(bitstream *stream) const noexcept {
			if(stream) stream_close(stream);
		}
	};

	using FieldPtr = std::unique_ptr<zfp_field, FieldDeleter>;
	using StreamPtr = std::unique_ptr<zfp_stream, StreamDeleter>;
	using BitstreamPtr = std::unique_ptr<bitstream, BitstreamDeleter>;

	FieldPtr field(zfp_field_1d(out.data(), zfp_type_double, elemCount));
	if(!field) {
		throw std::runtime_error("ZfpCodec: failed creating zfp field");
	}

	StreamPtr stream(zfp_stream_open(nullptr));
	if(!stream) {
		throw std::runtime_error("ZfpCodec: failed creating zfp stream");
	}
	switch(cfg_.zfp.mode) {
		case ZfpCompressionMode::FixedRate:
			if(zfp_stream_set_rate(stream.get(), cfg_.zfp.rate, zfp_type_double, 1u, 0u) <= 0.0) {
				throw std::runtime_error("ZfpCodec: failed to configure fixed-rate mode");
			}
			break;
		case ZfpCompressionMode::FixedPrecision:
			if(zfp_stream_set_precision(stream.get(), cfg_.zfp.precision) == 0u) {
				throw std::runtime_error("ZfpCodec: failed to configure fixed-precision mode");
			}
			break;
		case ZfpCompressionMode::FixedAccuracy:
			if(zfp_stream_set_accuracy(stream.get(), cfg_.zfp.accuracy) <= 0.0) {
				throw std::runtime_error("ZfpCodec: failed to configure fixed-accuracy mode");
			}
			break;
	}

	BitstreamPtr bitstream(stream_open(const_cast<uint8_t *>(compressed.data()), compressed.size()));
	if(!bitstream) {
		throw std::runtime_error("ZfpCodec: failed creating bitstream");
	}
	zfp_stream_set_bit_stream(stream.get(), bitstream.get());
	zfp_stream_rewind(stream.get());
	if(zfp_decompress(stream.get(), field.get()) == 0) {
		throw std::runtime_error("ZfpCodec: decompression failed");
	}
#endif
}

} // namespace storage
} // namespace tmfqs
