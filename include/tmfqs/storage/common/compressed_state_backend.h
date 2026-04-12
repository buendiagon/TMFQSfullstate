#ifndef TMFQS_STORAGE_COMMON_COMPRESSED_STATE_BACKEND_H
#define TMFQS_STORAGE_COMMON_COMPRESSED_STATE_BACKEND_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "tmfqs/core/state_space.h"
#include "tmfqs/storage/common/backend_validation.h"
#include "tmfqs/storage/common/chunk_cache.h"
#include "tmfqs/storage/common/chunk_layout.h"
#include "tmfqs/storage/common/compressed_backend_metrics.h"
#include "tmfqs/storage/common/gate_apply_engine.h"
#include "tmfqs/storage/common/pair_kernel_executor.h"
#include "tmfqs/storage/i_state_backend.h"

namespace tmfqs {
namespace storage {

namespace compressed_detail {

constexpr double kGateMatchTolerance = 1e-12;

inline bool approxEqual(double a, double b, double tol = kGateMatchTolerance) {
	return std::abs(a - b) <= tol;
}

inline bool matchesAmplitude(const Amplitude &amp, double real, double imag = 0.0) {
	return approxEqual(amp.real, real) && approxEqual(amp.imag, imag);
}

inline bool isZeroAmplitude(const Amplitude &amp) {
	return matchesAmplitude(amp, 0.0, 0.0);
}

inline bool matchesHadamardGate(const QuantumGate &gate) {
	if(gate.dimension() != 2u) return false;
	const double invSqrt2 = 1.0 / std::sqrt(2.0);
	return matchesAmplitude(gate[0][0], invSqrt2) &&
	       matchesAmplitude(gate[0][1], invSqrt2) &&
	       matchesAmplitude(gate[1][0], invSqrt2) &&
	       matchesAmplitude(gate[1][1], -invSqrt2);
}

inline bool matchesPauliXGate(const QuantumGate &gate) {
	if(gate.dimension() != 2u) return false;
	return isZeroAmplitude(gate[0][0]) &&
	       matchesAmplitude(gate[0][1], 1.0) &&
	       matchesAmplitude(gate[1][0], 1.0) &&
	       isZeroAmplitude(gate[1][1]);
}

inline bool matchesControlledNotGate(const QuantumGate &gate) {
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

inline bool matchControlledPhaseShiftGate(const QuantumGate &gate, double &thetaOut) {
	if(gate.dimension() != 4u) return false;
	for(unsigned int row = 0; row < 4u; ++row) {
		for(unsigned int col = 0; col < 4u; ++col) {
			if(row != col && !isZeroAmplitude(gate[row][col])) return false;
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

} // namespace compressed_detail

template <typename CodecPolicy>
class CompressedStateBackend final : public IStateBackend {
	private:
		struct ChunkStorage {
			std::vector<uint8_t> compressed;
			size_t elemCount = 0u;
		};

		enum class PairMutation : uint8_t {
			None = 0u,
			First = 1u,
			Second = 2u,
			Both = 3u
		};

		struct CacheAmplitudeRef {
			ChunkCache::SlotHandle slot;
			double *amp = nullptr;
		};

		unsigned int numQubits_ = 0;
		unsigned int numStates_ = 0;
		RegisterConfig cfg_;
		CodecPolicy codec_;
		ChunkLayout layout_;
		std::vector<ChunkStorage> chunks_;
		ChunkCache cache_;
		mutable std::vector<double> ioScratch_;
		unsigned int batchDepth_ = 0u;
		GateBlockWorkspace gateWorkspace_;
		CompressedBackendMetrics metrics_;

		void ensureInitialized(const char *operation) const {
			ensureBackendInitialized(numStates_ > 0 && !chunks_.empty(), CodecPolicy::backendName(), operation);
		}

		bool touchesFirst(PairMutation mutation) const {
			return mutation == PairMutation::First || mutation == PairMutation::Both;
		}

		bool touchesSecond(PairMutation mutation) const {
			return mutation == PairMutation::Second || mutation == PairMutation::Both;
		}

		size_t chunkElemCount(size_t chunkIndex) const {
			return chunks_[chunkIndex].elemCount;
		}

		void recordEncode(size_t elemCount, size_t compressedBytes, double seconds) {
			if(!compressedBackendMetricsEnabled()) return;
			++metrics_.encodeCalls;
			metrics_.encodeInputBytes += elemCount * sizeof(double);
			metrics_.encodeOutputBytes += compressedBytes;
			metrics_.encodeSeconds += seconds;
		}

		void recordDecode(size_t compressedBytes, size_t elemCount, double seconds) {
			if(!compressedBackendMetricsEnabled()) return;
			++metrics_.decodeCalls;
			metrics_.decodeInputBytes += compressedBytes;
			metrics_.decodeOutputBytes += elemCount * sizeof(double);
			metrics_.decodeSeconds += seconds;
		}

		void loadChunk(size_t chunkIndex, std::vector<double> &buffer, size_t elemCount) {
			const ChunkStorage &chunk = chunks_[chunkIndex];
			if(chunk.elemCount != elemCount) {
				throw std::logic_error(std::string(CodecPolicy::backendName()) + ": chunk size mismatch while loading");
			}
			const auto start = std::chrono::steady_clock::now();
			codec_.decompress(chunk.compressed, elemCount, buffer);
			recordDecode(chunk.compressed.size(), elemCount, elapsedSecondsSince(start));
			if(compressedBackendMetricsEnabled()) ++metrics_.cacheLoads;
		}

		void storeChunk(size_t chunkIndex, const std::vector<double> &buffer, size_t elemCount) {
			ChunkStorage &chunk = chunks_[chunkIndex];
			if(chunk.elemCount != elemCount) {
				throw std::logic_error(std::string(CodecPolicy::backendName()) + ": chunk size mismatch while storing");
			}
			const auto start = std::chrono::steady_clock::now();
			codec_.compress(buffer.data(), elemCount, chunk.compressed);
			recordEncode(elemCount, chunk.compressed.size(), elapsedSecondsSince(start));
			if(compressedBackendMetricsEnabled()) ++metrics_.cacheStores;
		}

		ChunkCache::SlotHandle acquireChunk(size_t chunkIndex) {
			return cache_.acquire(
				chunkIndex,
				[&](size_t index, std::vector<double> &buffer, size_t elemCount) { loadChunk(index, buffer, elemCount); },
				[&](size_t index, const std::vector<double> &buffer, size_t elemCount) {
					if(compressedBackendMetricsEnabled()) ++metrics_.cacheEvictions;
					storeChunk(index, buffer, elemCount);
				},
				[&](size_t index) { return chunkElemCount(index); });
		}

		CacheAmplitudeRef acquireAmplitudeRef(StateIndex state) {
			const ChunkRef ref = layout_.stateRef(state);
			auto slot = acquireChunk(ref.chunkIndex);
			return {slot, slot.data + ref.elemOffset};
		}

		const double *findAmplitudeData(StateIndex state) const {
			const ChunkRef ref = layout_.stateRef(state);
			if(const std::vector<double> *buffer = cache_.findBuffer(ref.chunkIndex)) {
				return buffer->data() + ref.elemOffset;
			}
			const ChunkStorage &chunk = chunks_[ref.chunkIndex];
			const auto start = std::chrono::steady_clock::now();
			codec_.decompress(chunk.compressed, chunk.elemCount, ioScratch_);
			const_cast<CompressedStateBackend *>(this)->recordDecode(
				chunk.compressed.size(),
				chunk.elemCount,
				elapsedSecondsSince(start));
			return ioScratch_.data() + ref.elemOffset;
		}

		Amplitude readAmplitudeMutable(StateIndex state) {
			CacheAmplitudeRef ref = acquireAmplitudeRef(state);
			return {ref.amp[0], ref.amp[1]};
		}

		void writeAmplitudeMutable(StateIndex state, Amplitude amp) {
			CacheAmplitudeRef ref = acquireAmplitudeRef(state);
			ref.amp[0] = amp.real;
			ref.amp[1] = amp.imag;
			*ref.slot.dirty = true;
		}

		void flushCache() {
			cache_.flushAll(
				[&](size_t index, const std::vector<double> &buffer, size_t elemCount) { storeChunk(index, buffer, elemCount); },
				[&](size_t index) { return chunkElemCount(index); });
			if(compressedBackendMetricsEnabled()) ++metrics_.cacheFlushes;
		}

		void flushCacheIfNeeded() {
			if(batchDepth_ == 0u) {
				flushCache();
			}
		}

		void applyInversionAboutMeanWithMean(Amplitude mean) {
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				auto slot = acquireChunk(chunkIndex);
				for(size_t elem = 0; elem < chunks_[chunkIndex].elemCount; elem += 2u) {
					const double real = slot.data[elem];
					const double imag = slot.data[elem + 1u];
					slot.data[elem] = 2.0 * mean.real - real;
					slot.data[elem + 1u] = 2.0 * mean.imag - imag;
				}
				*slot.dirty = true;
			}
		}

		size_t effectiveCacheSlots() const {
			return std::min(CodecPolicy::cacheSlots(cfg_), chunks_.size());
		}

		void configureStorage(unsigned int numQubits) {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			layout_ = ChunkLayout(static_cast<size_t>(numStates_), CodecPolicy::chunkStates(cfg_));
			chunks_.assign(layout_.chunkCount(), ChunkStorage{});
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				chunks_[chunkIndex].elemCount = layout_.chunkElemCount(chunkIndex);
			}
			cache_.configure(effectiveCacheSlots(), chunks_.size());
			batchDepth_ = 0u;
		}

		template <typename PairFn>
		void runPairKernel(unsigned int targetMask, PairFn pairFn) {
			const PairKernelMode mode = PairKernelExecutor::selectMode(layout_, targetMask);
			if(mode == PairKernelMode::IntraChunk) {
				size_t activeChunk = std::numeric_limits<size_t>::max();
				ChunkCache::SlotHandle activeSlot;
				PairKernelExecutor::runIntraChunk(layout_, targetMask, [&](size_t chunkIndex, size_t local0, size_t local1, StateIndex state0, StateIndex state1) {
					if(activeChunk != chunkIndex) {
						activeSlot = acquireChunk(chunkIndex);
						activeChunk = chunkIndex;
					}
					double *a0 = activeSlot.data + local0 * 2u;
					double *a1 = activeSlot.data + local1 * 2u;
					if(pairFn(state0, state1, a0, a1) != PairMutation::None) {
						*activeSlot.dirty = true;
					}
				});
				return;
			}

			if(mode == PairKernelMode::InterChunk && cache_.capacity() >= 2u) {
				size_t chunk0Active = std::numeric_limits<size_t>::max();
				size_t chunk1Active = std::numeric_limits<size_t>::max();
				ChunkCache::SlotHandle slot0;
				ChunkCache::SlotHandle slot1;
				PairKernelExecutor::runInterChunk(layout_, targetMask, [&](size_t chunk0, size_t chunk1, size_t local, StateIndex state0, StateIndex state1) {
					if(chunk0Active != chunk0) {
						slot0 = acquireChunk(chunk0);
						chunk0Active = chunk0;
					}
					if(chunk1Active != chunk1) {
						slot1 = acquireChunk(chunk1);
						chunk1Active = chunk1;
					}
					double *a0 = slot0.data + local * 2u;
					double *a1 = slot1.data + local * 2u;
					const PairMutation mutation = pairFn(state0, state1, a0, a1);
					if(touchesFirst(mutation)) *slot0.dirty = true;
					if(touchesSecond(mutation)) *slot1.dirty = true;
				});
				return;
			}

			PairKernelExecutor::runFallback(numStates_, targetMask, [&](StateIndex state0, StateIndex state1) {
				CacheAmplitudeRef a0Ref = acquireAmplitudeRef(state0);
				CacheAmplitudeRef a1Ref = acquireAmplitudeRef(state1);
				const PairMutation mutation = pairFn(state0, state1, a0Ref.amp, a1Ref.amp);
				if(mutation == PairMutation::None) return;
				if(a0Ref.slot.slotIndex == a1Ref.slot.slotIndex) {
					*a0Ref.slot.dirty = true;
					return;
				}
				if(touchesFirst(mutation)) *a0Ref.slot.dirty = true;
				if(touchesSecond(mutation)) *a1Ref.slot.dirty = true;
			});
		}

		template <typename MutateFn>
		void mutate(const char *operation, MutateFn mutateFn) {
			ensureInitialized(operation);
			mutateFn();
			flushCacheIfNeeded();
		}

	public:
		explicit CompressedStateBackend(const RegisterConfig &cfg)
			: cfg_(cfg), codec_(cfg), cache_(CodecPolicy::cacheSlots(cfg), 1u) {}

		CompressedStateBackend(const CompressedStateBackend &) = default;
		CompressedStateBackend &operator=(const CompressedStateBackend &) = default;

		~CompressedStateBackend() override {
			printCompressedBackendMetrics(CodecPolicy::metricsName(), metrics_);
		}

		std::unique_ptr<IStateBackend> clone() const override {
			return std::make_unique<CompressedStateBackend>(*this);
		}

		void beginOperationBatch() override {
			ensureInitialized("batch begin");
			++batchDepth_;
		}

		void endOperationBatch() override {
			if(batchDepth_ == 0u) {
				throw std::logic_error(std::string(CodecPolicy::backendName()) + "::endOperationBatch called without matching beginOperationBatch");
			}
			--batchDepth_;
			if(batchDepth_ == 0u) {
				flushCache();
			}
		}

		void initZero(unsigned int numQubits) override {
			configureStorage(numQubits);
			std::vector<double> zeroChunk;
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				ChunkStorage &chunk = chunks_[chunkIndex];
				zeroChunk.assign(chunk.elemCount, 0.0);
				storeChunk(chunkIndex, zeroChunk, chunk.elemCount);
			}
		}

		void initBasis(unsigned int numQubits, StateIndex initState, Amplitude amp) override {
			initZero(numQubits);
			validateBackendStateIndex((std::string(CodecPolicy::backendName()) + "::initBasis").c_str(), initState, numStates_);
			writeAmplitudeMutable(initState, amp);
			flushCacheIfNeeded();
		}

		void initUniformSuperposition(unsigned int numQubits, const BasisStateList &basisStates) override {
			configureStorage(numQubits);
			std::vector<StateIndex> selected;
			selected.reserve(basisStates.size());
			for(StateIndex state : basisStates) {
				validateBackendStateIndex((std::string(CodecPolicy::backendName()) + "::initUniformSuperposition").c_str(), state, numStates_);
				selected.push_back(state);
			}
			std::sort(selected.begin(), selected.end());
			selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
			if(selected.empty()) {
				throw std::invalid_argument(std::string(CodecPolicy::backendName()) + "::initUniformSuperposition requires at least one state");
			}

			const double ampReal = 1.0 / std::sqrt(static_cast<double>(selected.size()));
			std::vector<double> denseChunk;
			size_t selectedPos = 0u;
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				ChunkStorage &chunk = chunks_[chunkIndex];
				denseChunk.assign(chunk.elemCount, 0.0);
				const size_t chunkStateBegin = layout_.chunkStateBegin(chunkIndex);
				const size_t statesInChunk = chunk.elemCount / 2u;
				const size_t chunkStateEnd = chunkStateBegin + statesInChunk;
				while(selectedPos < selected.size() && static_cast<size_t>(selected[selectedPos]) < chunkStateEnd) {
					if(static_cast<size_t>(selected[selectedPos]) >= chunkStateBegin) {
						const size_t localState = static_cast<size_t>(selected[selectedPos]) - chunkStateBegin;
						denseChunk[2u * localState] = ampReal;
					}
					++selectedPos;
				}
				storeChunk(chunkIndex, denseChunk, chunk.elemCount);
			}
		}

		void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) override {
			configureStorage(numQubits);
			if(amplitudes.size() != checkedAmplitudeElementCount(numQubits_)) {
				throw std::invalid_argument(std::string(CodecPolicy::backendName()) + ": amplitudes size mismatch");
			}
			cache_.clear();
			const size_t fullChunkElems = layout_.elemsPerChunk();
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				ChunkStorage &chunk = chunks_[chunkIndex];
				const size_t begin = chunkIndex * fullChunkElems;
				const auto start = std::chrono::steady_clock::now();
				codec_.compress(amplitudes.data() + begin, chunk.elemCount, chunk.compressed);
				recordEncode(chunk.elemCount, chunk.compressed.size(), elapsedSecondsSince(start));
			}
		}

		void exportAmplitudes(AmplitudesVector &out) const override {
			ensureInitialized("amplitude export");
			out.assign(checkedAmplitudeElementCount(numQubits_), 0.0);
			std::vector<double> scratch;
			size_t baseElem = 0u;
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				const std::vector<double> *cached = cache_.findBuffer(chunkIndex);
				const double *data = nullptr;
				const size_t elemCount = chunks_[chunkIndex].elemCount;
				if(cached) {
					data = cached->data();
				} else {
					const auto start = std::chrono::steady_clock::now();
					codec_.decompress(chunks_[chunkIndex].compressed, elemCount, scratch);
					const_cast<CompressedStateBackend *>(this)->recordDecode(
						chunks_[chunkIndex].compressed.size(),
						elemCount,
						elapsedSecondsSince(start));
					data = scratch.data();
				}
				std::copy(data, data + elemCount, out.begin() + static_cast<std::ptrdiff_t>(baseElem));
				baseElem += elemCount;
			}
		}

		void forEachAmplitudeChunk(const AmplitudeChunkVisitor &visitor) const override {
			ensureInitialized("amplitude chunk iteration");
			std::vector<double> scratch;
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				const std::vector<double> *cached = cache_.findBuffer(chunkIndex);
				const double *data = nullptr;
				const size_t elemCount = chunks_[chunkIndex].elemCount;
				if(cached) {
					data = cached->data();
				} else {
					const auto start = std::chrono::steady_clock::now();
					codec_.decompress(chunks_[chunkIndex].compressed, elemCount, scratch);
					const_cast<CompressedStateBackend *>(this)->recordDecode(
						chunks_[chunkIndex].compressed.size(),
						elemCount,
						elapsedSecondsSince(start));
					data = scratch.data();
				}
				visitor(static_cast<StateIndex>(layout_.chunkStateBegin(chunkIndex)), data, elemCount);
			}
		}

		Amplitude amplitude(StateIndex state) const override {
			ensureInitialized("amplitude query");
			validateBackendStateIndex((std::string(CodecPolicy::backendName()) + "::amplitude").c_str(), state, numStates_);
			const double *ampData = findAmplitudeData(state);
			return {ampData[0], ampData[1]};
		}

		void setAmplitude(StateIndex state, Amplitude amp) override {
			mutate("state update", [&]() {
				validateBackendStateIndex((std::string(CodecPolicy::backendName()) + "::setAmplitude").c_str(), state, numStates_);
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
				const std::vector<double> *cached = cache_.findBuffer(chunkIndex);
				const double *data = nullptr;
				if(cached) {
					data = cached->data();
				} else {
					const auto start = std::chrono::steady_clock::now();
					codec_.decompress(chunks_[chunkIndex].compressed, chunks_[chunkIndex].elemCount, scratch);
					const_cast<CompressedStateBackend *>(this)->recordDecode(
						chunks_[chunkIndex].compressed.size(),
						chunks_[chunkIndex].elemCount,
						elapsedSecondsSince(start));
					data = scratch.data();
				}
				for(size_t elem = 0; elem < chunks_[chunkIndex].elemCount; elem += 2u) {
					total += data[elem] * data[elem] + data[elem + 1u] * data[elem + 1u];
				}
			}
			return total;
		}

		StateIndex sampleMeasurement(double rnd) const override {
			ensureInitialized("measurement");
			if(rnd < 0.0 || rnd >= 1.0) {
				throw std::invalid_argument(std::string(CodecPolicy::backendName()) + "::sampleMeasurement requires rnd in [0,1)");
			}
			double cumulative = 0.0;
			size_t baseState = 0u;
			std::vector<double> scratch;
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				const std::vector<double> *cached = cache_.findBuffer(chunkIndex);
				const double *data = nullptr;
				if(cached) {
					data = cached->data();
				} else {
					const auto start = std::chrono::steady_clock::now();
					codec_.decompress(chunks_[chunkIndex].compressed, chunks_[chunkIndex].elemCount, scratch);
					const_cast<CompressedStateBackend *>(this)->recordDecode(
						chunks_[chunkIndex].compressed.size(),
						chunks_[chunkIndex].elemCount,
						elapsedSecondsSince(start));
					data = scratch.data();
				}
				const size_t statesInChunk = chunks_[chunkIndex].elemCount / 2u;
				for(size_t localState = 0; localState < statesInChunk; ++localState) {
					const size_t elem = localState * 2u;
					cumulative += data[elem] * data[elem] + data[elem + 1u] * data[elem + 1u];
					if(rnd <= cumulative) {
						return static_cast<StateIndex>(baseState + localState);
					}
				}
				baseState += statesInChunk;
			}
			throw std::runtime_error(std::string(CodecPolicy::backendName()) + "::sampleMeasurement cumulative probability did not reach sample");
		}

		void printNonZeroStates(std::ostream &os, double epsilon) const override {
			ensureInitialized("state printing");
			if(epsilon < 0.0) {
				throw std::invalid_argument(std::string(CodecPolicy::backendName()) + "::printNonZeroStates epsilon must be non-negative");
			}
			size_t baseState = 0u;
			std::vector<double> scratch;
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				const std::vector<double> *cached = cache_.findBuffer(chunkIndex);
				const double *data = nullptr;
				if(cached) {
					data = cached->data();
				} else {
					const auto start = std::chrono::steady_clock::now();
					codec_.decompress(chunks_[chunkIndex].compressed, chunks_[chunkIndex].elemCount, scratch);
					const_cast<CompressedStateBackend *>(this)->recordDecode(
						chunks_[chunkIndex].compressed.size(),
						chunks_[chunkIndex].elemCount,
						elapsedSecondsSince(start));
					data = scratch.data();
				}
				const size_t statesInChunk = chunks_[chunkIndex].elemCount / 2u;
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
				validateBackendStateIndex((std::string(CodecPolicy::backendName()) + "::phaseFlipBasisState").c_str(), state, numStates_);
				CacheAmplitudeRef ref = acquireAmplitudeRef(state);
				ref.amp[0] = -ref.amp[0];
				ref.amp[1] = -ref.amp[1];
				*ref.slot.dirty = true;
			});
		}

		void inversionAboutMean() override {
			mutate("inversion about mean", [&]() {
				double sumReal = 0.0;
				double sumImag = 0.0;
				for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
					auto slot = acquireChunk(chunkIndex);
					for(size_t elem = 0; elem < chunks_[chunkIndex].elemCount; elem += 2u) {
						sumReal += slot.data[elem];
						sumImag += slot.data[elem + 1u];
					}
				}
				applyInversionAboutMeanWithMean({
					sumReal / static_cast<double>(numStates_),
					sumImag / static_cast<double>(numStates_)
				});
			});
		}

		void inversionAboutMean(Amplitude mean) override {
			mutate("inversion about mean", [&]() { applyInversionAboutMeanWithMean(mean); });
		}

		void applyHadamard(QubitIndex qubit) override {
			ensureInitialized("Hadamard");
			validateBackendSingleQubit((std::string(CodecPolicy::backendName()) + "::applyHadamard").c_str(), qubit, numQubits_);
			const unsigned int targetMask = qubitMaskFromMsbIndex(qubit, numQubits_);
			const double invSqrt2 = 1.0 / std::sqrt(2.0);
			mutate("Hadamard", [&]() {
				runPairKernel(targetMask, [&](StateIndex, StateIndex, double *a0, double *a1) {
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
			ensureInitialized("PauliX");
			validateBackendSingleQubit((std::string(CodecPolicy::backendName()) + "::applyPauliX").c_str(), qubit, numQubits_);
			const unsigned int targetMask = qubitMaskFromMsbIndex(qubit, numQubits_);
			mutate("PauliX", [&]() {
				runPairKernel(targetMask, [&](StateIndex, StateIndex, double *a0, double *a1) {
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
			ensureInitialized("controlled phase shift");
			validateBackendTwoQubits((std::string(CodecPolicy::backendName()) + "::applyControlledPhaseShift").c_str(), controlQubit, targetQubit, numQubits_);
			const unsigned int controlMask = qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = qubitMaskFromMsbIndex(targetQubit, numQubits_);
			const double phaseReal = std::cos(theta);
			const double phaseImag = std::sin(theta);
			mutate("controlled phase shift", [&]() {
				runPairKernel(targetMask, [&](StateIndex, StateIndex state1, double *, double *a1) {
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
			ensureInitialized("controlled not");
			validateBackendTwoQubits((std::string(CodecPolicy::backendName()) + "::applyControlledNot").c_str(), controlQubit, targetQubit, numQubits_);
			const unsigned int controlMask = qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = qubitMaskFromMsbIndex(targetQubit, numQubits_);
			mutate("controlled not", [&]() {
				runPairKernel(targetMask, [&](StateIndex state0, StateIndex, double *a0, double *a1) {
					if((state0 & controlMask) == 0u) return PairMutation::None;
					if(a0[0] == a1[0] && a0[1] == a1[1]) return PairMutation::None;
					std::swap(a0[0], a1[0]);
					std::swap(a0[1], a1[1]);
					return PairMutation::Both;
				});
			});
		}

		void applyGate(const QuantumGate &gate, const QubitList &qubits) override {
			ensureInitialized("gate application");
			if(qubits.size() == 1u) {
				if(compressed_detail::matchesHadamardGate(gate)) {
					applyHadamard(qubits[0]);
					return;
				}
				if(compressed_detail::matchesPauliXGate(gate)) {
					applyPauliX(qubits[0]);
					return;
				}
			} else if(qubits.size() == 2u) {
				if(compressed_detail::matchesControlledNotGate(gate)) {
					applyControlledNot(qubits[0], qubits[1]);
					return;
				}
				double theta = 0.0;
				if(compressed_detail::matchControlledPhaseShiftGate(gate, theta)) {
					applyControlledPhaseShift(qubits[0], qubits[1], theta);
					return;
				}
			}

			mutate("gate application", [&]() {
				auto loadAmplitude = [&](StateIndex state) -> Amplitude { return readAmplitudeMutable(state); };
				auto storeAmplitude = [&](StateIndex state, Amplitude amp) { writeAmplitudeMutable(state, amp); };
				GateApplyEngine::apply(
					CodecPolicy::backendName(),
					gate,
					qubits,
					numQubits_,
					loadAmplitude,
					storeAmplitude,
					gateWorkspace_);
			});
		}
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_COMMON_COMPRESSED_STATE_BACKEND_H
