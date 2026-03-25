#include "tmfqs/storage/zfp/zfp_codec.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>

#ifdef HAVE_ZFP
#include <memory>

#include <zfp.h>
#endif

namespace tmfqs {
namespace storage {

struct ZfpCodec::Impl {
#ifdef HAVE_ZFP
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

	mutable FieldPtr compressionField;
	mutable FieldPtr decompressionField;
	mutable StreamPtr compressionStream;
	mutable StreamPtr decompressionStream;
	mutable BitstreamPtr compressionBitstream;
	mutable size_t compressionBitstreamCapacity = 0u;
#endif
};

#ifdef HAVE_ZFP
namespace {

struct FieldShape {
	size_t nx = 0u;
	size_t ny = 0u;
	bool is2d() const noexcept { return nx != 0u && ny != 0u; }
	uint dimensions() const noexcept { return is2d() ? 2u : 1u; }
};

FieldShape chooseFieldShape(size_t elemCount) {
	if(elemCount < 64u) {
		return {};
	}
	size_t side = static_cast<size_t>(std::sqrt(static_cast<double>(elemCount)));
	for(size_t ny = side; ny >= 8u; --ny) {
		if(elemCount % ny != 0u) {
			continue;
		}
		const size_t nx = elemCount / ny;
		if(nx >= 8u) {
			return {nx, ny};
		}
	}
	return {};
}

void applyStreamMode(zfp_stream *stream, const RegisterConfig &cfg, int executionThreads, uint dimensions) {
	if(executionThreads > 1) {
		if(!zfp_stream_set_omp_threads(stream, static_cast<uint>(executionThreads))) {
			throw std::runtime_error("ZfpCodec: failed to configure OpenMP execution");
		}
	} else if(!zfp_stream_set_execution(stream, zfp_exec_serial)) {
		throw std::runtime_error("ZfpCodec: failed to configure serial execution");
	}

	switch(cfg.zfp.mode) {
		case ZfpCompressionMode::FixedRate:
			if(zfp_stream_set_rate(stream, cfg.zfp.rate, zfp_type_double, dimensions, 0u) <= 0.0) {
				throw std::runtime_error("ZfpCodec: failed to configure fixed-rate mode");
			}
			break;
		case ZfpCompressionMode::FixedPrecision:
			if(zfp_stream_set_precision(stream, cfg.zfp.precision) == 0u) {
				throw std::runtime_error("ZfpCodec: failed to configure fixed-precision mode");
			}
			break;
		case ZfpCompressionMode::FixedAccuracy:
			if(zfp_stream_set_accuracy(stream, cfg.zfp.accuracy) <= 0.0) {
				throw std::runtime_error("ZfpCodec: failed to configure fixed-accuracy mode");
			}
			break;
	}
}

FieldShape prepareField(ZfpCodec::Impl::FieldPtr &field, void *data, size_t elemCount) {
	const FieldShape shape = chooseFieldShape(elemCount);
	if(shape.is2d()) {
		field.reset(zfp_field_2d(data, zfp_type_double, shape.nx, shape.ny));
	} else {
		field.reset(zfp_field_1d(data, zfp_type_double, elemCount));
	}
	if(!field) {
		throw std::runtime_error("ZfpCodec: failed creating zfp field");
	}
	return shape;
}

void ensureCompressionBitstream(
	ZfpCodec::Impl::BitstreamPtr &bitstream,
	size_t &capacityBytes,
	uint8_t *buffer,
	size_t bufferBytes) {
	if(!bitstream || capacityBytes != bufferBytes) {
		bitstream.reset(stream_open(buffer, bufferBytes));
		if(!bitstream) {
			throw std::runtime_error("ZfpCodec: failed creating bitstream");
		}
		capacityBytes = bufferBytes;
		return;
	}

	stream_rewind(bitstream.get());
}

} // namespace
#endif

namespace {

void resetCodecStreams(ZfpCodec::Impl &impl) {
#ifdef HAVE_ZFP
	impl.compressionField.reset();
	impl.compressionStream.reset();
	impl.compressionBitstream.reset();
	impl.compressionBitstreamCapacity = 0u;
#else
	(void)impl;
#endif
}

void resetDecompressionStream(ZfpCodec::Impl &impl) {
#ifdef HAVE_ZFP
	impl.decompressionField.reset();
	impl.decompressionStream.reset();
#else
	(void)impl;
#endif
}

} // namespace

/** @brief Validates ZFP-related configuration fields. */
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
	if(cfg.zfp.nthreads <= 0) {
		throw std::invalid_argument("ZfpCodec: nthreads must be >= 1");
	}
	if(cfg.zfp.gateCacheSlots < 2u) {
		throw std::invalid_argument("ZfpCodec: gateCacheSlots must be >= 2");
	}
}

/** @brief Constructs codec and validates runtime configuration. */
ZfpCodec::ZfpCodec(const RegisterConfig &cfg)
	: cfg_(cfg),
	  compressionThreads_(cfg.zfp.nthreads),
	  decompressionThreads_(cfg.zfp.nthreads),
	  impl_(std::make_unique<Impl>()) {
	validateConfig(cfg_);
}

ZfpCodec::ZfpCodec(const ZfpCodec &other)
	: cfg_(other.cfg_),
	  compressionThreads_(other.compressionThreads_),
	  decompressionThreads_(other.decompressionThreads_),
	  compressionScratch_(other.compressionScratch_),
	  impl_(std::make_unique<Impl>()) {
	validateConfig(cfg_);
}

ZfpCodec &ZfpCodec::operator=(const ZfpCodec &other) {
	if(this == &other) {
		return *this;
	}
	cfg_ = other.cfg_;
	compressionThreads_ = other.compressionThreads_;
	decompressionThreads_ = other.decompressionThreads_;
	compressionScratch_ = other.compressionScratch_;
	impl_ = std::make_unique<Impl>();
	validateConfig(cfg_);
	return *this;
}

ZfpCodec::~ZfpCodec() = default;

/** @brief Compresses one dense amplitude chunk into a byte buffer. */
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
	if(!impl_) {
		impl_ = std::make_unique<Impl>();
	}

	const FieldShape shape = prepareField(impl_->compressionField, const_cast<double *>(data), elemCount);

	if(!impl_->compressionStream) {
		impl_->compressionStream.reset(zfp_stream_open(nullptr));
		if(!impl_->compressionStream) {
			throw std::runtime_error("ZfpCodec: failed creating zfp stream");
		}
	}
	applyStreamMode(impl_->compressionStream.get(), cfg_, compressionThreads_, shape.dimensions());

	const size_t maxSize = zfp_stream_maximum_size(impl_->compressionStream.get(), impl_->compressionField.get());
	if(maxSize == 0u) {
		throw std::runtime_error("ZfpCodec: invalid maximum compressed size");
	}
	if(compressionScratch_.size() < maxSize) {
		compressionScratch_.resize(maxSize);
	}

	ensureCompressionBitstream(
		impl_->compressionBitstream,
		impl_->compressionBitstreamCapacity,
		compressionScratch_.data(),
		compressionScratch_.size());
	zfp_stream_set_bit_stream(impl_->compressionStream.get(), impl_->compressionBitstream.get());
	zfp_stream_rewind(impl_->compressionStream.get());
	/** @brief `zfp_compress` returns 0 when compression fails. */
	size_t compressedSize = zfp_compress(impl_->compressionStream.get(), impl_->compressionField.get());
	if(compressedSize == 0u && compressionThreads_ > 1) {
		compressionThreads_ = 1;
		resetCodecStreams(*impl_);
		compress(data, elemCount, out);
		return;
	}
	if(compressedSize == 0u) {
		throw std::runtime_error("ZfpCodec: compression failed");
	}
	out.assign(compressionScratch_.begin(), compressionScratch_.begin() + compressedSize);
#endif
}

/** @brief Decompresses one chunk into a dense `double` buffer. */
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
	if(!impl_) {
		impl_ = std::make_unique<Impl>();
	}

	const FieldShape shape = prepareField(impl_->decompressionField, out.data(), elemCount);

	if(!impl_->decompressionStream) {
		impl_->decompressionStream.reset(zfp_stream_open(nullptr));
		if(!impl_->decompressionStream) {
			throw std::runtime_error("ZfpCodec: failed creating zfp stream");
		}
	}
	applyStreamMode(impl_->decompressionStream.get(), cfg_, decompressionThreads_, shape.dimensions());

	Impl::BitstreamPtr bitstream(stream_open(const_cast<uint8_t *>(compressed.data()), compressed.size()));
	if(!bitstream) {
		throw std::runtime_error("ZfpCodec: failed creating bitstream");
	}
	zfp_stream_set_bit_stream(impl_->decompressionStream.get(), bitstream.get());
	zfp_stream_rewind(impl_->decompressionStream.get());
	const size_t decompressedSize = zfp_decompress(impl_->decompressionStream.get(), impl_->decompressionField.get());
	if(decompressedSize == 0u && decompressionThreads_ > 1) {
		decompressionThreads_ = 1;
		resetDecompressionStream(*impl_);
		decompress(compressed, elemCount, out);
		return;
	}
	if(decompressedSize == 0u) {
		throw std::runtime_error("ZfpCodec: decompression failed");
	}
#endif
}

} // namespace storage
} // namespace tmfqs
