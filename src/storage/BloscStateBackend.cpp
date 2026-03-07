#include "storage/IStateBackend.h"
#include "storage/GateBlockApply.h"
#include "stateSpace.h"
#include "validation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
			uint64_t age = 0;
		};

		unsigned int numQubits_ = 0;
		unsigned int numStates_ = 0;
		RegisterConfig cfg_;
		BloscSchunkPtr schunk_;
		blosc2_cparams schunkCParams_ = BLOSC2_CPARAMS_DEFAULTS;
		BloscContextPtr compressionCtx_;
		std::vector<uint8_t> compressionScratch_;
		std::vector<CacheSlot> gateCache_;
		uint64_t cacheAgeCounter_ = 1;
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
			if(!schunk_) {
				throw std::logic_error(std::string("BloscStateBackend is not initialized for ") + opName);
			}
		}

		void invalidateGateCache() {
			for(CacheSlot &slot : gateCache_) {
				slot.chunkIndex = -1;
				slot.dirty = false;
				slot.age = 0;
			}
			cacheAgeCounter_ = 1;
		}

		void ensureGateCacheStorage() {
			const size_t slots = cfg_.blosc.gateCacheSlots;
			const size_t chunkElems = elemsPerChunk();
			if(gateCache_.size() != slots) {
				gateCache_.assign(slots, {});
			}
			for(CacheSlot &slot : gateCache_) {
				if(slot.buffer.size() != chunkElems) {
					slot.buffer.assign(chunkElems, 0.0);
				}
				slot.chunkIndex = -1;
				slot.dirty = false;
				slot.age = 0;
			}
			cacheAgeCounter_ = 1;
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

		void readChunk(int64_t chunkIndex, std::vector<double> &buffer, size_t expectedElems) const {
			ensureInitialized("chunk read");
			if(buffer.size() < expectedElems) buffer.resize(expectedElems, 0.0);
			const int32_t destBytes = checkedElemsBytesToI32(expectedElems, "readChunk");
			const int dsz = blosc2_schunk_decompress_chunk(
				schunk_.get(), chunkIndex, buffer.data(), destBytes);
			if(dsz < 0) {
				throw std::runtime_error("Blosc2: chunk decompress failed");
			}
		}

		void writeChunk(int64_t chunkIndex, const std::vector<double> &buffer, size_t elems) {
			ensureInitialized("chunk write");
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

		void validateState(StateIndex state) const {
			if(state >= numStates_) {
				throw std::out_of_range("State index out of range");
			}
		}

		void validateSingleQubitOperation(QubitIndex qubit, const char *opName) const {
			ensureInitialized(opName);
			if(qubit >= numQubits_) {
				throw std::out_of_range("BloscStateBackend single-qubit operation qubit index out of range");
			}
		}

		void validateTwoQubitOperation(QubitIndex controlQubit, QubitIndex targetQubit, const char *opName) const {
			ensureInitialized(opName);
			if(controlQubit >= numQubits_ || targetQubit >= numQubits_) {
				throw std::out_of_range("BloscStateBackend two-qubit operation qubit index out of range");
			}
			if(controlQubit == targetQubit) {
				throw std::invalid_argument("BloscStateBackend two-qubit operation requires distinct qubits");
			}
		}

		void validateGateApplicationInputs(const QuantumGate &gate, const QubitList &qubits) const {
			ensureInitialized("gate application");
			validateGateTargets("BloscStateBackend::applyGate", qubits, numQubits_, gate.dimension());
		}

		void flushSlot(size_t slotIndex) {
			CacheSlot &slot = gateCache_[slotIndex];
			if(!slot.dirty || slot.chunkIndex < 0) return;
			const size_t chunkElems = std::min(
				elemsPerChunk(), totalElems() - static_cast<size_t>(slot.chunkIndex) * elemsPerChunk());
			writeChunk(slot.chunkIndex, slot.buffer, chunkElems);
			slot.dirty = false;
		}

		int getSlot(int64_t chunkIndex) {
			for(size_t slot = 0; slot < gateCache_.size(); ++slot) {
				if(gateCache_[slot].chunkIndex == chunkIndex) {
					gateCache_[slot].age = cacheAgeCounter_++;
					return static_cast<int>(slot);
				}
			}

			size_t evict = 0;
			bool foundFree = false;
			for(size_t slot = 0; slot < gateCache_.size(); ++slot) {
				if(gateCache_[slot].chunkIndex < 0) {
					evict = slot;
					foundFree = true;
					break;
				}
			}
			if(!foundFree) {
				for(size_t slot = 1; slot < gateCache_.size(); ++slot) {
					if(gateCache_[slot].age < gateCache_[evict].age) {
						evict = slot;
					}
				}
			}

			flushSlot(evict);
			CacheSlot &slot = gateCache_[evict];
			const size_t chunkElems = std::min(
				elemsPerChunk(), totalElems() - static_cast<size_t>(chunkIndex) * elemsPerChunk());
			const int32_t destBytes = checkedElemsBytesToI32(chunkElems, "getSlot decompress");
			const int dsz = blosc2_schunk_decompress_chunk(
				schunk_.get(), chunkIndex, slot.buffer.data(), destBytes);
			if(dsz < 0) throw std::runtime_error("Blosc2: chunk decompress failed");
			slot.chunkIndex = chunkIndex;
			slot.dirty = false;
			slot.age = cacheAgeCounter_++;
			return static_cast<int>(evict);
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
			swap(gateCache_, other.gateCache_);
			swap(cacheAgeCounter_, other.cacheAgeCounter_);
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

		void initZero(unsigned int numQubits) override {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			createEmptySchunk();
			ensureGateCacheStorage();
			compressionCtx_.reset();
			compressionScratch_.clear();

			const size_t statesPerChunk = chunkStates();
			const size_t chunkElems = elemsPerChunk();
			std::vector<double> chunkBuffer(chunkElems, 0.0);

			for(size_t base = 0; base < numStates_; base += statesPerChunk) {
				const size_t statesInChunk = std::min(statesPerChunk, static_cast<size_t>(numStates_) - base);
				const size_t elemsInChunk = statesInChunk * 2u;
				const int32_t bytes = checkedElemsBytesToI32(elemsInChunk, "initZero append");
				const int64_t nchunks = blosc2_schunk_append_buffer(schunk_.get(), chunkBuffer.data(), bytes);
				if(nchunks < 0) {
					throw std::runtime_error("Blosc2: append zero chunk failed");
				}
			}
			invalidateGateCache();
		}

		void initBasis(unsigned int numQubits, StateIndex initState, Amplitude amp) override {
			initZero(numQubits);
			if(initState >= numStates_) {
				throw std::out_of_range("BloscStateBackend::initBasis state index out of range");
			}
			setAmplitude(initState, amp);
		}

		void initUniformSuperposition(unsigned int numQubits, const BasisStateList &basisStates) override {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			std::vector<StateIndex> selected;
			selected.reserve(basisStates.size());
			for(StateIndex state : basisStates) {
				if(state >= numStates_) {
					throw std::out_of_range("BloscStateBackend::initUniformSuperposition basis state index out of range");
				}
				selected.push_back(state);
			}
			std::sort(selected.begin(), selected.end());
			selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
			if(selected.empty()) {
				throw std::invalid_argument("BloscStateBackend: no valid basis states for uniform initialization");
			}

			createEmptySchunk();
			ensureGateCacheStorage();
			compressionCtx_.reset();
			compressionScratch_.clear();
			const size_t statesPerChunk = chunkStates();
			const double realAmplitude = 1.0 / std::sqrt(static_cast<double>(selected.size()));

			size_t selectedIndex = 0;
			for(size_t baseState = 0; baseState < numStates_; baseState += statesPerChunk) {
				const size_t statesInChunk = std::min(statesPerChunk, static_cast<size_t>(numStates_) - baseState);
				std::vector<double> chunkBuffer(statesInChunk * 2u, 0.0);
				const size_t chunkEnd = baseState + statesInChunk;

				while(selectedIndex < selected.size() && selected[selectedIndex] < chunkEnd) {
					const size_t localState = static_cast<size_t>(selected[selectedIndex]) - baseState;
					chunkBuffer[localState * 2u] = realAmplitude;
					chunkBuffer[localState * 2u + 1u] = 0.0;
					++selectedIndex;
				}

				const int32_t bytes = checkedElemsBytesToI32(chunkBuffer.size(), "initUniformSuperposition append");
				const int64_t nchunks = blosc2_schunk_append_buffer(schunk_.get(), chunkBuffer.data(), bytes);
				if(nchunks < 0) {
					throw std::runtime_error("Blosc2: append uniform-superposition chunk failed");
				}
			}
			invalidateGateCache();
		}

		void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) override {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			if(amplitudes.size() != checkedAmplitudeElementCount(numQubits_)) {
				throw std::invalid_argument("BloscStateBackend: amplitudes size mismatch");
			}
			createEmptySchunk();
			ensureGateCacheStorage();
			compressionCtx_.reset();
			compressionScratch_.clear();
			const size_t statesPerChunk = chunkStates();
			for(size_t baseState = 0; baseState < numStates_; baseState += statesPerChunk) {
				const size_t statesInChunk = std::min(statesPerChunk, static_cast<size_t>(numStates_) - baseState);
				const size_t elemOffset = baseState * 2u;
				const int32_t bytes = checkedElemsBytesToI32(statesInChunk * 2u, "loadAmplitudes append");
				const int64_t nchunks = blosc2_schunk_append_buffer(
					schunk_.get(), amplitudes.data() + elemOffset, bytes);
				if(nchunks < 0) {
					throw std::runtime_error("Blosc2: append amplitudes chunk failed");
				}
			}
			invalidateGateCache();
		}

		Amplitude amplitude(StateIndex state) const override {
			ensureInitialized("amplitude query");
			validateState(state);
			const size_t statesPerChunk = chunkStates();
			const int64_t chunkIndex = static_cast<int64_t>(state / statesPerChunk);
			const size_t offset = static_cast<size_t>(state % statesPerChunk) * 2u;
			const size_t chunkElems = std::min(
				elemsPerChunk(), totalElems() - static_cast<size_t>(chunkIndex) * elemsPerChunk());
			std::vector<double> buffer(chunkElems, 0.0);
			readChunk(chunkIndex, buffer, chunkElems);
			return {buffer[offset], buffer[offset + 1u]};
		}

		void setAmplitude(StateIndex state, Amplitude amp) override {
			ensureInitialized("state update");
			validateState(state);
			const size_t statesPerChunk = chunkStates();
			const int64_t chunkIndex = static_cast<int64_t>(state / statesPerChunk);
			const size_t offset = static_cast<size_t>(state % statesPerChunk) * 2u;
			const size_t chunkElems = std::min(
				elemsPerChunk(), totalElems() - static_cast<size_t>(chunkIndex) * elemsPerChunk());
			std::vector<double> buffer(chunkElems, 0.0);
			readChunk(chunkIndex, buffer, chunkElems);
			buffer[offset] = amp.real;
			buffer[offset + 1u] = amp.imag;
			writeChunk(chunkIndex, buffer, chunkElems);
			invalidateGateCache();
		}

		double probability(StateIndex state) const override {
			const Amplitude amp = amplitude(state);
			return amp.real * amp.real + amp.imag * amp.imag;
		}

		double totalProbability() const override {
			ensureInitialized("total probability query");
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
			if(rnd < 0.0 || rnd >= 1.0) {
				throw std::invalid_argument("BloscStateBackend::sampleMeasurement requires rnd in [0,1)");
			}
			double cumulative = 0.0;
			const size_t statesPerChunk = chunkStates();
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
				baseState += static_cast<StateIndex>(statesPerChunk);
			}
			throw std::runtime_error("BloscStateBackend::sampleMeasurement cumulative probability did not reach sample");
		}

		void printNonZeroStates(std::ostream &os, double epsilon) const override {
			ensureInitialized("state printing");
			if(epsilon < 0.0) {
				throw std::invalid_argument("BloscStateBackend::printNonZeroStates epsilon must be non-negative");
			}
			const size_t statesPerChunk = chunkStates();
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
				baseState += static_cast<StateIndex>(statesPerChunk);
			}
		}

		void phaseFlipBasisState(StateIndex state) override {
			ensureInitialized("basis-state phase flip");
			validateState(state);
			const size_t statesPerChunk = chunkStates();
			const int64_t chunkIndex = static_cast<int64_t>(state / statesPerChunk);
			const size_t offset = static_cast<size_t>(state % statesPerChunk) * 2u;
			const size_t chunkElems = std::min(
				elemsPerChunk(), totalElems() - static_cast<size_t>(chunkIndex) * elemsPerChunk());
			std::vector<double> buffer(chunkElems, 0.0);
			readChunk(chunkIndex, buffer, chunkElems);
			buffer[offset] = -buffer[offset];
			buffer[offset + 1u] = -buffer[offset + 1u];
			writeChunk(chunkIndex, buffer, chunkElems);
			invalidateGateCache();
		}

		void inversionAboutMean() override {
			ensureInitialized("inversion about mean");
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
			applyGate(QuantumGate::Hadamard(), QubitList{qubit});
		}

		void applyPauliX(QubitIndex qubit) override {
			validateSingleQubitOperation(qubit, "PauliX");
			applyGate(QuantumGate::PauliX(), QubitList{qubit});
		}

		void applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) override {
			validateTwoQubitOperation(controlQubit, targetQubit, "controlled phase shift");
			applyGate(QuantumGate::ControlledPhaseShift(theta), QubitList{controlQubit, targetQubit});
		}

		void applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) override {
			validateTwoQubitOperation(controlQubit, targetQubit, "controlled not");
			applyGate(QuantumGate::ControlledNot(), QubitList{controlQubit, targetQubit});
		}

		void applyGate(const QuantumGate &gate, const QubitList &qubits) override {
			validateGateApplicationInputs(gate, qubits);
			ensureGateCacheStorage();
			ensureCompressionContext();
			invalidateGateCache();

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

			const GateBlockLayout layout = makeGateBlockLayout(qubits, numQubits_);
			applyGateByBlocks(gate, layout, loadAmplitude, storeAmplitude, gateWorkspace_);

			for(size_t slot = 0; slot < gateCache_.size(); ++slot) {
				flushSlot(slot);
			}
			invalidateGateCache();
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
