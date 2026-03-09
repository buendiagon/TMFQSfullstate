#include "tmfqs/storage/i_state_backend.h"
#include "tmfqs/storage/gate_block_apply.h"
#include "tmfqs/core/state_space.h"
#include "backend_common.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tmfqs {

#ifdef HAVE_BLOSC2
#include <blosc2.h>

namespace {
struct BloscRuntimeGuard {
	BloscRuntimeGuard() { blosc2_init(); }
	~BloscRuntimeGuard() { blosc2_destroy(); }
};

void ensureBloscRuntimeInitialized() {
	static BloscRuntimeGuard runtimeGuard;
	(void)runtimeGuard;
}

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

using BloscContextPtr = std::unique_ptr<blosc2_context, BloscContextDeleter>;
using BloscSchunkPtr = std::unique_ptr<blosc2_schunk, BloscSchunkDeleter>;

struct ChunkGuard {
	uint8_t *data = nullptr;
	bool needsFree = false;

	~ChunkGuard() {
		if(needsFree && data) free(data);
	}
};

int32_t checkedBytesToI32(size_t numBytes, const char *context) {
	if(numBytes > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
		throw std::overflow_error(std::string("Blosc2 byte-count overflow in ") + context);
	}
	return static_cast<int32_t>(numBytes);
}

int32_t checkedElemsBytesToI32(size_t numElems, const char *context) {
	if(numElems > (std::numeric_limits<size_t>::max() / sizeof(double))) {
		throw std::overflow_error(std::string("Blosc2 element-to-byte overflow in ") + context);
	}
	return checkedBytesToI32(numElems * sizeof(double), context);
}

constexpr double kGateMatchTolerance = 1e-12;

bool approxEqual(double a, double b, double tol = kGateMatchTolerance) {
	return std::abs(a - b) <= tol;
}

bool matchesAmplitude(const Amplitude &amp, double real, double imag = 0.0) {
	return approxEqual(amp.real, real) && approxEqual(amp.imag, imag);
}

bool isZeroAmplitude(const Amplitude &amp) {
	return matchesAmplitude(amp, 0.0, 0.0);
}

bool matchesHadamardGate(const QuantumGate &gate) {
	if(gate.dimension() != 2u) return false;
	const double invSqrt2 = 1.0 / std::sqrt(2.0);
	return matchesAmplitude(gate[0][0], invSqrt2) &&
	       matchesAmplitude(gate[0][1], invSqrt2) &&
	       matchesAmplitude(gate[1][0], invSqrt2) &&
	       matchesAmplitude(gate[1][1], -invSqrt2);
}

bool matchesPauliXGate(const QuantumGate &gate) {
	if(gate.dimension() != 2u) return false;
	return isZeroAmplitude(gate[0][0]) &&
	       matchesAmplitude(gate[0][1], 1.0) &&
	       matchesAmplitude(gate[1][0], 1.0) &&
	       isZeroAmplitude(gate[1][1]);
}

bool matchesControlledNotGate(const QuantumGate &gate) {
	if(gate.dimension() != 4u) return false;
	for(unsigned int row = 0; row < 4u; ++row) {
		for(unsigned int col = 0; col < 4u; ++col) {
			const bool isOne =
				(row == 0u && col == 0u) ||
				(row == 1u && col == 1u) ||
				(row == 2u && col == 3u) ||
				(row == 3u && col == 2u);
			if(isOne) {
				if(!matchesAmplitude(gate[row][col], 1.0)) return false;
			} else if(!isZeroAmplitude(gate[row][col])) {
				return false;
			}
		}
	}
	return true;
}

bool matchControlledPhaseShiftGate(const QuantumGate &gate, double &thetaOut) {
	if(gate.dimension() != 4u) return false;
	for(unsigned int row = 0; row < 4u; ++row) {
		for(unsigned int col = 0; col < 4u; ++col) {
			const bool isDiag = (row == col);
			if(!isDiag && !isZeroAmplitude(gate[row][col])) {
				return false;
			}
		}
	}
	if(!matchesAmplitude(gate[0][0], 1.0) ||
	   !matchesAmplitude(gate[1][1], 1.0) ||
	   !matchesAmplitude(gate[2][2], 1.0)) {
		return false;
	}
	const Amplitude phase = gate[3][3];
	const double magnitudeSq = phase.real * phase.real + phase.imag * phase.imag;
	if(!approxEqual(magnitudeSq, 1.0, 1e-10)) return false;
	thetaOut = std::atan2(phase.imag, phase.real);
	return true;
}

void validateBloscConfig(const RegisterConfig &cfg) {
	if(cfg.blosc.chunkStates == 0) {
		throw std::invalid_argument("BloscStateBackend: chunkStates must be >= 1");
	}
	if(cfg.blosc.clevel < 0 || cfg.blosc.clevel > 9) {
		throw std::invalid_argument("BloscStateBackend: clevel must be in [0, 9]");
	}
	if(cfg.blosc.nthreads <= 0) {
		throw std::invalid_argument("BloscStateBackend: nthreads must be >= 1");
	}
	if(cfg.blosc.gateCacheSlots == 0) {
		throw std::invalid_argument("BloscStateBackend: gateCacheSlots must be >= 1");
	}
	const size_t maxElemsPerChunk = static_cast<size_t>(std::numeric_limits<int32_t>::max()) / sizeof(double);
	const size_t requestedElems = cfg.blosc.chunkStates * 2u;
	if(requestedElems > maxElemsPerChunk) {
		throw std::invalid_argument("BloscStateBackend: chunkStates produces chunk payload larger than int32 byte limits");
	}
}
} // namespace

class BloscStateBackend : public IStateBackend {

		private:
			struct CacheSlot {
				std::vector<double> buffer;
				int64_t chunkIndex = -1;
				bool dirty = false;
				size_t lruPrev = std::numeric_limits<size_t>::max();
				size_t lruNext = std::numeric_limits<size_t>::max();
			};

			struct StateChunkRef {
				int64_t chunkIndex = 0;
				size_t elemOffset = 0;
				size_t chunkElems = 0;
			};

			struct CacheAmplitudeRef {
				CacheSlot *slot = nullptr;
				double *amp = nullptr; // points to {real, imag}
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
			BloscSchunkPtr schunk_;
			blosc2_cparams schunkCParams_ = BLOSC2_CPARAMS_DEFAULTS;
			BloscContextPtr compressionCtx_;
			std::vector<uint8_t> compressionScratch_;
			mutable std::vector<double> ioChunkScratch_;
			std::vector<CacheSlot> gateCache_;
			std::unordered_map<int64_t, size_t> chunkToSlot_;
			std::vector<size_t> freeSlots_;
			size_t lruHead_ = std::numeric_limits<size_t>::max();
			size_t lruTail_ = std::numeric_limits<size_t>::max();
			unsigned int batchDepth_ = 0;
			GateBlockWorkspace gateWorkspace_;

		size_t chunkStates() const {
			return cfg_.blosc.chunkStates;
		}

		size_t elemsPerChunk() const {
			return chunkStates() * 2u;
		}

		size_t totalElems() const {
			return static_cast<size_t>(numStates_) * 2u;
		}

			void ensureInitialized(const char *opName) const {
				ensureBackendInitialized(static_cast<bool>(schunk_), "BloscStateBackend", opName);
			}

			void syncPendingCacheForRead() const {
				if(batchDepth_ > 0u) {
					auto *self = const_cast<BloscStateBackend *>(this);
					self->flushAllSlots();
				}
			}

			void resetCompressionScratch() {
				compressionCtx_.reset();
				compressionScratch_.clear();
			}

			void resetRuntimeScratch() {
				ioChunkScratch_.clear();
			}

			static constexpr size_t kInvalidSlot = std::numeric_limits<size_t>::max();

			static bool touchesFirst(PairMutation mutation) {
				return mutation == PairMutation::First || mutation == PairMutation::Both;
			}

			static bool touchesSecond(PairMutation mutation) {
				return mutation == PairMutation::Second || mutation == PairMutation::Both;
			}

			void detachFromLru(size_t slotIndex) {
				CacheSlot &slot = gateCache_[slotIndex];
				if(slot.lruPrev == kInvalidSlot && slot.lruNext == kInvalidSlot && lruHead_ != slotIndex) {
					return;
				}
				if(slot.lruPrev != kInvalidSlot) {
					gateCache_[slot.lruPrev].lruNext = slot.lruNext;
				} else {
					lruHead_ = slot.lruNext;
				}
				if(slot.lruNext != kInvalidSlot) {
					gateCache_[slot.lruNext].lruPrev = slot.lruPrev;
				} else {
					lruTail_ = slot.lruPrev;
				}
				slot.lruPrev = kInvalidSlot;
				slot.lruNext = kInvalidSlot;
			}

			void attachToLruFront(size_t slotIndex) {
				CacheSlot &slot = gateCache_[slotIndex];
				slot.lruPrev = kInvalidSlot;
				slot.lruNext = lruHead_;
				if(lruHead_ != kInvalidSlot) {
					gateCache_[lruHead_].lruPrev = slotIndex;
				} else {
					lruTail_ = slotIndex;
				}
				lruHead_ = slotIndex;
			}

			void markSlotRecent(size_t slotIndex) {
				if(lruHead_ == slotIndex) return;
				detachFromLru(slotIndex);
				attachToLruFront(slotIndex);
			}

			void resetGateCacheEntries() {
				chunkToSlot_.clear();
				freeSlots_.clear();
				lruHead_ = kInvalidSlot;
				lruTail_ = kInvalidSlot;
				freeSlots_.reserve(gateCache_.size());
				for(CacheSlot &slot : gateCache_) {
					slot.chunkIndex = -1;
					slot.dirty = false;
					slot.lruPrev = kInvalidSlot;
					slot.lruNext = kInvalidSlot;
				}
				for(size_t slot = gateCache_.size(); slot > 0; --slot) {
					freeSlots_.push_back(slot - 1u);
				}
			}

			void invalidateGateCache() {
				resetGateCacheEntries();
			}

			void flushDirtyCacheIfNeeded() {
				if(batchDepth_ == 0u) {
					flushAllSlots();
				}
			}

			void ensureGateCacheStorage() {
				const size_t slots = cfg_.blosc.gateCacheSlots;
				const size_t chunkElems = elemsPerChunk();
				bool shouldResetEntries = false;
				if(gateCache_.size() != slots) {
					gateCache_.assign(slots, {});
					shouldResetEntries = true;
				}
				for(CacheSlot &slot : gateCache_) {
					if(slot.buffer.size() != chunkElems) {
						slot.buffer.assign(chunkElems, 0.0);
						shouldResetEntries = true;
					}
				}
				if(shouldResetEntries) {
					resetGateCacheEntries();
				}
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

		blosc2_cparams makeCParams() const {
			blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
			cparams.compcode = static_cast<uint8_t>(cfg_.blosc.compcode);
			cparams.clevel = cfg_.blosc.clevel;
			cparams.nthreads = cfg_.blosc.nthreads;
			cparams.typesize = sizeof(double);
			cparams.filters[BLOSC2_MAX_FILTERS - 1] = cfg_.blosc.useShuffle ? BLOSC_SHUFFLE : BLOSC_NOFILTER;
			return cparams;
		}

		void ensureCompressionContext() {
			if(compressionCtx_) return;
			blosc2_cparams cparams = makeCParams();
			compressionCtx_ = BloscContextPtr(blosc2_create_cctx(cparams));
			if(!compressionCtx_) {
				throw std::runtime_error("Blosc2: failed creating compression context");
			}
		}

		void ensureCompressionScratch(size_t srcBytes) {
			const size_t required = srcBytes + BLOSC2_MAX_OVERHEAD;
			if(compressionScratch_.size() < required) {
				compressionScratch_.resize(required);
			}
		}

			void createEmptySchunk() {
				blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
				schunkCParams_ = makeCParams();
			storage.cparams = &schunkCParams_;
			blosc2_schunk *raw = blosc2_schunk_new(&storage);
			if(!raw) {
				throw std::runtime_error("Blosc2: failed creating super-chunk");
			}
				schunk_.reset(raw);
			}

			void prepareFreshStorage(unsigned int numQubits) {
				numQubits_ = numQubits;
				numStates_ = checkedStateCount(numQubits_);
				createEmptySchunk();
				ensureGateCacheStorage();
				resetGateCacheEntries();
				resetCompressionScratch();
				resetRuntimeScratch();
			}

			size_t chunkElemsForIndex(int64_t chunkIndex) const {
				ensureInitialized("chunk indexing");
				if(chunkIndex < 0 || chunkIndex >= schunk_->nchunks) {
					throw std::out_of_range("BloscStateBackend: chunk index out of range");
				}
				const size_t chunkBaseElem = static_cast<size_t>(chunkIndex) * elemsPerChunk();
				if(chunkBaseElem >= totalElems()) {
					throw std::out_of_range("BloscStateBackend: chunk offset out of range");
				}
				return std::min(elemsPerChunk(), totalElems() - chunkBaseElem);
			}

			StateChunkRef locateState(StateIndex state) const {
				const size_t statesPerChunk = chunkStates();
				const int64_t chunkIndex = static_cast<int64_t>(state / statesPerChunk);
				const size_t elemOffset = static_cast<size_t>(state % statesPerChunk) * 2u;
				return {chunkIndex, elemOffset, chunkElemsForIndex(chunkIndex)};
			}

			void readChunk(int64_t chunkIndex, std::vector<double> &buffer, size_t expectedElems) const {
				ensureInitialized("chunk read");
				if(expectedElems == 0) {
					throw std::invalid_argument("BloscStateBackend: chunk read expects at least one element");
				}
				if(buffer.size() < expectedElems) buffer.resize(expectedElems, 0.0);
				const int32_t destBytes = checkedElemsBytesToI32(expectedElems, "readChunk");
				const int dsz = blosc2_schunk_decompress_chunk(
					schunk_.get(), chunkIndex, buffer.data(), destBytes);
				if(dsz < 0) {
					throw std::runtime_error("Blosc2: chunk decompress failed");
				}
				if(dsz != destBytes) {
					throw std::runtime_error(
						"Blosc2: chunk decompress size mismatch (expected " +
						std::to_string(destBytes) + " bytes, got " + std::to_string(dsz) + ")");
				}
			}

			void writeChunk(int64_t chunkIndex, const std::vector<double> &buffer, size_t elems) {
				ensureInitialized("chunk write");
				if(elems == 0) {
					throw std::invalid_argument("BloscStateBackend: chunk write expects at least one element");
				}
				ensureCompressionContext();
				const size_t srcBytes = elems * sizeof(double);
				ensureCompressionScratch(srcBytes);
			const int32_t srcBytesI32 = checkedBytesToI32(srcBytes, "writeChunk");
			const int32_t dstBytesI32 = checkedBytesToI32(compressionScratch_.size(), "writeChunk scratch");
			const int csz = blosc2_compress_ctx(
				compressionCtx_.get(),
				buffer.data(),
				srcBytesI32,
				compressionScratch_.data(),
				dstBytesI32);
			if(csz <= 0) {
				throw std::runtime_error("Blosc2: chunk compress failed");
			}
			const int64_t rc = blosc2_schunk_update_chunk(
				schunk_.get(), chunkIndex, compressionScratch_.data(), true);
				if(rc < 0) {
					throw std::runtime_error("Blosc2: chunk update failed");
				}
			}

			void appendChunk(const double *data, size_t elems, const char *context) {
				const int32_t bytes = checkedElemsBytesToI32(elems, context);
				const int64_t nchunks = blosc2_schunk_append_buffer(schunk_.get(), data, bytes);
				if(nchunks < 0) {
					throw std::runtime_error(std::string("Blosc2: append chunk failed in ") + context);
				}
			}

		void validateState(StateIndex state) const {
			validateBackendStateIndex("BloscStateBackend state operation", state, numStates_);
		}

		void validateSingleQubitOperation(QubitIndex qubit, const char *opName) const {
			ensureInitialized(opName);
			validateBackendSingleQubit("BloscStateBackend single-qubit operation", qubit, numQubits_);
		}

		void validateTwoQubitOperation(QubitIndex controlQubit, QubitIndex targetQubit, const char *opName) const {
			ensureInitialized(opName);
			validateBackendTwoQubits(
				"BloscStateBackend two-qubit operation", controlQubit, targetQubit, numQubits_);
		}

		void validateGateApplicationInputs(const QuantumGate &gate, const QubitList &qubits) const {
			ensureInitialized("gate application");
			validateGateTargets("BloscStateBackend::applyGate", qubits, numQubits_, gate.dimension());
		}

			void flushSlot(size_t slotIndex) {
				CacheSlot &slot = gateCache_[slotIndex];
				if(!slot.dirty || slot.chunkIndex < 0) return;
				const size_t chunkElems = chunkElemsForIndex(slot.chunkIndex);
				writeChunk(slot.chunkIndex, slot.buffer, chunkElems);
				slot.dirty = false;
			}

			void flushAllSlots() {
				for(size_t slot = 0; slot < gateCache_.size(); ++slot) {
					flushSlot(slot);
				}
			}

		int getSlot(int64_t chunkIndex) {
			const auto hit = chunkToSlot_.find(chunkIndex);
			if(hit != chunkToSlot_.end()) {
				markSlotRecent(hit->second);
				return static_cast<int>(hit->second);
			}

			size_t slotIndex = kInvalidSlot;
			if(!freeSlots_.empty()) {
				slotIndex = freeSlots_.back();
				freeSlots_.pop_back();
			} else {
				slotIndex = lruTail_;
				if(slotIndex == kInvalidSlot) {
					throw std::logic_error("BloscStateBackend cache invariant broken: no free slot and empty LRU");
				}
				flushSlot(slotIndex);
				CacheSlot &evicted = gateCache_[slotIndex];
				if(evicted.chunkIndex >= 0) {
					chunkToSlot_.erase(evicted.chunkIndex);
				}
				detachFromLru(slotIndex);
			}

			CacheSlot &slot = gateCache_[slotIndex];
			const size_t chunkElems = chunkElemsForIndex(chunkIndex);
			readChunk(chunkIndex, slot.buffer, chunkElems);
			slot.chunkIndex = chunkIndex;
			slot.dirty = false;
			slot.lruPrev = kInvalidSlot;
			slot.lruNext = kInvalidSlot;
			chunkToSlot_[chunkIndex] = slotIndex;
			attachToLruFront(slotIndex);
			return static_cast<int>(slotIndex);
		}

		CacheAmplitudeRef acquireCacheAmplitude(StateIndex state, size_t statesPerChunk) {
			const int64_t chunkIndex = static_cast<int64_t>(state / statesPerChunk);
			const size_t elemOffset = static_cast<size_t>(state % statesPerChunk) * 2u;
			CacheSlot &slot = gateCache_[static_cast<size_t>(getSlot(chunkIndex))];
			return {&slot, slot.buffer.data() + elemOffset};
		}

		template <typename PairFn>
		void runPairKernelFallback(unsigned int targetMask, PairFn pairFn) {
			const size_t statesPerChunk = chunkStates();
			forEachStatePairByMask(numStates_, targetMask, [&](unsigned int state0, unsigned int state1) {
				CacheAmplitudeRef a0Ref = acquireCacheAmplitude(state0, statesPerChunk);
				CacheAmplitudeRef a1Ref = acquireCacheAmplitude(state1, statesPerChunk);
				const PairMutation mutation = pairFn(state0, state1, a0Ref.amp, a1Ref.amp);
				if(mutation == PairMutation::None) return;
				if(a0Ref.slot == a1Ref.slot) {
					a0Ref.slot->dirty = true;
					return;
				}
				if(touchesFirst(mutation)) {
					a0Ref.slot->dirty = true;
				}
				if(touchesSecond(mutation)) {
					a1Ref.slot->dirty = true;
				}
			});
		}

		template <typename PairFn>
		void runPairKernelIntraChunk(unsigned int targetMask, PairFn pairFn) {
			const size_t statesPerChunk = chunkStates();
			const size_t localMask = static_cast<size_t>(targetMask);
			const size_t stride = localMask << 1u;
			for(int64_t chunkIndex = 0; chunkIndex < schunk_->nchunks; ++chunkIndex) {
				const size_t slotIndex = static_cast<size_t>(getSlot(chunkIndex));
				CacheSlot &slot = gateCache_[slotIndex];
				const size_t statesInChunk = chunkElemsForIndex(chunkIndex) / 2u;
				const size_t chunkBaseState = static_cast<size_t>(chunkIndex) * statesPerChunk;
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
			const size_t nchunks = static_cast<size_t>(schunk_->nchunks);
			for(size_t groupBase = 0; groupBase < nchunks; groupBase += chunkStride) {
				for(size_t groupOffset = 0; groupOffset < chunkDelta; ++groupOffset) {
					const size_t chunk0 = groupBase + groupOffset;
					const size_t chunk1 = chunk0 + chunkDelta;
					if(chunk1 >= nchunks) break;

					const size_t slot0Index = static_cast<size_t>(getSlot(static_cast<int64_t>(chunk0)));
					const size_t slot1Index = static_cast<size_t>(getSlot(static_cast<int64_t>(chunk1)));
					CacheSlot &slot0 = gateCache_[slot0Index];
					CacheSlot &slot1 = gateCache_[slot1Index];
					const size_t statesInChunk0 = chunkElemsForIndex(static_cast<int64_t>(chunk0)) / 2u;
					const size_t statesInChunk1 = chunkElemsForIndex(static_cast<int64_t>(chunk1)) / 2u;
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
					if(gateCache_.size() >= 2u) {
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

		void swap(BloscStateBackend &other) noexcept {
			using std::swap;
			swap(numQubits_, other.numQubits_);
			swap(numStates_, other.numStates_);
			swap(cfg_, other.cfg_);
			swap(schunk_, other.schunk_);
				swap(schunkCParams_, other.schunkCParams_);
				swap(compressionCtx_, other.compressionCtx_);
				swap(compressionScratch_, other.compressionScratch_);
				swap(ioChunkScratch_, other.ioChunkScratch_);
				swap(gateCache_, other.gateCache_);
				swap(chunkToSlot_, other.chunkToSlot_);
				swap(freeSlots_, other.freeSlots_);
				swap(lruHead_, other.lruHead_);
				swap(lruTail_, other.lruTail_);
				swap(batchDepth_, other.batchDepth_);
				swap(gateWorkspace_, other.gateWorkspace_);
			}

	public:
		explicit BloscStateBackend(const RegisterConfig &cfg) : cfg_(cfg) {
			ensureBloscRuntimeInitialized();
			validateBloscConfig(cfg_);
		}

		BloscStateBackend(const BloscStateBackend &other) : cfg_(other.cfg_) {
			ensureBloscRuntimeInitialized();
			validateBloscConfig(cfg_);
			numQubits_ = other.numQubits_;
			numStates_ = other.numStates_;
			if(!other.schunk_) return;
			createEmptySchunk();
			try {
				for(int64_t ci = 0; ci < other.schunk_->nchunks; ++ci) {
					ChunkGuard chunk;
					const int cbytes = blosc2_schunk_get_chunk(
						other.schunk_.get(), ci, &chunk.data, &chunk.needsFree);
					if(cbytes < 0) {
						throw std::runtime_error("Blosc2: failed reading chunk while cloning");
					}
					const int64_t rc = blosc2_schunk_append_chunk(schunk_.get(), chunk.data, true);
					if(rc < 0) {
						throw std::runtime_error("Blosc2: failed appending chunk while cloning");
					}
				}
			} catch(...) {
				schunk_.reset();
				throw;
			}
			ensureGateCacheStorage();
		}

		BloscStateBackend &operator=(BloscStateBackend other) {
			swap(other);
			return *this;
		}

		~BloscStateBackend() override = default;

		std::unique_ptr<IStateBackend> clone() const override {
			return std::make_unique<BloscStateBackend>(*this);
		}

		void beginOperationBatch() override {
			++batchDepth_;
		}

		void endOperationBatch() override {
			if(batchDepth_ == 0u) {
				throw std::logic_error("BloscStateBackend::endOperationBatch called without matching beginOperationBatch");
			}
			--batchDepth_;
			if(batchDepth_ == 0u) {
				flushAllSlots();
			}
		}

			void initZero(unsigned int numQubits) override {
				prepareFreshStorage(numQubits);

				const size_t statesPerChunk = chunkStates();
				const size_t chunkElems = elemsPerChunk();
				std::vector<double> chunkBuffer(chunkElems, 0.0);

				for(size_t base = 0; base < numStates_; base += statesPerChunk) {
					const size_t statesInChunk = std::min(statesPerChunk, static_cast<size_t>(numStates_) - base);
					const size_t elemsInChunk = statesInChunk * 2u;
					appendChunk(chunkBuffer.data(), elemsInChunk, "BloscStateBackend::initZero");
				}
			}

		void initBasis(unsigned int numQubits, StateIndex initState, Amplitude amp) override {
			initZero(numQubits);
			if(initState >= numStates_) {
				throw std::out_of_range("BloscStateBackend::initBasis state index out of range");
			}
			setAmplitude(initState, amp);
		}

			void initUniformSuperposition(unsigned int numQubits, const BasisStateList &basisStates) override {
				const unsigned int stateCount = checkedStateCount(numQubits);
				std::vector<StateIndex> selected;
				selected.reserve(basisStates.size());
				for(StateIndex state : basisStates) {
					if(state >= stateCount) {
						throw std::out_of_range("BloscStateBackend::initUniformSuperposition basis state index out of range");
					}
					selected.push_back(state);
			}
			std::sort(selected.begin(), selected.end());
			selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
				if(selected.empty()) {
					throw std::invalid_argument("BloscStateBackend: no valid basis states for uniform initialization");
				}

				prepareFreshStorage(numQubits);
				const size_t statesPerChunk = chunkStates();
				const double realAmplitude = 1.0 / std::sqrt(static_cast<double>(selected.size()));
				const size_t chunkElemsMax = elemsPerChunk();
				std::vector<double> chunkBuffer(chunkElemsMax, 0.0);

				size_t selectedIndex = 0;
				for(size_t baseState = 0; baseState < numStates_; baseState += statesPerChunk) {
					const size_t statesInChunk = std::min(statesPerChunk, static_cast<size_t>(numStates_) - baseState);
					const size_t elemsInChunk = statesInChunk * 2u;
					std::fill_n(chunkBuffer.begin(), elemsInChunk, 0.0);
					const size_t chunkEnd = baseState + statesInChunk;

					while(selectedIndex < selected.size() && selected[selectedIndex] < chunkEnd) {
						const size_t localState = static_cast<size_t>(selected[selectedIndex]) - baseState;
						chunkBuffer[localState * 2u] = realAmplitude;
						chunkBuffer[localState * 2u + 1u] = 0.0;
						++selectedIndex;
					}

					appendChunk(chunkBuffer.data(), elemsInChunk, "BloscStateBackend::initUniformSuperposition");
				}
			}

			void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) override {
				const unsigned int stateCount = checkedStateCount(numQubits);
				if(amplitudes.size() != checkedAmplitudeElementCount(numQubits)) {
					throw std::invalid_argument("BloscStateBackend: amplitudes size mismatch");
				}
				prepareFreshStorage(numQubits);
				if(stateCount != numStates_) {
					throw std::logic_error("BloscStateBackend: state-count mismatch while loading amplitudes");
				}
				const size_t statesPerChunk = chunkStates();
				for(size_t baseState = 0; baseState < numStates_; baseState += statesPerChunk) {
					const size_t statesInChunk = std::min(statesPerChunk, static_cast<size_t>(numStates_) - baseState);
					const size_t elemOffset = baseState * 2u;
					appendChunk(
						amplitudes.data() + elemOffset,
						statesInChunk * 2u,
						"BloscStateBackend::loadAmplitudes");
				}
			}

			Amplitude amplitude(StateIndex state) const override {
				ensureInitialized("amplitude query");
				syncPendingCacheForRead();
				validateState(state);
				const StateChunkRef chunkRef = locateState(state);
				readChunk(chunkRef.chunkIndex, ioChunkScratch_, chunkRef.chunkElems);
				return {ioChunkScratch_[chunkRef.elemOffset], ioChunkScratch_[chunkRef.elemOffset + 1u]};
			}

			void setAmplitude(StateIndex state, Amplitude amp) override {
				ensureInitialized("state update");
				flushAllSlots();
				validateState(state);
				const StateChunkRef chunkRef = locateState(state);
				readChunk(chunkRef.chunkIndex, ioChunkScratch_, chunkRef.chunkElems);
				ioChunkScratch_[chunkRef.elemOffset] = amp.real;
				ioChunkScratch_[chunkRef.elemOffset + 1u] = amp.imag;
				writeChunk(chunkRef.chunkIndex, ioChunkScratch_, chunkRef.chunkElems);
				invalidateGateCache();
			}

		double probability(StateIndex state) const override {
			const Amplitude amp = amplitude(state);
			return amp.real * amp.real + amp.imag * amp.imag;
		}

		double totalProbability() const override {
			ensureInitialized("total probability query");
			syncPendingCacheForRead();
			double sum = 0.0;
			const size_t chunkElemsMax = elemsPerChunk();
			std::vector<double> buffer(chunkElemsMax, 0.0);
			for(int64_t ci = 0; ci < schunk_->nchunks; ++ci) {
				const size_t chunkElems = std::min(
					chunkElemsMax, totalElems() - static_cast<size_t>(ci) * chunkElemsMax);
				readChunk(ci, buffer, chunkElems);
				for(size_t elem = 0; elem + 1u < chunkElems; elem += 2u) {
					const double real = buffer[elem];
					const double imag = buffer[elem + 1u];
					sum += real * real + imag * imag;
				}
			}
			return sum;
		}

		StateIndex sampleMeasurement(double rnd) const override {
			ensureInitialized("measurement");
				syncPendingCacheForRead();
				if(rnd < 0.0 || rnd >= 1.0) {
					throw std::invalid_argument("BloscStateBackend::sampleMeasurement requires rnd in [0,1)");
				}
				double cumulative = 0.0;
				const size_t chunkElemsMax = elemsPerChunk();
				std::vector<double> buffer(chunkElemsMax, 0.0);
				StateIndex baseState = 0;
			for(int64_t ci = 0; ci < schunk_->nchunks; ++ci) {
				const size_t chunkElems = std::min(
					chunkElemsMax, totalElems() - static_cast<size_t>(ci) * chunkElemsMax);
				const size_t statesInChunk = chunkElems / 2u;
				readChunk(ci, buffer, chunkElems);
					for(size_t local = 0; local < statesInChunk; ++local) {
						const double real = buffer[local * 2u];
						const double imag = buffer[local * 2u + 1u];
						cumulative += real * real + imag * imag;
						if(rnd <= cumulative) {
							return baseState + static_cast<StateIndex>(local);
						}
					}
					baseState += static_cast<StateIndex>(statesInChunk);
				}
				throw std::runtime_error("BloscStateBackend::sampleMeasurement cumulative probability did not reach sample");
			}

		void printNonZeroStates(std::ostream &os, double epsilon) const override {
			ensureInitialized("state printing");
				syncPendingCacheForRead();
				if(epsilon < 0.0) {
					throw std::invalid_argument("BloscStateBackend::printNonZeroStates epsilon must be non-negative");
				}
				const size_t chunkElemsMax = elemsPerChunk();
				std::vector<double> buffer(chunkElemsMax, 0.0);
				StateIndex baseState = 0;
			for(int64_t ci = 0; ci < schunk_->nchunks; ++ci) {
				const size_t chunkElems = std::min(
					chunkElemsMax, totalElems() - static_cast<size_t>(ci) * chunkElemsMax);
				const size_t statesInChunk = chunkElems / 2u;
				readChunk(ci, buffer, chunkElems);
					for(size_t local = 0; local < statesInChunk; ++local) {
						const double real = buffer[local * 2u];
						const double imag = buffer[local * 2u + 1u];
						if(std::abs(real) <= epsilon && std::abs(imag) <= epsilon) continue;
						os << (baseState + static_cast<StateIndex>(local)) << ": "
						   << real << " + " << imag << "i\n";
					}
					baseState += static_cast<StateIndex>(statesInChunk);
				}
			}

			void phaseFlipBasisState(StateIndex state) override {
				ensureInitialized("basis-state phase flip");
				flushAllSlots();
				validateState(state);
				const StateChunkRef chunkRef = locateState(state);
				readChunk(chunkRef.chunkIndex, ioChunkScratch_, chunkRef.chunkElems);
				ioChunkScratch_[chunkRef.elemOffset] = -ioChunkScratch_[chunkRef.elemOffset];
				ioChunkScratch_[chunkRef.elemOffset + 1u] = -ioChunkScratch_[chunkRef.elemOffset + 1u];
				writeChunk(chunkRef.chunkIndex, ioChunkScratch_, chunkRef.chunkElems);
				invalidateGateCache();
			}

		void inversionAboutMean() override {
			ensureInitialized("inversion about mean");
			flushAllSlots();
			if(numStates_ == 0) {
				throw std::logic_error("BloscStateBackend::inversionAboutMean requires a non-empty state");
			}
			double sumReal = 0.0;
			double sumImag = 0.0;
			const size_t chunkElemsMax = elemsPerChunk();
			std::vector<double> buffer(chunkElemsMax, 0.0);
			for(int64_t ci = 0; ci < schunk_->nchunks; ++ci) {
				const size_t chunkElems = std::min(
					chunkElemsMax, totalElems() - static_cast<size_t>(ci) * chunkElemsMax);
				readChunk(ci, buffer, chunkElems);
				for(size_t elem = 0; elem + 1u < chunkElems; elem += 2u) {
					sumReal += buffer[elem];
					sumImag += buffer[elem + 1u];
				}
			}
			const double meanReal = sumReal / static_cast<double>(numStates_);
			const double meanImag = sumImag / static_cast<double>(numStates_);
			for(int64_t ci = 0; ci < schunk_->nchunks; ++ci) {
				const size_t chunkElems = std::min(
					chunkElemsMax, totalElems() - static_cast<size_t>(ci) * chunkElemsMax);
				readChunk(ci, buffer, chunkElems);
				for(size_t elem = 0; elem + 1u < chunkElems; elem += 2u) {
					const double real = buffer[elem];
					const double imag = buffer[elem + 1u];
					buffer[elem] = 2.0 * meanReal - real;
					buffer[elem + 1u] = 2.0 * meanImag - imag;
				}
				writeChunk(ci, buffer, chunkElems);
			}
			invalidateGateCache();
		}

		void applyHadamard(QubitIndex qubit) override {
			validateSingleQubitOperation(qubit, "Hadamard");
			const unsigned int targetMask = qubitMaskFromMsbIndex(qubit, numQubits_);
			const double invSqrt2 = 1.0 / std::sqrt(2.0);
			ensureGateCacheStorage();
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
			flushDirtyCacheIfNeeded();
		}

		void applyPauliX(QubitIndex qubit) override {
			validateSingleQubitOperation(qubit, "PauliX");
			const unsigned int targetMask = qubitMaskFromMsbIndex(qubit, numQubits_);
			ensureGateCacheStorage();
			runPairKernel(targetMask, [&](StateIndex, StateIndex, double *a0, double *a1) -> PairMutation {
				if(a0[0] == a1[0] && a0[1] == a1[1]) {
					return PairMutation::None;
				}
				std::swap(a0[0], a1[0]);
				std::swap(a0[1], a1[1]);
				return PairMutation::Both;
			});
			flushDirtyCacheIfNeeded();
		}

		void applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) override {
			validateTwoQubitOperation(controlQubit, targetQubit, "controlled phase shift");
			const unsigned int controlMask = qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = qubitMaskFromMsbIndex(targetQubit, numQubits_);
			const double phaseReal = std::cos(theta);
			const double phaseImag = std::sin(theta);
			ensureGateCacheStorage();
			runPairKernel(targetMask, [&](StateIndex, StateIndex state1, double *, double *a1) -> PairMutation {
				if((state1 & controlMask) == 0u) {
					return PairMutation::None;
				}
				const double real = a1[0];
				const double imag = a1[1];
				if(real == 0.0 && imag == 0.0) {
					return PairMutation::None;
				}
				a1[0] = phaseReal * real - phaseImag * imag;
				a1[1] = phaseReal * imag + phaseImag * real;
				return PairMutation::Second;
			});
			flushDirtyCacheIfNeeded();
		}

		void applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) override {
			validateTwoQubitOperation(controlQubit, targetQubit, "controlled not");
			const unsigned int controlMask = qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = qubitMaskFromMsbIndex(targetQubit, numQubits_);
			ensureGateCacheStorage();
			runPairKernel(targetMask, [&](StateIndex state0, StateIndex, double *a0, double *a1) -> PairMutation {
				if((state0 & controlMask) == 0u) {
					return PairMutation::None;
				}
				if(a0[0] == a1[0] && a0[1] == a1[1]) {
					return PairMutation::None;
				}
				std::swap(a0[0], a1[0]);
				std::swap(a0[1], a1[1]);
				return PairMutation::Both;
			});
			flushDirtyCacheIfNeeded();
		}

			void applyGate(const QuantumGate &gate, const QubitList &qubits) override {
				validateGateApplicationInputs(gate, qubits);
				if(qubits.size() == 1u) {
					if(matchesHadamardGate(gate)) {
						applyHadamard(qubits[0]);
						return;
					}
					if(matchesPauliXGate(gate)) {
						applyPauliX(qubits[0]);
						return;
					}
				} else if(qubits.size() == 2u) {
					if(matchesControlledNotGate(gate)) {
						applyControlledNot(qubits[0], qubits[1]);
						return;
					}
					double theta = 0.0;
					if(matchControlledPhaseShiftGate(gate, theta)) {
						applyControlledPhaseShift(qubits[0], qubits[1], theta);
						return;
					}
				}
				ensureGateCacheStorage();

				const size_t statesPerChunk = chunkStates();
				auto loadAmplitude = [&](StateIndex state) -> Amplitude {
				const int64_t chunkIndex = static_cast<int64_t>(state / statesPerChunk);
				const size_t offset = static_cast<size_t>(state % statesPerChunk) * 2u;
				const int slot = getSlot(chunkIndex);
				return {gateCache_[static_cast<size_t>(slot)].buffer[offset],
				        gateCache_[static_cast<size_t>(slot)].buffer[offset + 1u]};
			};
			auto storeAmplitude = [&](StateIndex state, Amplitude amp) {
				const int64_t chunkIndex = static_cast<int64_t>(state / statesPerChunk);
				const size_t offset = static_cast<size_t>(state % statesPerChunk) * 2u;
				const int slot = getSlot(chunkIndex);
				gateCache_[static_cast<size_t>(slot)].buffer[offset] = amp.real;
				gateCache_[static_cast<size_t>(slot)].buffer[offset + 1u] = amp.imag;
				gateCache_[static_cast<size_t>(slot)].dirty = true;
			};

			applyGateThroughBlocks(
				"BloscStateBackend::applyGate",
				gate,
				qubits,
				numQubits_,
				loadAmplitude,
					storeAmplitude,
					gateWorkspace_);

				flushDirtyCacheIfNeeded();
			}
};

bool isBloscBackendCompiled() {
	return true;
}

std::unique_ptr<IStateBackend> createBloscBackend(unsigned int numQubits, const RegisterConfig &cfg) {
	(void)numQubits;
	return std::make_unique<BloscStateBackend>(cfg);
}

#else

bool isBloscBackendCompiled() {
	return false;
}

std::unique_ptr<IStateBackend> createBloscBackend(unsigned int, const RegisterConfig &) {
	return nullptr;
}

#endif

} // namespace tmfqs
