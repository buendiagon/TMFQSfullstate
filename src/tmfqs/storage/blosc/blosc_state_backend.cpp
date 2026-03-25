#include "tmfqs/storage/blosc/blosc_state_backend.h"

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
#include "tmfqs/storage/blosc/blosc_codec.h"
#include "tmfqs/storage/common/backend_validation.h"
#include "tmfqs/storage/common/chunk_cache.h"
#include "tmfqs/storage/common/chunk_layout.h"
#include "tmfqs/storage/common/gate_apply_engine.h"
#include "tmfqs/storage/common/pair_kernel_executor.h"

namespace tmfqs {

#ifdef HAVE_BLOSC2
namespace {

constexpr double kGateMatchTolerance = 1e-12;

/**
 * @brief Approximate floating-point comparison helper used in gate matching.
 */
bool approxEqual(double a, double b, double tol = kGateMatchTolerance) {
	return std::abs(a - b) <= tol;
}

/** @brief Checks whether an amplitude matches target complex value within tolerance. */
bool matchesAmplitude(const Amplitude &amp, double real, double imag = 0.0) {
	return approxEqual(amp.real, real) && approxEqual(amp.imag, imag);
}

/** @brief Checks whether an amplitude is approximately zero. */
bool isZeroAmplitude(const Amplitude &amp) {
	return matchesAmplitude(amp, 0.0, 0.0);
}

/** @brief Detects canonical single-qubit Hadamard matrix. */
bool matchesHadamardGate(const QuantumGate &gate) {
	if(gate.dimension() != 2u) return false;
	const double invSqrt2 = 1.0 / std::sqrt(2.0);
	return matchesAmplitude(gate[0][0], invSqrt2) &&
	       matchesAmplitude(gate[0][1], invSqrt2) &&
	       matchesAmplitude(gate[1][0], invSqrt2) &&
	       matchesAmplitude(gate[1][1], -invSqrt2);
}

/** @brief Detects canonical single-qubit Pauli-X matrix. */
bool matchesPauliXGate(const QuantumGate &gate) {
	if(gate.dimension() != 2u) return false;
	return isZeroAmplitude(gate[0][0]) &&
	       matchesAmplitude(gate[0][1], 1.0) &&
	       matchesAmplitude(gate[1][0], 1.0) &&
	       isZeroAmplitude(gate[1][1]);
}

/** @brief Detects canonical two-qubit controlled-NOT matrix. */
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

/** @brief Detects controlled phase-shift matrix and extracts phase angle. */
bool matchControlledPhaseShiftGate(const QuantumGate &gate, double &thetaOut) {
	if(gate.dimension() != 4u) return false;
	for(unsigned int row = 0; row < 4u; ++row) {
		for(unsigned int col = 0; col < 4u; ++col) {
			if(row != col && !isZeroAmplitude(gate[row][col])) {
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

/**
 * @brief Blosc-backed state backend with chunk cache for decompressed working set.
 */
class BloscStateBackend final : public IStateBackend {
	private:
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
		/** @brief Compression/storage and chunk geometry state. */
		storage::BloscCodec codec_;
		storage::ChunkLayout layout_;
		/** @brief Decompressed working-set cache used for mutations. */
		storage::ChunkCache cache_;
		/** @brief Scratch used for read-only access when a chunk is not cached. */
		mutable std::vector<double> ioScratch_;
		/** @brief Nesting depth for begin/end batch sections. */
		unsigned int batchDepth_ = 0;
		/** @brief Shared workspace for generic gate application. */
		GateBlockWorkspace gateWorkspace_;

		/** @brief Verifies that backend storage has been configured. */
		void ensureInitialized(const char *operation) const {
			storage::ensureBackendInitialized(codec_.hasStorage(), "BloscStateBackend", operation);
		}

		/** @brief Tests whether a pair operation modified the first amplitude. */
		bool touchesFirst(PairMutation mutation) const {
			return mutation == PairMutation::First || mutation == PairMutation::Both;
		}

		/** @brief Tests whether a pair operation modified the second amplitude. */
		bool touchesSecond(PairMutation mutation) const {
			return mutation == PairMutation::Second || mutation == PairMutation::Both;
		}

		/** @brief Returns element count stored in one chunk. */
		size_t chunkElemCount(size_t chunkIndex) const {
			return layout_.chunkElemCount(chunkIndex);
		}

		/** @brief Loads one chunk into a cache buffer. */
		void loadChunk(size_t chunkIndex, std::vector<double> &buffer, size_t elemCount) {
			codec_.readChunk(chunkIndex, buffer, elemCount);
		}

		/** @brief Stores one cache buffer back into compressed storage. */
		void storeChunk(size_t chunkIndex, const std::vector<double> &buffer, size_t elemCount) {
			codec_.writeChunk(chunkIndex, buffer, elemCount);
		}

		/** @brief Acquires writable cache slot for one chunk index. */
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
			codec_.readChunk(ref.chunkIndex, ioScratch_, chunkElemCount(ref.chunkIndex));
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

		/** @brief Flushes cache only when not inside an operation batch. */
		void flushCacheIfNeeded() {
			if(batchDepth_ == 0u) {
				flushCache();
			}
		}

		/** @brief Applies inversion-about-mean with a known mean inside one mutate scope. */
		void applyInversionAboutMeanWithMean(Amplitude mean) {
			for(size_t chunkIndex = 0; chunkIndex < layout_.chunkCount(); ++chunkIndex) {
				auto slot = acquireChunk(chunkIndex);
				const size_t elemCount = chunkElemCount(chunkIndex);
				for(size_t elem = 0; elem + 1u < elemCount; elem += 2u) {
					const double real = slot.data[elem];
					const double imag = slot.data[elem + 1u];
					slot.data[elem] = 2.0 * mean.real - real;
					slot.data[elem + 1u] = 2.0 * mean.imag - imag;
				}
				*slot.dirty = true;
			}
		}

		/** @brief Configures chunk layout, codec storage, and cache topology. */
		void configureStorage(unsigned int numQubits) {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			layout_ = storage::ChunkLayout(static_cast<size_t>(numStates_), cfg_.blosc.chunkStates);
			codec_.resetStorage();
			cache_.configure(cfg_.blosc.gateCacheSlots, layout_.chunkCount());
			batchDepth_ = 0u;
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

		template <typename MutateFn>
		/** @brief Runs one mutating action and conditionally flushes writes. */
		void mutate(const char *operation, MutateFn mutateFn) {
			ensureInitialized(operation);
			mutateFn();
			// Outside batches, keep compressed storage immediately consistent.
			flushCacheIfNeeded();
		}

	public:
		/** @brief Constructs backend from register configuration. */
		explicit BloscStateBackend(const RegisterConfig &cfg)
			: cfg_(cfg), codec_(cfg_), cache_(cfg.blosc.gateCacheSlots, 1u) {}

		BloscStateBackend(const BloscStateBackend &) = default;
		BloscStateBackend &operator=(const BloscStateBackend &) = default;

		/** @brief Clones backend including current compressed state. */
		std::unique_ptr<IStateBackend> clone() const override {
			return std::make_unique<BloscStateBackend>(*this);
		}

		/** @brief Opens an operation batch scope. */
		void beginOperationBatch() override {
			ensureInitialized("batch begin");
			++batchDepth_;
		}

		/** @brief Closes an operation batch scope and flushes pending writes. */
		void endOperationBatch() override {
			if(batchDepth_ == 0u) {
				throw std::logic_error("BloscStateBackend::endOperationBatch called without matching beginOperationBatch");
			}
			--batchDepth_;
			if(batchDepth_ == 0u) {
				flushCache();
			}
		}

		/** @brief Initializes backend to zero amplitudes. */
		void initZero(unsigned int numQubits) override {
			configureStorage(numQubits);
			const size_t statesPerChunk = layout_.statesPerChunk();
			const size_t chunkElems = layout_.elemsPerChunk();
			std::vector<double> chunkBuffer(chunkElems, 0.0);
			for(size_t base = 0; base < numStates_; base += statesPerChunk) {
				const size_t statesInChunk = std::min(statesPerChunk, static_cast<size_t>(numStates_) - base);
				codec_.appendChunk(chunkBuffer.data(), statesInChunk * 2u, "BloscStateBackend::initZero");
			}
		}

		/** @brief Initializes one basis state with custom amplitude. */
		void initBasis(unsigned int numQubits, StateIndex initState, Amplitude amp) override {
			initZero(numQubits);
			storage::validateBackendStateIndex("BloscStateBackend::initBasis", initState, numStates_);
			setAmplitude(initState, amp);
		}

		/** @brief Initializes equal superposition over selected basis states. */
		void initUniformSuperposition(unsigned int numQubits, const BasisStateList &basisStates) override {
			const unsigned int stateCount = checkedStateCount(numQubits);
			std::vector<StateIndex> selected;
			selected.reserve(basisStates.size());
			for(StateIndex state : basisStates) {
				storage::validateBackendStateIndex("BloscStateBackend::initUniformSuperposition", state, stateCount);
				selected.push_back(state);
			}
			std::sort(selected.begin(), selected.end());
			selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
			if(selected.empty()) {
				throw std::invalid_argument("BloscStateBackend: no valid basis states for uniform initialization");
			}

			configureStorage(numQubits);
			const double realAmplitude = 1.0 / std::sqrt(static_cast<double>(selected.size()));
			const size_t chunkElemsMax = layout_.elemsPerChunk();
			std::vector<double> chunkBuffer(chunkElemsMax, 0.0);

			size_t selectedIndex = 0;
			for(size_t baseState = 0; baseState < numStates_; baseState += layout_.statesPerChunk()) {
				const size_t statesInChunk = std::min(layout_.statesPerChunk(), static_cast<size_t>(numStates_) - baseState);
				const size_t elemsInChunk = statesInChunk * 2u;
				std::fill_n(chunkBuffer.begin(), elemsInChunk, 0.0);
				const size_t chunkEnd = baseState + statesInChunk;
				while(selectedIndex < selected.size() && selected[selectedIndex] < chunkEnd) {
					const size_t localState = static_cast<size_t>(selected[selectedIndex]) - baseState;
					chunkBuffer[localState * 2u] = realAmplitude;
					++selectedIndex;
				}
				codec_.appendChunk(chunkBuffer.data(), elemsInChunk, "BloscStateBackend::initUniformSuperposition");
			}
		}

		/** @brief Loads complete amplitude vector into compressed chunk storage. */
		void loadAmplitudes(unsigned int numQubits, const AmplitudesVector &amplitudes) override {
			const unsigned int stateCount = checkedStateCount(numQubits);
			if(amplitudes.size() != checkedAmplitudeElementCount(numQubits)) {
				throw std::invalid_argument("BloscStateBackend: amplitudes size mismatch");
			}
			configureStorage(numQubits);
			if(stateCount != numStates_) {
				throw std::logic_error("BloscStateBackend: state-count mismatch while loading amplitudes");
			}

			for(size_t baseState = 0; baseState < numStates_; baseState += layout_.statesPerChunk()) {
				const size_t statesInChunk = std::min(layout_.statesPerChunk(), static_cast<size_t>(numStates_) - baseState);
				codec_.appendChunk(amplitudes.data() + baseState * 2u, statesInChunk * 2u, "BloscStateBackend::loadAmplitudes");
			}
		}

		/** @brief Exports the full register by decoding each chunk once. */
		AmplitudesVector exportAmplitudes() const override {
			ensureInitialized("amplitude export");
			AmplitudesVector amplitudes(checkedAmplitudeElementCount(numQubits_), 0.0);
			std::vector<double> buffer;
			size_t dstOffset = 0u;
			for(size_t chunkIndex = 0; chunkIndex < layout_.chunkCount(); ++chunkIndex) {
				const size_t elemCount = chunkElemCount(chunkIndex);
				const std::vector<double> *cached = cache_.findBuffer(chunkIndex);
				const double *data = nullptr;
				if(cached) {
					data = cached->data();
				} else {
					codec_.readChunk(chunkIndex, buffer, elemCount);
					data = buffer.data();
				}
				std::copy_n(data, elemCount, amplitudes.data() + dstOffset);
				dstOffset += elemCount;
			}
			return amplitudes;
		}

		/** @brief Returns the number of chunk tiles in compressed storage. */
		size_t tileCount() const override {
			ensureInitialized("tile count query");
			return layout_.chunkCount();
		}

		/** @brief Reads one compressed chunk tile. */
		void readTile(size_t tileIndex, AmplitudesVector &amplitudes) const override {
			ensureInitialized("tile read");
			const size_t elemCount = chunkElemCount(tileIndex);
			if(const std::vector<double> *cached = cache_.findBuffer(tileIndex)) {
				amplitudes = *cached;
				return;
			}
			codec_.readChunk(tileIndex, amplitudes, elemCount);
		}

		/** @brief Writes one compressed chunk tile through the cache path. */
		void writeTile(size_t tileIndex, const AmplitudesVector &amplitudes) override {
			ensureInitialized("tile write");
			const size_t elemCount = chunkElemCount(tileIndex);
			if(amplitudes.size() != elemCount) {
				throw std::invalid_argument("BloscStateBackend::writeTile amplitudes size mismatch");
			}
			auto slot = acquireChunk(tileIndex);
			std::copy(amplitudes.begin(), amplitudes.end(), slot.data);
			*slot.dirty = true;
			flushCacheIfNeeded();
		}

		/** @brief Reads one basis-state amplitude. */
		Amplitude amplitude(StateIndex state) const override {
			ensureInitialized("amplitude query");
			storage::validateBackendStateIndex("BloscStateBackend::amplitude", state, numStates_);
			const double *ampData = findAmplitudeData(state);
			return {ampData[0], ampData[1]};
		}

		/** @brief Writes one basis-state amplitude. */
		void setAmplitude(StateIndex state, Amplitude amp) override {
			mutate("state update", [&]() {
				storage::validateBackendStateIndex("BloscStateBackend::setAmplitude", state, numStates_);
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
			double sum = 0.0;
			std::vector<double> buffer;
			for(size_t chunkIndex = 0; chunkIndex < layout_.chunkCount(); ++chunkIndex) {
				const std::vector<double> *cached = cache_.findBuffer(chunkIndex);
				const double *data = nullptr;
				const size_t elemCount = chunkElemCount(chunkIndex);
				if(cached) {
					data = cached->data();
				} else {
					codec_.readChunk(chunkIndex, buffer, elemCount);
					data = buffer.data();
				}
				for(size_t elem = 0; elem + 1u < elemCount; elem += 2u) {
					sum += data[elem] * data[elem] + data[elem + 1u] * data[elem + 1u];
				}
			}
			return sum;
		}

		/** @brief Samples one basis state using cumulative probability scan. */
		StateIndex sampleMeasurement(double rnd) const override {
			ensureInitialized("measurement");
			if(rnd < 0.0 || rnd >= 1.0) {
				throw std::invalid_argument("BloscStateBackend::sampleMeasurement requires rnd in [0,1)");
			}
			double cumulative = 0.0;
			StateIndex baseState = 0;
			std::vector<double> buffer;
			for(size_t chunkIndex = 0; chunkIndex < layout_.chunkCount(); ++chunkIndex) {
				const std::vector<double> *cached = cache_.findBuffer(chunkIndex);
				const double *data = nullptr;
				const size_t elemCount = chunkElemCount(chunkIndex);
				if(cached) {
					data = cached->data();
				} else {
					codec_.readChunk(chunkIndex, buffer, elemCount);
					data = buffer.data();
				}
				const size_t statesInChunk = elemCount / 2u;
				for(size_t local = 0; local < statesInChunk; ++local) {
					const double real = data[local * 2u];
					const double imag = data[local * 2u + 1u];
					cumulative += real * real + imag * imag;
					if(rnd <= cumulative) {
						return baseState + static_cast<StateIndex>(local);
					}
				}
				baseState += static_cast<StateIndex>(statesInChunk);
			}
			throw std::runtime_error("BloscStateBackend::sampleMeasurement cumulative probability did not reach sample");
		}

		/** @brief Prints amplitudes above threshold epsilon. */
		void printNonZeroStates(std::ostream &os, double epsilon) const override {
			ensureInitialized("state printing");
			if(epsilon < 0.0) {
				throw std::invalid_argument("BloscStateBackend::printNonZeroStates epsilon must be non-negative");
			}
			StateIndex baseState = 0;
			std::vector<double> buffer;
			for(size_t chunkIndex = 0; chunkIndex < layout_.chunkCount(); ++chunkIndex) {
				const std::vector<double> *cached = cache_.findBuffer(chunkIndex);
				const double *data = nullptr;
				const size_t elemCount = chunkElemCount(chunkIndex);
				if(cached) {
					data = cached->data();
				} else {
					codec_.readChunk(chunkIndex, buffer, elemCount);
					data = buffer.data();
				}
				const size_t statesInChunk = elemCount / 2u;
				for(size_t local = 0; local < statesInChunk; ++local) {
					const double real = data[local * 2u];
					const double imag = data[local * 2u + 1u];
					if(std::abs(real) <= epsilon && std::abs(imag) <= epsilon) continue;
					os << (baseState + static_cast<StateIndex>(local)) << ": "
					   << real << " + " << imag << "i\n";
				}
				baseState += static_cast<StateIndex>(statesInChunk);
			}
		}

		/** @brief Applies phase flip to one basis state. */
		void phaseFlipBasisState(StateIndex state) override {
			mutate("basis-state phase flip", [&]() {
				storage::validateBackendStateIndex("BloscStateBackend::phaseFlipBasisState", state, numStates_);
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
				for(size_t chunkIndex = 0; chunkIndex < layout_.chunkCount(); ++chunkIndex) {
					auto slot = acquireChunk(chunkIndex);
					const size_t elemCount = chunkElemCount(chunkIndex);
					for(size_t elem = 0; elem + 1u < elemCount; elem += 2u) {
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
			ensureInitialized("Hadamard");
			storage::validateBackendSingleQubit("BloscStateBackend::applyHadamard", qubit, numQubits_);
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(qubit, numQubits_);
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

		/** @brief Applies Pauli-X gate to one qubit. */
		void applyPauliX(QubitIndex qubit) override {
			ensureInitialized("PauliX");
			storage::validateBackendSingleQubit("BloscStateBackend::applyPauliX", qubit, numQubits_);
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(qubit, numQubits_);
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

		/** @brief Applies controlled phase-shift gate. */
		void applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) override {
			ensureInitialized("controlled phase shift");
			storage::validateBackendTwoQubits("BloscStateBackend::applyControlledPhaseShift", controlQubit, targetQubit, numQubits_);
			const unsigned int controlMask = storage::qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(targetQubit, numQubits_);
			const double phaseReal = std::cos(theta);
			const double phaseImag = std::sin(theta);
			mutate("controlled phase shift", [&]() {
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
			ensureInitialized("controlled not");
			storage::validateBackendTwoQubits("BloscStateBackend::applyControlledNot", controlQubit, targetQubit, numQubits_);
			const unsigned int controlMask = storage::qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(targetQubit, numQubits_);
			mutate("controlled not", [&]() {
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

		/** @brief Applies arbitrary gate matrix with optional fast-path specialization. */
		void applyGate(const QuantumGate &gate, const QubitList &qubits) override {
			ensureInitialized("gate application");
			// Fast-path common gates to specialized pair kernels for lower overhead.
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

			mutate("gate application", [&]() {
				// Fallback for arbitrary gate matrices.
				auto loadAmplitude = [&](StateIndex state) -> Amplitude { return readAmplitudeMutable(state); };
				auto storeAmplitude = [&](StateIndex state, Amplitude amp) { writeAmplitudeMutable(state, amp); };
				storage::GateApplyEngine::apply(
					"BloscStateBackend::applyGate",
					gate,
					qubits,
					numQubits_,
					loadAmplitude,
					storeAmplitude,
					gateWorkspace_);
			});
		}
};

} // namespace
#endif

/** @brief Reports whether Blosc backend support is available. */
bool isBloscStateBackendAvailable() {
	return storage::BloscCodec::available();
}

/** @brief Factory helper for creating a Blosc backend instance. */
std::unique_ptr<IStateBackend> createBloscStateBackend(const RegisterConfig &cfg) {
#ifdef HAVE_BLOSC2
	return std::make_unique<BloscStateBackend>(cfg);
#else
	(void)cfg;
	return nullptr;
#endif
}

} // namespace tmfqs
