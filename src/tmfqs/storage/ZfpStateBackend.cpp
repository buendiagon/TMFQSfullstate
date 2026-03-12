#include "tmfqs/storage/i_state_backend.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "backend_common.h"
#include "tmfqs/core/state_space.h"

#ifdef HAVE_ZFP
#include <zfp.h>
#endif

namespace tmfqs {

#ifdef HAVE_ZFP
namespace {

struct ZfpFieldDeleter {
	void operator()(zfp_field *field) const noexcept {
		if(field) zfp_field_free(field);
	}
};

struct ZfpStreamDeleter {
	void operator()(zfp_stream *stream) const noexcept {
		if(stream) zfp_stream_close(stream);
	}
};

struct BitstreamDeleter {
	void operator()(bitstream *stream) const noexcept {
		if(stream) stream_close(stream);
	}
};

using ZfpFieldPtr = std::unique_ptr<zfp_field, ZfpFieldDeleter>;
using ZfpStreamPtr = std::unique_ptr<zfp_stream, ZfpStreamDeleter>;
using BitstreamPtr = std::unique_ptr<bitstream, BitstreamDeleter>;

void validateZfpConfig(const RegisterConfig &cfg) {
	if(!std::isfinite(cfg.zfp.rate) || cfg.zfp.rate <= 0.0) {
		throw std::invalid_argument("ZfpStateBackend: rate must be a finite value > 0");
	}
	if(cfg.zfp.precision == 0u || cfg.zfp.precision > 64u) {
		throw std::invalid_argument("ZfpStateBackend: precision must be in [1, 64]");
	}
	if(!std::isfinite(cfg.zfp.accuracy) || cfg.zfp.accuracy <= 0.0) {
		throw std::invalid_argument("ZfpStateBackend: accuracy must be a finite value > 0");
	}
	if(cfg.zfp.chunkStates == 0u) {
		throw std::invalid_argument("ZfpStateBackend: chunkStates must be >= 1");
	}
	if(cfg.zfp.gateCacheSlots == 0u) {
		throw std::invalid_argument("ZfpStateBackend: gateCacheSlots must be >= 1");
	}
}

} // namespace

class ZfpStateBackend : public IStateBackend {
	private:
		static constexpr size_t kInvalidChunk = std::numeric_limits<size_t>::max();

		struct ChunkStorage {
			std::vector<uint8_t> compressed;
			size_t elemCount = 0;
		};

		struct CacheSlot {
			size_t chunkIndex = kInvalidChunk;
			std::vector<double> buffer;
			bool dirty = false;
			uint64_t touch = 0;
		};

		struct CacheAmplitudeRef {
			CacheSlot *slot = nullptr;
			double *amp = nullptr;
		};

		struct ChunkRef {
			size_t chunkIndex = 0;
			size_t elemOffset = 0;
		};

		enum class PairMutation : uint8_t {
			None = 0u,
			First = 1u,
			Second = 2u,
			Both = 3u
		};

		enum class PairKernelMode {
			Fallback,
			IntraChunk,
			InterChunk
		};

		unsigned int numQubits_ = 0;
		unsigned int numStates_ = 0;
		RegisterConfig cfg_;
		std::vector<ChunkStorage> chunks_;
		std::vector<CacheSlot> cache_;
		std::vector<size_t> chunkToSlot_;
		mutable std::vector<uint8_t> compressionScratch_;
		uint64_t touchCounter_ = 0;
		unsigned int batchDepth_ = 0;
		GateBlockWorkspace gateWorkspace_;

		static bool touchesFirst(PairMutation mutation) {
			return mutation == PairMutation::First || mutation == PairMutation::Both;
		}

		static bool touchesSecond(PairMutation mutation) {
			return mutation == PairMutation::Second || mutation == PairMutation::Both;
		}

		void ensureInitialized(const char *operation) const {
			ensureBackendInitialized(numStates_ > 0 && !chunks_.empty(), "ZfpStateBackend", operation);
		}

		size_t chunkStates() const {
			return cfg_.zfp.chunkStates;
		}

		size_t chunkElemsPerFullChunk() const {
			return cfg_.zfp.chunkStates * 2u;
		}

		size_t cacheCapacity() const {
			return cfg_.zfp.gateCacheSlots;
		}

		void configureCompressionStream(zfp_stream *stream) const {
			switch(cfg_.zfp.mode) {
				case ZfpCompressionMode::FixedRate: {
					const double appliedRate = zfp_stream_set_rate(stream, cfg_.zfp.rate, zfp_type_double, 1u, 0u);
					if(appliedRate <= 0.0) {
						throw std::runtime_error("ZfpStateBackend: failed to configure fixed-rate mode");
					}
					break;
				}
				case ZfpCompressionMode::FixedPrecision: {
					const unsigned int appliedPrecision = zfp_stream_set_precision(stream, cfg_.zfp.precision);
					if(appliedPrecision == 0u) {
						throw std::runtime_error("ZfpStateBackend: failed to configure fixed-precision mode");
					}
					break;
				}
				case ZfpCompressionMode::FixedAccuracy: {
					const double appliedAccuracy = zfp_stream_set_accuracy(stream, cfg_.zfp.accuracy);
					if(appliedAccuracy <= 0.0) {
						throw std::runtime_error("ZfpStateBackend: failed to configure fixed-accuracy mode");
					}
					break;
				}
			}
		}

		void compressBuffer(const double *data, size_t elemCount, std::vector<uint8_t> &out) const {
			if(elemCount == 0u) {
				throw std::logic_error("ZfpStateBackend: cannot compress empty buffer");
			}

			ZfpFieldPtr field(zfp_field_1d(const_cast<double *>(data), zfp_type_double, elemCount));
			if(!field) {
				throw std::runtime_error("ZfpStateBackend: failed creating zfp field");
			}

			ZfpStreamPtr stream(zfp_stream_open(nullptr));
			if(!stream) {
				throw std::runtime_error("ZfpStateBackend: failed creating zfp stream");
			}
			configureCompressionStream(stream.get());
			const size_t maxSize = zfp_stream_maximum_size(stream.get(), field.get());
			if(maxSize == 0u) {
				throw std::runtime_error("ZfpStateBackend: invalid maximum compressed size");
			}
			if(compressionScratch_.size() < maxSize) {
				compressionScratch_.resize(maxSize);
			}

			BitstreamPtr bitstream(stream_open(compressionScratch_.data(), maxSize));
			if(!bitstream) {
				throw std::runtime_error("ZfpStateBackend: failed creating bitstream");
			}

			zfp_stream_set_bit_stream(stream.get(), bitstream.get());
			zfp_stream_rewind(stream.get());
			const size_t compressedSize = zfp_compress(stream.get(), field.get());
			if(compressedSize == 0u) {
				throw std::runtime_error("ZfpStateBackend: compression failed");
			}
			out.assign(compressionScratch_.begin(), compressionScratch_.begin() + compressedSize);
		}

		void decompressBuffer(const ChunkStorage &chunk, std::vector<double> &out) const {
			if(chunk.elemCount == 0u || chunk.compressed.empty()) {
				throw std::logic_error("ZfpStateBackend: invalid chunk while decompressing");
			}

			if(out.size() != chunk.elemCount) {
				out.resize(chunk.elemCount);
			}
			ZfpFieldPtr field(zfp_field_1d(out.data(), zfp_type_double, chunk.elemCount));
			if(!field) {
				throw std::runtime_error("ZfpStateBackend: failed creating zfp field");
			}

			ZfpStreamPtr stream(zfp_stream_open(nullptr));
			if(!stream) {
				throw std::runtime_error("ZfpStateBackend: failed creating zfp stream");
			}
			configureCompressionStream(stream.get());

			BitstreamPtr bitstream(stream_open(const_cast<uint8_t *>(chunk.compressed.data()), chunk.compressed.size()));
			if(!bitstream) {
				throw std::runtime_error("ZfpStateBackend: failed creating bitstream");
			}
			zfp_stream_set_bit_stream(stream.get(), bitstream.get());
			zfp_stream_rewind(stream.get());
			if(zfp_decompress(stream.get(), field.get()) == 0) {
				throw std::runtime_error("ZfpStateBackend: decompression failed");
			}
		}

		ChunkRef chunkRef(StateIndex state) const {
			const size_t stateSz = static_cast<size_t>(state);
			ChunkRef ref;
			ref.chunkIndex = stateSz / chunkStates();
			ref.elemOffset = (stateSz % chunkStates()) * 2u;
			return ref;
		}

		const CacheSlot *findCachedSlot(size_t chunkIndex) const {
			if(chunkIndex >= chunkToSlot_.size()) return nullptr;
			const size_t slotIndex = chunkToSlot_[chunkIndex];
			if(slotIndex == kInvalidChunk) return nullptr;
			return &cache_[slotIndex];
		}

		void clearCache() {
			cache_.clear();
			cache_.reserve(cacheCapacity());
			touchCounter_ = 0;
		}

		void writeBackSlot(CacheSlot &slot) {
			if(!slot.dirty || slot.chunkIndex == kInvalidChunk) return;
			ChunkStorage &chunk = chunks_[slot.chunkIndex];
			if(slot.buffer.size() != chunk.elemCount) {
				throw std::logic_error("ZfpStateBackend: cache/chunk size mismatch while flushing");
			}
			compressBuffer(slot.buffer.data(), slot.buffer.size(), chunk.compressed);
			slot.dirty = false;
		}

		size_t selectEvictionSlot() const {
			size_t candidate = 0u;
			uint64_t minTouch = cache_[0].touch;
			for(size_t idx = 1; idx < cache_.size(); ++idx) {
				if(cache_[idx].touch < minTouch) {
					minTouch = cache_[idx].touch;
					candidate = idx;
				}
			}
			return candidate;
		}

			CacheSlot &acquireSlot(size_t chunkIndex) {
			if(chunkIndex >= chunks_.size()) {
				throw std::out_of_range("ZfpStateBackend: chunk index out of range");
			}

			const size_t mappedSlot = chunkToSlot_[chunkIndex];
			if(mappedSlot != kInvalidChunk) {
				CacheSlot &slot = cache_[mappedSlot];
				slot.touch = ++touchCounter_;
				return slot;
			}

			size_t slotIndex = 0u;
			if(cache_.size() < cacheCapacity()) {
				slotIndex = cache_.size();
				cache_.push_back(CacheSlot{});
			} else {
				slotIndex = selectEvictionSlot();
				CacheSlot &evicted = cache_[slotIndex];
				writeBackSlot(evicted);
				if(evicted.chunkIndex != kInvalidChunk) {
					chunkToSlot_[evicted.chunkIndex] = kInvalidChunk;
				}
			}

			CacheSlot &slot = cache_[slotIndex];
			slot.chunkIndex = chunkIndex;
			slot.dirty = false;
			slot.touch = ++touchCounter_;
			decompressBuffer(chunks_[chunkIndex], slot.buffer);
			chunkToSlot_[chunkIndex] = slotIndex;
			return slot;
		}

			CacheAmplitudeRef acquireCacheAmplitude(StateIndex state) {
				const ChunkRef ref = chunkRef(state);
				CacheSlot &slot = acquireSlot(ref.chunkIndex);
				return {&slot, slot.buffer.data() + ref.elemOffset};
			}

			PairKernelMode selectPairKernelMode(unsigned int targetMask) const {
				const size_t statesPerChunk = chunkStates();
				if(statesPerChunk == 0u || (statesPerChunk & (statesPerChunk - 1u)) != 0u) {
					return PairKernelMode::Fallback;
				}
				if(static_cast<size_t>(targetMask) < statesPerChunk) {
					return PairKernelMode::IntraChunk;
				}
				if((static_cast<size_t>(targetMask) % statesPerChunk) == 0u) {
					return PairKernelMode::InterChunk;
				}
				return PairKernelMode::Fallback;
			}

			template <typename PairFn>
			void runPairKernelFallback(unsigned int targetMask, PairFn pairFn) {
				forEachStatePairByMask(numStates_, targetMask, [&](StateIndex state0, StateIndex state1) {
					CacheAmplitudeRef a0Ref = acquireCacheAmplitude(state0);
					CacheAmplitudeRef a1Ref = acquireCacheAmplitude(state1);
					const PairMutation mutation = pairFn(state0, state1, a0Ref.amp, a1Ref.amp);
					if(mutation == PairMutation::None) return;
					if(a0Ref.slot == a1Ref.slot) {
						a0Ref.slot->dirty = true;
						return;
					}
					if(touchesFirst(mutation)) a0Ref.slot->dirty = true;
					if(touchesSecond(mutation)) a1Ref.slot->dirty = true;
				});
			}

			template <typename PairFn>
			void runPairKernelIntraChunk(unsigned int targetMask, PairFn pairFn) {
				const size_t statesPerChunk = chunkStates();
				const size_t localMask = static_cast<size_t>(targetMask);
				const size_t stride = localMask << 1u;
				for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
					CacheSlot &slot = acquireSlot(chunkIndex);
					const size_t statesInChunk = chunks_[chunkIndex].elemCount / 2u;
					const size_t chunkBaseState = chunkIndex * statesPerChunk;
					bool dirty = false;
					for(size_t base = 0; base < statesInChunk; base += stride) {
						for(size_t offset = 0; offset < localMask; ++offset) {
							const size_t local0 = base + offset;
							if(local0 + localMask >= statesInChunk) break;
							const size_t local1 = local0 + localMask;
							double *amp0 = slot.buffer.data() + local0 * 2u;
							double *amp1 = slot.buffer.data() + local1 * 2u;
							const StateIndex state0 = static_cast<StateIndex>(chunkBaseState + local0);
							const StateIndex state1 = static_cast<StateIndex>(chunkBaseState + local1);
							if(pairFn(state0, state1, amp0, amp1) != PairMutation::None) {
								dirty = true;
							}
						}
					}
					if(dirty) {
						slot.dirty = true;
					}
				}
			}

			template <typename PairFn>
			void runPairKernelInterChunk(unsigned int targetMask, PairFn pairFn) {
				const size_t statesPerChunk = chunkStates();
				const size_t chunkDelta = static_cast<size_t>(targetMask) / statesPerChunk;
				const size_t chunkStride = chunkDelta << 1u;
				const size_t chunkCount = chunks_.size();
				for(size_t groupBase = 0; groupBase < chunkCount; groupBase += chunkStride) {
					for(size_t groupOffset = 0; groupOffset < chunkDelta; ++groupOffset) {
						const size_t chunk0 = groupBase + groupOffset;
						const size_t chunk1 = chunk0 + chunkDelta;
						if(chunk1 >= chunkCount) break;

						CacheSlot &slot0 = acquireSlot(chunk0);
						CacheSlot &slot1 = acquireSlot(chunk1);
						const size_t statesInChunk0 = chunks_[chunk0].elemCount / 2u;
						const size_t statesInChunk1 = chunks_[chunk1].elemCount / 2u;
						const size_t statesInPair = std::min(statesInChunk0, statesInChunk1);
						const size_t chunk0BaseState = chunk0 * statesPerChunk;
						const size_t chunk1BaseState = chunk1 * statesPerChunk;
						bool dirty0 = false;
						bool dirty1 = false;
						for(size_t local = 0; local < statesInPair; ++local) {
							double *amp0 = slot0.buffer.data() + local * 2u;
							double *amp1 = slot1.buffer.data() + local * 2u;
							const StateIndex state0 = static_cast<StateIndex>(chunk0BaseState + local);
							const StateIndex state1 = static_cast<StateIndex>(chunk1BaseState + local);
							const PairMutation mutation = pairFn(state0, state1, amp0, amp1);
							dirty0 = dirty0 || touchesFirst(mutation);
							dirty1 = dirty1 || touchesSecond(mutation);
						}
						if(dirty0) slot0.dirty = true;
						if(dirty1) slot1.dirty = true;
					}
				}
			}

			template <typename PairFn>
			void runPairKernel(unsigned int targetMask, PairFn pairFn) {
				switch(selectPairKernelMode(targetMask)) {
					case PairKernelMode::IntraChunk:
						runPairKernelIntraChunk(targetMask, pairFn);
						break;
					case PairKernelMode::InterChunk:
						if(cacheCapacity() >= 2u) {
							runPairKernelInterChunk(targetMask, pairFn);
						} else {
							runPairKernelFallback(targetMask, pairFn);
						}
						break;
					case PairKernelMode::Fallback:
					default:
						runPairKernelFallback(targetMask, pairFn);
						break;
				}
			}

		Amplitude readAmplitudeFromCacheOrStorage(StateIndex state) const {
			const ChunkRef ref = chunkRef(state);
			if(ref.chunkIndex >= chunks_.size()) {
				throw std::out_of_range("ZfpStateBackend: chunk index out of range for read");
			}

			if(const CacheSlot *slot = findCachedSlot(ref.chunkIndex)) {
				return {slot->buffer[ref.elemOffset], slot->buffer[ref.elemOffset + 1u]};
			}

			std::vector<double> scratch;
			decompressBuffer(chunks_[ref.chunkIndex], scratch);
			return {scratch[ref.elemOffset], scratch[ref.elemOffset + 1u]};
		}

		Amplitude readAmplitudeMutable(StateIndex state) {
			const ChunkRef ref = chunkRef(state);
			CacheSlot &slot = acquireSlot(ref.chunkIndex);
			return {slot.buffer[ref.elemOffset], slot.buffer[ref.elemOffset + 1u]};
		}

		void writeAmplitudeMutable(StateIndex state, Amplitude amp) {
			const ChunkRef ref = chunkRef(state);
			CacheSlot &slot = acquireSlot(ref.chunkIndex);
			slot.buffer[ref.elemOffset] = amp.real;
			slot.buffer[ref.elemOffset + 1u] = amp.imag;
			slot.dirty = true;
		}

		void flushDirtyCache() {
			for(CacheSlot &slot : cache_) {
				writeBackSlot(slot);
			}
		}

		void flushDirtyCacheIfNeeded() {
			if(batchDepth_ == 0u) {
				flushDirtyCache();
			}
		}

		void resetStorage(unsigned int numQubits) {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			batchDepth_ = 0u;
			clearCache();

			const size_t totalElems = checkedAmplitudeElementCount(numQubits_);
			const size_t fullChunkElems = chunkElemsPerFullChunk();
			const size_t chunkCount = (totalElems + fullChunkElems - 1u) / fullChunkElems;
			chunks_.assign(chunkCount, ChunkStorage{});
			chunkToSlot_.assign(chunkCount, kInvalidChunk);
			for(size_t idx = 0; idx < chunkCount; ++idx) {
				const size_t begin = idx * fullChunkElems;
				const size_t remaining = totalElems - begin;
				chunks_[idx].elemCount = std::min(fullChunkElems, remaining);
			}
		}

		template <typename MutateFn>
		void mutate(const char *operation, MutateFn mutateFn) {
			ensureInitialized(operation);
			mutateFn();
			flushDirtyCacheIfNeeded();
		}

	public:
		explicit ZfpStateBackend(const RegisterConfig &cfg) : cfg_(cfg) {
			validateZfpConfig(cfg_);
		}

		ZfpStateBackend(const ZfpStateBackend &) = default;
		ZfpStateBackend &operator=(const ZfpStateBackend &) = default;

		std::unique_ptr<IStateBackend> clone() const override {
			return std::make_unique<ZfpStateBackend>(*this);
		}

		void initZero(unsigned int numQubits) override {
			resetStorage(numQubits);
			std::vector<double> zeroChunk;
			for(ChunkStorage &chunk : chunks_) {
				zeroChunk.assign(chunk.elemCount, 0.0);
				compressBuffer(zeroChunk.data(), zeroChunk.size(), chunk.compressed);
			}
		}

		void initBasis(unsigned int numQubits, StateIndex initState, Amplitude amp) override {
			initZero(numQubits);
			validateBackendStateIndex("ZfpStateBackend::initBasis", initState, numStates_);
			writeAmplitudeMutable(initState, amp);
			flushDirtyCache();
		}

		void initUniformSuperposition(unsigned int numQubits, const BasisStateList &basisStates) override {
			resetStorage(numQubits);

			std::vector<StateIndex> selected;
			selected.reserve(basisStates.size());
			for(StateIndex state : basisStates) {
				validateBackendStateIndex("ZfpStateBackend::initUniformSuperposition", state, numStates_);
				selected.push_back(state);
			}
			std::sort(selected.begin(), selected.end());
			selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
			if(selected.empty()) {
				throw std::invalid_argument("ZfpStateBackend::initUniformSuperposition requires at least one state");
			}

			const double ampReal = 1.0 / std::sqrt(static_cast<double>(selected.size()));
			const size_t chunkStatesCount = chunkStates();
			std::vector<double> denseChunk;
			size_t selectedPos = 0u;
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				ChunkStorage &chunk = chunks_[chunkIndex];
				denseChunk.assign(chunk.elemCount, 0.0);
				const size_t chunkStateBegin = chunkIndex * chunkStatesCount;
				const size_t chunkStateCount = chunk.elemCount / 2u;
				const size_t chunkStateEnd = chunkStateBegin + chunkStateCount;
				while(selectedPos < selected.size() && static_cast<size_t>(selected[selectedPos]) < chunkStateEnd) {
					const size_t state = selected[selectedPos];
					if(state >= chunkStateBegin) {
						const size_t localState = state - chunkStateBegin;
						denseChunk[2u * localState] = ampReal;
					}
					++selectedPos;
				}
				compressBuffer(denseChunk.data(), denseChunk.size(), chunk.compressed);
			}
		}

		void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) override {
			resetStorage(numQubits);
			if(amplitudes.size() != checkedAmplitudeElementCount(numQubits_)) {
				throw std::invalid_argument("ZfpStateBackend: amplitudes size mismatch");
			}

			const size_t fullChunkElems = chunkElemsPerFullChunk();
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				ChunkStorage &chunk = chunks_[chunkIndex];
				const size_t begin = chunkIndex * fullChunkElems;
				compressBuffer(amplitudes.data() + begin, chunk.elemCount, chunk.compressed);
			}
		}

		Amplitude amplitude(StateIndex state) const override {
			ensureInitialized("amplitude query");
			validateBackendStateIndex("ZfpStateBackend::amplitude", state, numStates_);
			return readAmplitudeFromCacheOrStorage(state);
		}

		void setAmplitude(StateIndex state, Amplitude amp) override {
			mutate("state update", [&]() {
				validateBackendStateIndex("ZfpStateBackend::setAmplitude", state, numStates_);
				writeAmplitudeMutable(state, amp);
			});
		}

		double probability(StateIndex state) const override {
			const Amplitude amp = amplitude(state);
			return amp.real * amp.real + amp.imag * amp.imag;
		}

		double totalProbability() const override {
			ensureInitialized("total probability query");
			double total = 0.0;
			std::vector<double> scratch;
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				const CacheSlot *cached = findCachedSlot(chunkIndex);
				const double *data = nullptr;
				size_t elemCount = chunks_[chunkIndex].elemCount;
				if(cached) {
					data = cached->buffer.data();
				} else {
					decompressBuffer(chunks_[chunkIndex], scratch);
					data = scratch.data();
				}
				for(size_t elem = 0; elem < elemCount; elem += 2u) {
					const double real = data[elem];
					const double imag = data[elem + 1u];
					total += real * real + imag * imag;
				}
			}
			return total;
		}

		StateIndex sampleMeasurement(double rnd) const override {
			ensureInitialized("measurement");
			if(rnd < 0.0 || rnd >= 1.0) {
				throw std::invalid_argument("ZfpStateBackend::sampleMeasurement requires rnd in [0,1)");
			}

			double cumulative = 0.0;
			size_t baseState = 0u;
			std::vector<double> scratch;
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				const CacheSlot *cached = findCachedSlot(chunkIndex);
				const double *data = nullptr;
				size_t elemCount = chunks_[chunkIndex].elemCount;
				if(cached) {
					data = cached->buffer.data();
				} else {
					decompressBuffer(chunks_[chunkIndex], scratch);
					data = scratch.data();
				}
				const size_t statesInChunk = elemCount / 2u;
				for(size_t localState = 0; localState < statesInChunk; ++localState) {
					const size_t elem = localState * 2u;
					const double real = data[elem];
					const double imag = data[elem + 1u];
					cumulative += real * real + imag * imag;
					if(rnd <= cumulative) {
						return static_cast<StateIndex>(baseState + localState);
					}
				}
				baseState += statesInChunk;
			}

			throw std::runtime_error("ZfpStateBackend::sampleMeasurement cumulative probability did not reach sample");
		}

		void printNonZeroStates(std::ostream &os, double epsilon) const override {
			ensureInitialized("state printing");
			if(epsilon < 0.0) {
				throw std::invalid_argument("ZfpStateBackend::printNonZeroStates epsilon must be non-negative");
			}

			size_t baseState = 0u;
			std::vector<double> scratch;
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				const CacheSlot *cached = findCachedSlot(chunkIndex);
				const double *data = nullptr;
				size_t elemCount = chunks_[chunkIndex].elemCount;
				if(cached) {
					data = cached->buffer.data();
				} else {
					decompressBuffer(chunks_[chunkIndex], scratch);
					data = scratch.data();
				}
				const size_t statesInChunk = elemCount / 2u;
				for(size_t localState = 0; localState < statesInChunk; ++localState) {
					const size_t elem = localState * 2u;
					const double real = data[elem];
					const double imag = data[elem + 1u];
					if(std::abs(real) > epsilon || std::abs(imag) > epsilon) {
						os << (baseState + localState) << ": " << real << " + " << imag << "i\n";
					}
				}
				baseState += statesInChunk;
			}
		}

		void phaseFlipBasisState(StateIndex state) override {
			mutate("basis-state phase flip", [&]() {
				validateBackendStateIndex("ZfpStateBackend::phaseFlipBasisState", state, numStates_);
				const ChunkRef ref = chunkRef(state);
				CacheSlot &slot = acquireSlot(ref.chunkIndex);
				slot.buffer[ref.elemOffset] = -slot.buffer[ref.elemOffset];
				slot.buffer[ref.elemOffset + 1u] = -slot.buffer[ref.elemOffset + 1u];
				slot.dirty = true;
			});
		}

		void inversionAboutMean() override {
			mutate("inversion about mean", [&]() {
				double sumReal = 0.0;
				double sumImag = 0.0;
				for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
					CacheSlot &slot = acquireSlot(chunkIndex);
					for(size_t elem = 0; elem < chunks_[chunkIndex].elemCount; elem += 2u) {
						sumReal += slot.buffer[elem];
						sumImag += slot.buffer[elem + 1u];
					}
				}
				const double meanReal = sumReal / static_cast<double>(numStates_);
				const double meanImag = sumImag / static_cast<double>(numStates_);

				for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
					CacheSlot &slot = acquireSlot(chunkIndex);
					for(size_t elem = 0; elem < chunks_[chunkIndex].elemCount; elem += 2u) {
						const double real = slot.buffer[elem];
						const double imag = slot.buffer[elem + 1u];
						slot.buffer[elem] = 2.0 * meanReal - real;
						slot.buffer[elem + 1u] = 2.0 * meanImag - imag;
					}
					slot.dirty = true;
				}
			});
		}

		void applyHadamard(QubitIndex qubit) override {
			ensureInitialized("hadamard application");
			validateBackendSingleQubit("ZfpStateBackend::applyHadamard", qubit, numQubits_);
			const unsigned int targetMask = qubitMaskFromMsbIndex(qubit, numQubits_);
			const double invSqrt2 = 1.0 / std::sqrt(2.0);

			mutate("hadamard application", [&]() {
				runPairKernel(targetMask, [&](StateIndex, StateIndex, double *a0, double *a1) -> PairMutation {
					const double aReal = a0[0];
					const double aImag = a0[1];
					const double bReal = a1[0];
					const double bImag = a1[1];
					if(aReal == 0.0 && aImag == 0.0 && bReal == 0.0 && bImag == 0.0) {
						return PairMutation::None;
					}
					a0[0] = (aReal + bReal) * invSqrt2;
					a0[1] = (aImag + bImag) * invSqrt2;
					a1[0] = (aReal - bReal) * invSqrt2;
					a1[1] = (aImag - bImag) * invSqrt2;
					return PairMutation::Both;
				});
			});
		}

		void applyPauliX(QubitIndex qubit) override {
			ensureInitialized("pauli-x application");
			validateBackendSingleQubit("ZfpStateBackend::applyPauliX", qubit, numQubits_);
			const unsigned int targetMask = qubitMaskFromMsbIndex(qubit, numQubits_);

			mutate("pauli-x application", [&]() {
				runPairKernel(targetMask, [&](StateIndex, StateIndex, double *a0, double *a1) -> PairMutation {
					if(a0[0] == a1[0] && a0[1] == a1[1]) {
						return PairMutation::None;
					}
					std::swap(a0[0], a1[0]);
					std::swap(a0[1], a1[1]);
					return PairMutation::Both;
				});
			});
		}

		void applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) override {
			ensureInitialized("controlled phase-shift application");
			validateBackendTwoQubits("ZfpStateBackend::applyControlledPhaseShift", controlQubit, targetQubit, numQubits_);
			const unsigned int controlMask = qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = qubitMaskFromMsbIndex(targetQubit, numQubits_);
			const double phaseReal = std::cos(theta);
			const double phaseImag = std::sin(theta);

			mutate("controlled phase-shift application", [&]() {
				runPairKernel(targetMask, [&](StateIndex, StateIndex state1, double *, double *a1) -> PairMutation {
					if((state1 & controlMask) == 0u) return PairMutation::None;
					const double real = a1[0];
					const double imag = a1[1];
					if(real == 0.0 && imag == 0.0) return PairMutation::None;
					a1[0] = phaseReal * real - phaseImag * imag;
					a1[1] = phaseReal * imag + phaseImag * real;
					return PairMutation::Second;
				});
			});
		}

		void applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) override {
			ensureInitialized("controlled-not application");
			validateBackendTwoQubits("ZfpStateBackend::applyControlledNot", controlQubit, targetQubit, numQubits_);
			const unsigned int controlMask = qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = qubitMaskFromMsbIndex(targetQubit, numQubits_);

			mutate("controlled-not application", [&]() {
				runPairKernel(targetMask, [&](StateIndex state0, StateIndex, double *a0, double *a1) -> PairMutation {
					if((state0 & controlMask) == 0u) return PairMutation::None;
					if(a0[0] == a1[0] && a0[1] == a1[1]) return PairMutation::None;
					std::swap(a0[0], a1[0]);
					std::swap(a0[1], a1[1]);
					return PairMutation::Both;
				});
			});
		}

		void applyGate(const QuantumGate &gate, const QubitList &qubits) override {
			mutate("gate application", [&]() {
				auto loadAmplitude = [&](StateIndex state) -> Amplitude {
					return readAmplitudeMutable(state);
				};
				auto storeAmplitude = [&](StateIndex state, Amplitude amp) {
					writeAmplitudeMutable(state, amp);
				};
				applyGateThroughBlocks(
					"ZfpStateBackend::applyGate",
					gate,
					qubits,
					numQubits_,
					loadAmplitude,
					storeAmplitude,
					gateWorkspace_);
			});
		}

		void beginOperationBatch() override {
			ensureInitialized("batch begin");
			++batchDepth_;
		}

		void endOperationBatch() override {
			if(batchDepth_ == 0u) {
				throw std::logic_error("ZfpStateBackend::endOperationBatch called without matching beginOperationBatch");
			}
			--batchDepth_;
			if(batchDepth_ == 0u) {
				flushDirtyCache();
			}
		}
};

bool isZfpBackendCompiled() {
	return true;
}

std::unique_ptr<IStateBackend> createZfpBackend(unsigned int numQubits, const RegisterConfig &cfg) {
	(void)numQubits;
	return std::make_unique<ZfpStateBackend>(cfg);
}

#else

bool isZfpBackendCompiled() {
	return false;
}

std::unique_ptr<IStateBackend> createZfpBackend(unsigned int, const RegisterConfig &) {
	return nullptr;
}

#endif

} // namespace tmfqs
