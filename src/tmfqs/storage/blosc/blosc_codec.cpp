#include "tmfqs/storage/blosc/blosc_codec.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

#ifdef HAVE_BLOSC2
#include <blosc2.h>
#endif

namespace tmfqs {
namespace storage {

/** @brief Internal Blosc runtime state and RAII wrappers (pimpl payload). */
struct BloscCodec::Impl {
#ifdef HAVE_BLOSC2
	/** @brief Initializes/finalizes the global Blosc runtime once per process. */
	struct BloscRuntimeGuard {
		BloscRuntimeGuard() { blosc2_init(); }
		~BloscRuntimeGuard() { blosc2_destroy(); }
	};

	struct BloscContextDeleter {
		void operator()(blosc2_context *ctx) const noexcept {
			if(ctx) blosc2_free_ctx(ctx);
		}
	};

	struct BloscSchunkDeleter {
		void operator()(blosc2_schunk *schunk) const noexcept {
			if(schunk) blosc2_schunk_free(schunk);
		}
	};

	struct ChunkGuard {
		uint8_t *data = nullptr;
		bool needsFree = false;
		~ChunkGuard() {
			if(needsFree && data) {
				free(data);
			}
		}
	};

	using BloscContextPtr = std::unique_ptr<blosc2_context, BloscContextDeleter>;
	using BloscSchunkPtr = std::unique_ptr<blosc2_schunk, BloscSchunkDeleter>;

	BloscSchunkPtr schunk;
	blosc2_cparams schunkCParams = BLOSC2_CPARAMS_DEFAULTS;
	BloscContextPtr compressionCtx;
	std::vector<uint8_t> compressionScratch;
#endif
};

/** @brief Constructs codec and validates Blosc configuration. */
BloscCodec::BloscCodec(const RegisterConfig &cfg)
	: cfg_(cfg), impl_(std::make_unique<Impl>()) {
	validateConfig(cfg_);
#ifdef HAVE_BLOSC2
	static Impl::BloscRuntimeGuard runtimeGuard;
	(void)runtimeGuard;
#endif
}

/** @brief Copy constructor that clones compressed storage. */
BloscCodec::BloscCodec(const BloscCodec &other)
	: cfg_(other.cfg_), impl_(std::make_unique<Impl>()) {
	validateConfig(cfg_);
#ifdef HAVE_BLOSC2
	static Impl::BloscRuntimeGuard runtimeGuard;
	(void)runtimeGuard;
#endif
	cloneFrom(other);
}

/** @brief Copy assignment that deep-copies compressed storage. */
BloscCodec &BloscCodec::operator=(const BloscCodec &other) {
	if(this == &other) {
		return *this;
	}
	cfg_ = other.cfg_;
	validateConfig(cfg_);
	if(!impl_) {
		impl_ = std::make_unique<Impl>();
	}
	cloneFrom(other);
	return *this;
}

/** @brief Default destructor. */
BloscCodec::~BloscCodec() = default;

/** @brief Validates Blosc-related configuration fields. */
void BloscCodec::validateConfig(const RegisterConfig &cfg) {
	if(cfg.blosc.chunkStates == 0u) {
		throw std::invalid_argument("BloscCodec: chunkStates must be >= 1");
	}
	if(cfg.blosc.clevel < 0 || cfg.blosc.clevel > 9) {
		throw std::invalid_argument("BloscCodec: clevel must be in [0, 9]");
	}
	if(cfg.blosc.nthreads <= 0) {
		throw std::invalid_argument("BloscCodec: nthreads must be >= 1");
	}
	if(cfg.blosc.gateCacheSlots < 2u) {
		throw std::invalid_argument("BloscCodec: gateCacheSlots must be >= 2");
	}
	const size_t maxElemsPerChunk = static_cast<size_t>(std::numeric_limits<int32_t>::max()) / sizeof(double);
	if(cfg.blosc.chunkStates * 2u > maxElemsPerChunk) {
		throw std::invalid_argument("BloscCodec: chunk payload exceeds int32 byte limits");
	}
}

/** @brief Returns whether Blosc2 support is compiled in. */
bool BloscCodec::available() {
#ifdef HAVE_BLOSC2
	return true;
#else
	return false;
#endif
}

/** @brief Recreates empty compressed storage with configured parameters. */
void BloscCodec::resetStorage() {
#ifdef HAVE_BLOSC2
	/** @brief Reset creates a new empty super-chunk using configured compression parameters. */
	blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
	impl_->schunkCParams = BLOSC2_CPARAMS_DEFAULTS;
	impl_->schunkCParams.compcode = static_cast<uint8_t>(cfg_.blosc.compcode);
	impl_->schunkCParams.clevel = cfg_.blosc.clevel;
	impl_->schunkCParams.nthreads = cfg_.blosc.nthreads;
	impl_->schunkCParams.typesize = sizeof(double);
	impl_->schunkCParams.filters[BLOSC2_MAX_FILTERS - 1] = cfg_.blosc.useShuffle ? BLOSC_SHUFFLE : BLOSC_NOFILTER;
	storage.cparams = &impl_->schunkCParams;
	blosc2_schunk *raw = blosc2_schunk_new(&storage);
	if(!raw) {
		throw std::runtime_error("BloscCodec: failed creating super-chunk");
	}
	impl_->schunk.reset(raw);
	impl_->compressionCtx.reset();
	impl_->compressionScratch.clear();
#else
	throw std::runtime_error("BloscCodec: blosc support is not available in this build");
#endif
}

/** @brief Compresses and appends one chunk to the super-chunk. */
void BloscCodec::appendChunk(const double *data, size_t elems, const char *context) {
#ifdef HAVE_BLOSC2
	ensureStorage(context);
	const size_t bytes = elems * sizeof(double);
	if(bytes > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
		throw std::overflow_error(std::string("BloscCodec: byte overflow in ") + context);
	}
	const int64_t rc = blosc2_schunk_append_buffer(
		impl_->schunk.get(),
		data,
		static_cast<int32_t>(bytes));
	if(rc < 0) {
		throw std::runtime_error(std::string("BloscCodec: append chunk failed in ") + context);
	}
#else
	(void)data;
	(void)elems;
	(void)context;
	throw std::runtime_error("BloscCodec: blosc support is not available in this build");
#endif
}

/** @brief Decompresses one chunk into a dense buffer. */
void BloscCodec::readChunk(size_t chunkIndex, std::vector<double> &buffer, size_t expectedElems) const {
#ifdef HAVE_BLOSC2
	ensureStorage("chunk read");
	if(chunkIndex >= static_cast<size_t>(impl_->schunk->nchunks)) {
		throw std::out_of_range("BloscCodec: chunk index out of range");
	}
	if(buffer.size() < expectedElems) {
		buffer.resize(expectedElems, 0.0);
	}
	const size_t bytes = expectedElems * sizeof(double);
	if(bytes > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
		throw std::overflow_error("BloscCodec: read size overflow");
	}
	const int32_t destBytes = static_cast<int32_t>(bytes);
	const int dsz = blosc2_schunk_decompress_chunk(
		impl_->schunk.get(),
		static_cast<int64_t>(chunkIndex),
		buffer.data(),
		destBytes);
	if(dsz < 0) {
		throw std::runtime_error("BloscCodec: chunk decompress failed");
	}
	if(dsz != destBytes) {
		throw std::runtime_error("BloscCodec: chunk decompress size mismatch");
	}
#else
	(void)chunkIndex;
	(void)buffer;
	(void)expectedElems;
	throw std::runtime_error("BloscCodec: blosc support is not available in this build");
#endif
}

/** @brief Recompresses and replaces one existing chunk. */
void BloscCodec::writeChunk(size_t chunkIndex, const std::vector<double> &buffer, size_t elems) {
#ifdef HAVE_BLOSC2
	ensureStorage("chunk write");
	if(chunkIndex >= static_cast<size_t>(impl_->schunk->nchunks)) {
		throw std::out_of_range("BloscCodec: chunk index out of range");
	}
	if(elems == 0u) {
		throw std::invalid_argument("BloscCodec: chunk write expects at least one element");
	}

	if(!impl_->compressionCtx) {
		/** @brief Reuse a dedicated compression context for repeated updates. */
		blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
		cparams.compcode = static_cast<uint8_t>(cfg_.blosc.compcode);
		cparams.clevel = cfg_.blosc.clevel;
		cparams.nthreads = cfg_.blosc.nthreads;
		cparams.typesize = sizeof(double);
		cparams.filters[BLOSC2_MAX_FILTERS - 1] = cfg_.blosc.useShuffle ? BLOSC_SHUFFLE : BLOSC_NOFILTER;
		impl_->compressionCtx = Impl::BloscContextPtr(blosc2_create_cctx(cparams));
		if(!impl_->compressionCtx) {
			throw std::runtime_error("BloscCodec: failed creating compression context");
		}
	}

	const size_t srcBytes = elems * sizeof(double);
	const size_t required = srcBytes + BLOSC2_MAX_OVERHEAD;
	if(impl_->compressionScratch.size() < required) {
		impl_->compressionScratch.resize(required);
	}
	if(srcBytes > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
		throw std::overflow_error("BloscCodec: source bytes overflow");
	}
	if(impl_->compressionScratch.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
		throw std::overflow_error("BloscCodec: scratch bytes overflow");
	}

	const int csz = blosc2_compress_ctx(
		impl_->compressionCtx.get(),
		buffer.data(),
		static_cast<int32_t>(srcBytes),
		impl_->compressionScratch.data(),
		static_cast<int32_t>(impl_->compressionScratch.size()));
	if(csz <= 0) {
		throw std::runtime_error("BloscCodec: chunk compress failed");
	}
	const int64_t rc = blosc2_schunk_update_chunk(
		impl_->schunk.get(),
		static_cast<int64_t>(chunkIndex),
		impl_->compressionScratch.data(),
		true);
	if(rc < 0) {
		throw std::runtime_error("BloscCodec: chunk update failed");
	}
#else
	(void)chunkIndex;
	(void)buffer;
	(void)elems;
	throw std::runtime_error("BloscCodec: blosc support is not available in this build");
#endif
}

/** @brief Returns number of chunks currently stored. */
size_t BloscCodec::chunkCount() const {
#ifdef HAVE_BLOSC2
	if(!impl_->schunk) return 0u;
	return static_cast<size_t>(impl_->schunk->nchunks);
#else
	return 0u;
#endif
}

/** @brief Returns whether compressed storage has been initialized. */
bool BloscCodec::hasStorage() const {
#ifdef HAVE_BLOSC2
	return static_cast<bool>(impl_->schunk);
#else
	return false;
#endif
}

/** @brief Validates storage availability before chunk operations. */
void BloscCodec::ensureStorage(const char *context) const {
	if(!hasStorage()) {
		throw std::logic_error(std::string("BloscCodec: storage not initialized for ") + context);
	}
}

/** @brief Deep-copies chunk payloads from another codec instance. */
void BloscCodec::cloneFrom(const BloscCodec &other) {
#ifdef HAVE_BLOSC2
	if(!other.impl_ || !other.impl_->schunk) {
		if(impl_) {
			impl_->schunk.reset();
			impl_->compressionCtx.reset();
			impl_->compressionScratch.clear();
		}
		return;
	}

	resetStorage();
	try {
		/** @brief Clone by copying raw compressed chunks; no decompression round-trip required. */
		for(int64_t ci = 0; ci < other.impl_->schunk->nchunks; ++ci) {
			Impl::ChunkGuard chunk;
			const int cbytes = blosc2_schunk_get_chunk(other.impl_->schunk.get(), ci, &chunk.data, &chunk.needsFree);
			if(cbytes < 0) {
				throw std::runtime_error("BloscCodec: failed reading chunk while cloning");
			}
			const int64_t rc = blosc2_schunk_append_chunk(impl_->schunk.get(), chunk.data, true);
			if(rc < 0) {
				throw std::runtime_error("BloscCodec: failed appending chunk while cloning");
			}
		}
	} catch(...) {
		impl_->schunk.reset();
		throw;
	}
#else
	(void)other;
#endif
}

} // namespace storage
} // namespace tmfqs
