#include "tmfqs/storage/zfp/zfp_state_backend.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "tmfqs/core/state_space.h"
#include "tmfqs/storage/common/backend_validation.h"
#include "tmfqs/storage/common/chunk_cache.h"
#include "tmfqs/storage/common/chunk_layout.h"
#include "tmfqs/storage/common/gate_apply_engine.h"
#include "tmfqs/storage/common/pair_kernel_executor.h"
#include "tmfqs/storage/zfp/zfp_codec.h"

namespace tmfqs {

#ifdef HAVE_ZFP
namespace {

/**
 * @brief ZFP-backed state backend with chunk cache for decompressed working set.
 */
class ZfpStateBackend final : public IStateBackend {
	private:
		/** @brief Persistent payload for one compressed chunk. */
		struct ChunkStorage {
			std::vector<uint8_t> compressed;
			size_t elemCount = 0;
		};

		enum class PairMutation : uint8_t {
			None = 0u,
			First = 1u,
			Second = 2u,
			Both = 3u
		};

		struct CacheAmplitudeRef {
			storage::ChunkCache::SlotHandle slot;
			double *amp = nullptr;
		};

		/** @brief Register and configuration metadata. */
		unsigned int numQubits_ = 0;
		unsigned int numStates_ = 0;
		RegisterConfig cfg_;
		/** @brief Codec and chunk geometry. */
		storage::ZfpCodec codec_;
		storage::ChunkLayout layout_;
		/** @brief Compressed chunk storage owned by this backend. */
		std::vector<ChunkStorage> chunks_;
		/** @brief Decompressed working-set cache used for gate application. */
		storage::ChunkCache cache_;
		/** @brief Scratch used for read-only queries when a chunk is not cached. */
		mutable std::vector<double> ioScratch_;
		/** @brief Nesting depth for begin/end batch sections. */
		unsigned int batchDepth_ = 0;
		/** @brief Shared workspace for generic gate application. */
		GateBlockWorkspace gateWorkspace_;

		/** @brief Verifies that backend storage has been configured. */
		void ensureInitialized(const char *operation) const {
			storage::ensureBackendInitialized(numStates_ > 0 && !chunks_.empty(), "ZfpStateBackend", operation);
		}

		/** @brief Tests whether a pair operation modified the first amplitude. */
		bool touchesFirst(PairMutation mutation) const {
			return mutation == PairMutation::First || mutation == PairMutation::Both;
		}

		/** @brief Tests whether a pair operation modified the second amplitude. */
		bool touchesSecond(PairMutation mutation) const {
			return mutation == PairMutation::Second || mutation == PairMutation::Both;
		}

		/** @brief Returns element count stored in one compressed chunk. */
		size_t chunkElemCount(size_t chunkIndex) const {
			return chunks_[chunkIndex].elemCount;
		}

		/** @brief Loads one chunk from compressed storage into cache buffer. */
		void loadChunk(size_t chunkIndex, std::vector<double> &buffer, size_t elemCount) {
			const ChunkStorage &chunk = chunks_[chunkIndex];
			if(chunk.elemCount != elemCount) {
				throw std::logic_error("ZfpStateBackend: chunk size mismatch while loading");
			}
			codec_.decompress(chunk.compressed, elemCount, buffer);
		}

		/** @brief Stores one cache buffer back into compressed chunk storage. */
		void storeChunk(size_t chunkIndex, const std::vector<double> &buffer, size_t elemCount) {
			ChunkStorage &chunk = chunks_[chunkIndex];
			if(chunk.elemCount != elemCount) {
				throw std::logic_error("ZfpStateBackend: chunk size mismatch while storing");
			}
			codec_.compress(buffer.data(), elemCount, chunk.compressed);
		}

		/** @brief Acquires a writable cache slot for one chunk index. */
		storage::ChunkCache::SlotHandle acquireChunk(size_t chunkIndex) {
			return cache_.acquire(
				chunkIndex,
				[&](size_t index, std::vector<double> &buffer, size_t elemCount) { loadChunk(index, buffer, elemCount); },
				[&](size_t index, const std::vector<double> &buffer, size_t elemCount) { storeChunk(index, buffer, elemCount); },
				[&](size_t index) { return chunkElemCount(index); });
		}

		/** @brief Resolves and acquires writable amplitude pointer for one state. */
		CacheAmplitudeRef acquireAmplitudeRef(StateIndex state) {
			const storage::ChunkRef ref = layout_.stateRef(state);
			auto slot = acquireChunk(ref.chunkIndex);
			return {slot, slot.data + ref.elemOffset};
		}

		/** @brief Returns read-only amplitude pointer (cache hit or scratch decode). */
		const double *findAmplitudeData(StateIndex state) const {
			const storage::ChunkRef ref = layout_.stateRef(state);
			if(const std::vector<double> *buffer = cache_.findBuffer(ref.chunkIndex)) {
				return buffer->data() + ref.elemOffset;
			}
			codec_.decompress(chunks_[ref.chunkIndex].compressed, chunks_[ref.chunkIndex].elemCount, ioScratch_);
			return ioScratch_.data() + ref.elemOffset;
		}

		/** @brief Reads amplitude through cache-backed mutable path. */
		Amplitude readAmplitudeMutable(StateIndex state) {
			CacheAmplitudeRef ref = acquireAmplitudeRef(state);
			return {ref.amp[0], ref.amp[1]};
		}

		/** @brief Writes amplitude through cache-backed mutable path. */
		void writeAmplitudeMutable(StateIndex state, Amplitude amp) {
			CacheAmplitudeRef ref = acquireAmplitudeRef(state);
			ref.amp[0] = amp.real;
			ref.amp[1] = amp.imag;
			*ref.slot.dirty = true;
		}

		/** @brief Flushes all dirty cache slots to compressed storage. */
		void flushCache() {
			cache_.flushAll(
				[&](size_t index, const std::vector<double> &buffer, size_t elemCount) { storeChunk(index, buffer, elemCount); },
				[&](size_t index) { return chunkElemCount(index); });
		}

		/** @brief Applies inversion-about-mean with a known mean inside one mutate scope. */
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

		/** @brief Chooses an effective cache size for the current register geometry. */
		size_t effectiveCacheSlots() const {
			const size_t chunkCount = chunks_.size();
			const size_t configuredSlots = cfg_.zfp.gateCacheSlots;
			return std::min(configuredSlots, chunkCount);
		}

		/** @brief Configures chunk layout, storage metadata, and cache topology. */
		void configureStorage(unsigned int numQubits) {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			layout_ = storage::ChunkLayout(static_cast<size_t>(numStates_), cfg_.zfp.chunkStates);
			chunks_.assign(layout_.chunkCount(), ChunkStorage{});
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				chunks_[chunkIndex].elemCount = layout_.chunkElemCount(chunkIndex);
			}
			cache_.configure(effectiveCacheSlots(), chunks_.size());
			batchDepth_ = 0u;
		}

		template <typename MutateFn>
		/** @brief Runs one mutating action under the write-back hot cache. */
		void mutate(const char *operation, MutateFn mutateFn) {
			ensureInitialized(operation);
			mutateFn();
		}

		template <typename PairFn>
		/** @brief Iterates state pairs for one target bit using chunk-aware traversal. */
		void runPairKernel(unsigned int targetMask, PairFn pairFn) {
			// Select traversal that best matches chunk boundaries.
			const storage::PairKernelMode mode = storage::PairKernelExecutor::selectMode(layout_, targetMask);
			if(mode == storage::PairKernelMode::IntraChunk) {
				size_t activeChunk = std::numeric_limits<size_t>::max();
				storage::ChunkCache::SlotHandle activeSlot;
				storage::PairKernelExecutor::runIntraChunk(layout_, targetMask, [&](size_t chunkIndex, size_t local0, size_t local1, StateIndex state0, StateIndex state1) {
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

			if(mode == storage::PairKernelMode::InterChunk && cache_.capacity() >= 2u) {
				size_t chunk0Active = std::numeric_limits<size_t>::max();
				size_t chunk1Active = std::numeric_limits<size_t>::max();
				storage::ChunkCache::SlotHandle slot0;
				storage::ChunkCache::SlotHandle slot1;
				storage::PairKernelExecutor::runInterChunk(layout_, targetMask, [&](size_t chunk0, size_t chunk1, size_t local, StateIndex state0, StateIndex state1) {
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

			storage::PairKernelExecutor::runFallback(numStates_, targetMask, [&](StateIndex state0, StateIndex state1) {
				CacheAmplitudeRef a0Ref = acquireAmplitudeRef(state0);
				CacheAmplitudeRef a1Ref = acquireAmplitudeRef(state1);
				const PairMutation mutation = pairFn(state0, state1, a0Ref.amp, a1Ref.amp);
				if(mutation == PairMutation::None) {
					return;
				}
				if(a0Ref.slot.slotIndex == a1Ref.slot.slotIndex) {
					*a0Ref.slot.dirty = true;
					return;
				}
				if(touchesFirst(mutation)) *a0Ref.slot.dirty = true;
				if(touchesSecond(mutation)) *a1Ref.slot.dirty = true;
			});
		}

	public:
		/** @brief Constructs backend from register configuration. */
		explicit ZfpStateBackend(const RegisterConfig &cfg)
			: cfg_(cfg), codec_(cfg_), cache_(cfg.zfp.gateCacheSlots, 1u) {}

		ZfpStateBackend(const ZfpStateBackend &) = default;
		ZfpStateBackend &operator=(const ZfpStateBackend &) = default;

		/** @brief Clones backend including current compressed state. */
		std::unique_ptr<IStateBackend> clone() const override {
			return std::make_unique<ZfpStateBackend>(*this);
		}

		/** @brief Initializes backend to zero amplitudes. */
		void initZero(unsigned int numQubits) override {
			configureStorage(numQubits);
			std::vector<double> zeroChunk;
			for(ChunkStorage &chunk : chunks_) {
				zeroChunk.assign(chunk.elemCount, 0.0);
				codec_.compress(zeroChunk.data(), zeroChunk.size(), chunk.compressed);
			}
		}

		/** @brief Initializes one basis state with custom amplitude. */
		void initBasis(unsigned int numQubits, StateIndex initState, Amplitude amp) override {
			initZero(numQubits);
			storage::validateBackendStateIndex("ZfpStateBackend::initBasis", initState, numStates_);
			writeAmplitudeMutable(initState, amp);
		}

		/** @brief Initializes equal superposition over selected basis states. */
		void initUniformSuperposition(unsigned int numQubits, const BasisStateList &basisStates) override {
			configureStorage(numQubits);
			std::vector<StateIndex> selected;
			selected.reserve(basisStates.size());
			for(StateIndex state : basisStates) {
				storage::validateBackendStateIndex("ZfpStateBackend::initUniformSuperposition", state, numStates_);
				selected.push_back(state);
			}
			std::sort(selected.begin(), selected.end());
			selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
			if(selected.empty()) {
				throw std::invalid_argument("ZfpStateBackend::initUniformSuperposition requires at least one state");
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
					const size_t state = selected[selectedPos];
					if(state >= chunkStateBegin) {
						const size_t localState = state - chunkStateBegin;
						denseChunk[2u * localState] = ampReal;
					}
					++selectedPos;
				}
				codec_.compress(denseChunk.data(), denseChunk.size(), chunk.compressed);
			}
		}

		/** @brief Loads complete amplitude vector into compressed chunk storage. */
		void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) override {
			configureStorage(numQubits);
			if(amplitudes.size() != checkedAmplitudeElementCount(numQubits_)) {
				throw std::invalid_argument("ZfpStateBackend: amplitudes size mismatch");
			}
			const size_t fullChunkElems = layout_.elemsPerChunk();
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				ChunkStorage &chunk = chunks_[chunkIndex];
				const size_t begin = chunkIndex * fullChunkElems;
				codec_.compress(amplitudes.data() + begin, chunk.elemCount, chunk.compressed);
			}
		}

		/** @brief Reads one basis-state amplitude. */
		Amplitude amplitude(StateIndex state) const override {
			ensureInitialized("amplitude query");
			storage::validateBackendStateIndex("ZfpStateBackend::amplitude", state, numStates_);
			const double *ampData = findAmplitudeData(state);
			return {ampData[0], ampData[1]};
		}

		/** @brief Writes one basis-state amplitude. */
		void setAmplitude(StateIndex state, Amplitude amp) override {
			mutate("state update", [&]() {
				storage::validateBackendStateIndex("ZfpStateBackend::setAmplitude", state, numStates_);
				writeAmplitudeMutable(state, amp);
			});
		}

		/** @brief Computes probability mass for one basis state. */
		double probability(StateIndex state) const override {
			const Amplitude amp = amplitude(state);
			return amp.real * amp.real + amp.imag * amp.imag;
		}

		/** @brief Computes total probability mass across all basis states. */
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
					codec_.decompress(chunks_[chunkIndex].compressed, chunks_[chunkIndex].elemCount, scratch);
					data = scratch.data();
				}
				for(size_t elem = 0; elem < chunks_[chunkIndex].elemCount; elem += 2u) {
					total += data[elem] * data[elem] + data[elem + 1u] * data[elem + 1u];
				}
			}
			return total;
		}

		/** @brief Samples one basis state using cumulative probability scan. */
		StateIndex sampleMeasurement(double rnd) const override {
			ensureInitialized("measurement");
			if(rnd < 0.0 || rnd >= 1.0) {
				throw std::invalid_argument("ZfpStateBackend::sampleMeasurement requires rnd in [0,1)");
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
					codec_.decompress(chunks_[chunkIndex].compressed, chunks_[chunkIndex].elemCount, scratch);
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
			throw std::runtime_error("ZfpStateBackend::sampleMeasurement cumulative probability did not reach sample");
		}

		/** @brief Prints amplitudes above threshold epsilon. */
		void printNonZeroStates(std::ostream &os, double epsilon) const override {
			ensureInitialized("state printing");
			if(epsilon < 0.0) {
				throw std::invalid_argument("ZfpStateBackend::printNonZeroStates epsilon must be non-negative");
			}
			size_t baseState = 0u;
			std::vector<double> scratch;
			for(size_t chunkIndex = 0; chunkIndex < chunks_.size(); ++chunkIndex) {
				const std::vector<double> *cached = cache_.findBuffer(chunkIndex);
				const double *data = nullptr;
				if(cached) {
					data = cached->data();
				} else {
					codec_.decompress(chunks_[chunkIndex].compressed, chunks_[chunkIndex].elemCount, scratch);
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

		/** @brief Applies phase flip to one basis state. */
		void phaseFlipBasisState(StateIndex state) override {
			mutate("basis-state phase flip", [&]() {
				storage::validateBackendStateIndex("ZfpStateBackend::phaseFlipBasisState", state, numStates_);
				CacheAmplitudeRef ref = acquireAmplitudeRef(state);
				ref.amp[0] = -ref.amp[0];
				ref.amp[1] = -ref.amp[1];
				*ref.slot.dirty = true;
			});
		}

		/** @brief Applies inversion-about-mean transform to all amplitudes. */
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

		/** @brief Applies inversion-about-mean transform using a precomputed mean. */
		void inversionAboutMean(Amplitude mean) override {
			mutate("inversion about mean", [&]() { applyInversionAboutMeanWithMean(mean); });
		}

		/** @brief Applies Hadamard gate to one qubit. */
		void applyHadamard(QubitIndex qubit) override {
			ensureInitialized("hadamard application");
			storage::validateBackendSingleQubit("ZfpStateBackend::applyHadamard", qubit, numQubits_);
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(qubit, numQubits_);
			const double invSqrt2 = 1.0 / std::sqrt(2.0);
			mutate("hadamard application", [&]() {
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

		/** @brief Applies Pauli-X gate to one qubit. */
		void applyPauliX(QubitIndex qubit) override {
			ensureInitialized("pauli-x application");
			storage::validateBackendSingleQubit("ZfpStateBackend::applyPauliX", qubit, numQubits_);
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(qubit, numQubits_);
			mutate("pauli-x application", [&]() {
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

		/** @brief Applies controlled phase-shift gate. */
		void applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) override {
			ensureInitialized("controlled phase-shift application");
			storage::validateBackendTwoQubits("ZfpStateBackend::applyControlledPhaseShift", controlQubit, targetQubit, numQubits_);
			const unsigned int controlMask = storage::qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(targetQubit, numQubits_);
			const double phaseReal = std::cos(theta);
			const double phaseImag = std::sin(theta);
			mutate("controlled phase-shift application", [&]() {
				runPairKernel(targetMask, [&](StateIndex, StateIndex state1, double *, double *a1) {
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
			});
		}

		/** @brief Applies controlled-NOT gate. */
		void applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) override {
			ensureInitialized("controlled-not application");
			storage::validateBackendTwoQubits("ZfpStateBackend::applyControlledNot", controlQubit, targetQubit, numQubits_);
			const unsigned int controlMask = storage::qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(targetQubit, numQubits_);
			mutate("controlled-not application", [&]() {
				runPairKernel(targetMask, [&](StateIndex state0, StateIndex, double *a0, double *a1) {
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
			});
		}

		/** @brief Applies an arbitrary gate matrix to selected qubits. */
		void applyGate(const QuantumGate &gate, const QubitList &qubits) override {
			mutate("gate application", [&]() {
				// Adapt cached chunk access to the generic gate application engine.
				auto loadAmplitude = [&](StateIndex state) -> Amplitude { return readAmplitudeMutable(state); };
				auto storeAmplitude = [&](StateIndex state, Amplitude amp) { writeAmplitudeMutable(state, amp); };
				storage::GateApplyEngine::apply(
					"ZfpStateBackend::applyGate",
					gate,
					qubits,
					numQubits_,
					loadAmplitude,
					storeAmplitude,
					gateWorkspace_);
			});
		}

		/** @brief Opens an operation batch scope. */
		void beginOperationBatch() override {
			ensureInitialized("batch begin");
			++batchDepth_;
		}

		/** @brief Closes an operation batch scope and flushes pending writes. */
		void endOperationBatch() override {
			if(batchDepth_ == 0u) {
				throw std::logic_error("ZfpStateBackend::endOperationBatch called without matching beginOperationBatch");
			}
			--batchDepth_;
		}
	};

} // namespace
#endif

/** @brief Reports whether ZFP backend support is available. */
bool isZfpStateBackendAvailable() {
#ifdef HAVE_ZFP
	return true;
#else
	return false;
#endif
}

/** @brief Factory helper for creating a ZFP backend instance. */
std::unique_ptr<IStateBackend> createZfpStateBackend(const RegisterConfig &cfg) {
#ifdef HAVE_ZFP
	return std::make_unique<ZfpStateBackend>(cfg);
#else
	(void)cfg;
	return nullptr;
#endif
}

} // namespace tmfqs
