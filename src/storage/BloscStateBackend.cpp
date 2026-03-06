#include "storage/IStateBackend.h"
#include "stateSpace.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <vector>

#ifdef HAVE_BLOSC2
#include <blosc2.h>

// Blosc backend: chunked compressed representation backed by a Blosc2 super-chunk.
class BloscStateBackend : public IStateBackend {

	private:
		unsigned int numQubits_ = 0;
		unsigned int numStates_ = 0;
		RegisterConfig cfg_;
		blosc2_schunk* schunk_ = nullptr;

		size_t chunkStates() const {
			return std::max<size_t>(1, cfg_.blosc.chunkStates);
		}

		size_t totalElems() const {
			return (size_t)numStates_ * 2;
		}

		// Release any previously allocated super-chunk.
		void freeSchunk() {
			if(schunk_) {
				blosc2_schunk_free(schunk_);
				schunk_ = nullptr;
			}
		}

		// Build compression parameters from RegisterConfig.
		blosc2_cparams makeCParams() const {
			blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
			cparams.compcode = (uint8_t)cfg_.blosc.compcode;
			cparams.clevel = cfg_.blosc.clevel;
			cparams.nthreads = cfg_.blosc.nthreads;
			cparams.typesize = sizeof(double);
			cparams.filters[BLOSC2_MAX_FILTERS - 1] = cfg_.blosc.useShuffle ? BLOSC_SHUFFLE : BLOSC_NOFILTER;
			return cparams;
		}

		// Create an empty in-memory Blosc2 super-chunk.
		void createEmptySchunk() {
			blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
			blosc2_cparams cparams = makeCParams();
			storage.cparams = &cparams;
			schunk_ = blosc2_schunk_new(&storage);
			if(!schunk_) {
				throw std::runtime_error("Blosc2: failed creating super-chunk");
			}
		}

		// Decompress one chunk into a temporary buffer.
		void readChunk(int64_t chunkIndex, std::vector<double> &buffer, size_t expectedElems) const {
			if(!schunk_) throw std::runtime_error("Blosc2: null schunk");
			if(buffer.size() < expectedElems) buffer.resize(expectedElems, 0.0);
			int dsz = blosc2_schunk_decompress_chunk(
				schunk_, chunkIndex, buffer.data(), (int32_t)(expectedElems * sizeof(double)));
			if(dsz < 0) {
				throw std::runtime_error("Blosc2: chunk decompress failed");
			}
		}

		// Compress and overwrite one chunk.
		void writeChunk(int64_t chunkIndex, const std::vector<double> &buffer, size_t elems) {
			if(!schunk_) throw std::runtime_error("Blosc2: null schunk");
			size_t srcBytes = elems * sizeof(double);
			std::vector<uint8_t> compressed(srcBytes + BLOSC2_MAX_OVERHEAD);
			blosc2_cparams cparams = makeCParams();
			blosc2_context* cctx = blosc2_create_cctx(cparams);
			if(!cctx) throw std::runtime_error("Blosc2: failed creating compression context");
			int csz = blosc2_compress_ctx(cctx, buffer.data(), (int32_t)srcBytes,
				compressed.data(), (int32_t)compressed.size());
			blosc2_free_ctx(cctx);
			if(csz <= 0) {
				throw std::runtime_error("Blosc2: chunk compress failed");
			}
			int64_t rc = blosc2_schunk_update_chunk(schunk_, chunkIndex, compressed.data(), true);
			if(rc < 0) {
				throw std::runtime_error("Blosc2: chunk update failed");
			}
		}

		void validateState(unsigned int state) const {
			if(state >= numStates_) {
				throw std::out_of_range("State index out of range");
			}
		}

	public:
		explicit BloscStateBackend(const RegisterConfig &cfg) : cfg_(cfg) {
			blosc2_init();
		}

		// Clone by copying all compressed chunks to a new super-chunk.
		BloscStateBackend(const BloscStateBackend &other) : cfg_(other.cfg_) {
			blosc2_init();
			numQubits_ = other.numQubits_;
			numStates_ = other.numStates_;
			if(!other.schunk_) return;
			createEmptySchunk();
			for(int64_t ci = 0; ci < other.schunk_->nchunks; ++ci) {
				uint8_t *chunk = nullptr;
				bool needsFree = false;
				int cbytes = blosc2_schunk_get_chunk(other.schunk_, ci, &chunk, &needsFree);
				if(cbytes < 0) {
					if(needsFree && chunk) free(chunk);
					throw std::runtime_error("Blosc2: failed reading chunk while cloning");
				}
				int64_t rc = blosc2_schunk_append_chunk(schunk_, chunk, true);
				if(needsFree && chunk) free(chunk);
				if(rc < 0) {
					throw std::runtime_error("Blosc2: failed appending chunk while cloning");
				}
			}
		}

		BloscStateBackend& operator=(const BloscStateBackend &other) {
			if(this == &other) return *this;
			freeSchunk();
			cfg_ = other.cfg_;
			numQubits_ = other.numQubits_;
			numStates_ = other.numStates_;
			if(other.schunk_) {
				createEmptySchunk();
				for(int64_t ci = 0; ci < other.schunk_->nchunks; ++ci) {
					uint8_t *chunk = nullptr;
					bool needsFree = false;
					int cbytes = blosc2_schunk_get_chunk(other.schunk_, ci, &chunk, &needsFree);
					if(cbytes < 0) {
						if(needsFree && chunk) free(chunk);
						throw std::runtime_error("Blosc2: failed reading chunk while assigning");
					}
					int64_t rc = blosc2_schunk_append_chunk(schunk_, chunk, true);
					if(needsFree && chunk) free(chunk);
					if(rc < 0) {
						throw std::runtime_error("Blosc2: failed appending chunk while assigning");
					}
				}
			}
			return *this;
		}

		~BloscStateBackend() override {
			freeSchunk();
		}

		std::unique_ptr<IStateBackend> clone() const override {
			return std::make_unique<BloscStateBackend>(*this);
		}

		// Initialize with all-zero amplitudes, stored chunk-by-chunk.
		void initZero(unsigned int numQubits) override {
			freeSchunk();
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			createEmptySchunk();

			const size_t statesPerChunk = chunkStates();
			const size_t elemsPerChunk = statesPerChunk * 2;
			std::vector<double> chunkBuffer(elemsPerChunk, 0.0);

			for(size_t base = 0; base < numStates_; base += statesPerChunk) {
				size_t statesInChunk = std::min(statesPerChunk, (size_t)numStates_ - base);
				int64_t nchunks = blosc2_schunk_append_buffer(
					schunk_, chunkBuffer.data(), (int32_t)(statesInChunk * 2 * sizeof(double)));
				if(nchunks < 0) {
					throw std::runtime_error("Blosc2: append zero chunk failed");
				}
			}
		}

		void initBasis(unsigned int numQubits, unsigned int initState, Amplitude amp) override {
			initZero(numQubits);
			if(initState < numStates_) {
				setAmplitude(initState, amp);
			}
		}

		// Build sparse uniform superposition directly into compressed chunks.
		void initUniformSuperposition(unsigned int numQubits, const StatesVector &basisStates) override {
			freeSchunk();
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);

			StatesVector selected;
			selected.reserve(basisStates.size());
			for(unsigned int state : basisStates) {
				if(state < numStates_) {
					selected.push_back(state);
				}
			}
			std::sort(selected.begin(), selected.end());
			selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
			if(selected.empty()) {
				throw std::invalid_argument("BloscStateBackend: no valid basis states for uniform initialization");
			}

			createEmptySchunk();
			const size_t statesPerChunk = chunkStates();
			const double realAmplitude = 1.0 / std::sqrt((double)selected.size());

			size_t selectedIndex = 0;
			for(size_t baseState = 0; baseState < numStates_; baseState += statesPerChunk) {
				size_t statesInChunk = std::min(statesPerChunk, (size_t)numStates_ - baseState);
				std::vector<double> chunkBuffer(statesInChunk * 2, 0.0);
				size_t chunkEnd = baseState + statesInChunk;

				while(selectedIndex < selected.size() && selected[selectedIndex] < chunkEnd) {
					size_t localState = (size_t)selected[selectedIndex] - baseState;
					chunkBuffer[localState * 2] = realAmplitude;
					chunkBuffer[localState * 2 + 1] = 0.0;
					++selectedIndex;
				}

				int64_t nchunks = blosc2_schunk_append_buffer(
					schunk_, chunkBuffer.data(), (int32_t)(chunkBuffer.size() * sizeof(double)));
				if(nchunks < 0) {
					throw std::runtime_error("Blosc2: append uniform-superposition chunk failed");
				}
			}
		}

		// Bulk-load dense amplitudes and chunk-compress them.
		void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) override {
			freeSchunk();
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			if(amplitudes.size() != checkedAmplitudeElementCount(numQubits_)) {
				throw std::invalid_argument("BloscStateBackend: amplitudes size mismatch");
			}
			createEmptySchunk();
			const size_t statesPerChunk = chunkStates();
			for(size_t baseState = 0; baseState < numStates_; baseState += statesPerChunk) {
				size_t statesInChunk = std::min(statesPerChunk, (size_t)numStates_ - baseState);
				size_t elemOffset = baseState * 2;
				int64_t nchunks = blosc2_schunk_append_buffer(
					schunk_, amplitudes.data() + elemOffset, (int32_t)(statesInChunk * 2 * sizeof(double)));
				if(nchunks < 0) {
					throw std::runtime_error("Blosc2: append amplitudes chunk failed");
				}
			}
		}

		Amplitude amplitude(unsigned int state) const override {
			Amplitude amp{0.0, 0.0};
			if(state >= numStates_ || !schunk_) return amp;
			const size_t statesPerChunk = chunkStates();
			const size_t elemsPerChunk = statesPerChunk * 2;
			int64_t chunkIndex = (int64_t)(state / statesPerChunk);
			size_t offset = (state % statesPerChunk) * 2;
			size_t chunkElems = std::min(elemsPerChunk, totalElems() - (size_t)chunkIndex * elemsPerChunk);
			std::vector<double> buffer(chunkElems, 0.0);
			readChunk(chunkIndex, buffer, chunkElems);
			amp.real = buffer[offset];
			amp.imag = buffer[offset + 1];
			return amp;
		}

		// Point update by round-tripping the owning chunk.
		void setAmplitude(unsigned int state, Amplitude amp) override {
			validateState(state);
			const size_t statesPerChunk = chunkStates();
			const size_t elemsPerChunk = statesPerChunk * 2;
			int64_t chunkIndex = (int64_t)(state / statesPerChunk);
			size_t offset = (state % statesPerChunk) * 2;
			size_t chunkElems = std::min(elemsPerChunk, totalElems() - (size_t)chunkIndex * elemsPerChunk);
			std::vector<double> buffer(chunkElems, 0.0);
			readChunk(chunkIndex, buffer, chunkElems);
			buffer[offset] = amp.real;
			buffer[offset + 1] = amp.imag;
			writeChunk(chunkIndex, buffer, chunkElems);
		}

		double probability(unsigned int state) const override {
			Amplitude amp = amplitude(state);
			return amp.real * amp.real + amp.imag * amp.imag;
		}

		// Aggregate probability by scanning all chunks.
		double probabilitySumatory() const override {
			if(!schunk_) return 0.0;
			double sum = 0.0;
			const size_t statesPerChunk = chunkStates();
			const size_t elemsPerChunk = statesPerChunk * 2;
			for(int64_t ci = 0; ci < schunk_->nchunks; ++ci) {
				size_t chunkElems = std::min(elemsPerChunk, totalElems() - (size_t)ci * elemsPerChunk);
				std::vector<double> buffer(chunkElems, 0.0);
				readChunk(ci, buffer, chunkElems);
				for(size_t elem = 0; elem + 1 < chunkElems; elem += 2) {
					double real = buffer[elem];
					double imag = buffer[elem + 1];
					sum += real * real + imag * imag;
				}
			}
			return sum;
		}

		// Gate application with small LRU-like chunk cache to avoid repeated
		// decompress/recompress on the same chunks during block updates.
		void applyGate(const QuantumGate &gate, const IntegerVector &qubits, unsigned int numQubits) override {
			if(!schunk_) return;
			if(qubits.size() > maxSupportedQubitsForU32States()) return;
			if(gate.dimension != checkedStateCount((unsigned int)qubits.size())) return;

			const size_t statesPerChunk = chunkStates();
			const size_t elemsPerChunk = statesPerChunk * 2;
			const int cacheSlots = 8;
			const size_t allElems = totalElems();

			int k = (int)qubits.size();
			if((unsigned int)k > numQubits) return;
			unsigned int numBlocks = checkedStateCount(numQubits - (unsigned int)k);
			unsigned int blockSize = checkedStateCount((unsigned int)k);

			std::vector<unsigned int> targetPos((size_t)k);
			for(int i = 0; i < k; ++i) targetPos[(size_t)i] = numQubits - qubits[(size_t)i] - 1;

			std::vector<Amplitude> localAmps(blockSize);
			std::vector<unsigned int> stateIndices(blockSize);

			std::vector<std::vector<double>> cacheBuf((size_t)cacheSlots, std::vector<double>(elemsPerChunk, 0.0));
			std::vector<int64_t> cacheChunkIdx((size_t)cacheSlots, -1LL);
			std::vector<bool> cacheDirty((size_t)cacheSlots, false);
			std::vector<uint64_t> cacheAge((size_t)cacheSlots, 0);
			uint64_t ageCounter = 1;

			blosc2_cparams cparams = makeCParams();
			blosc2_context* cctx = blosc2_create_cctx(cparams);
			if(!cctx) throw std::runtime_error("Blosc2: failed creating compression context");
			std::vector<uint8_t> compressedTmp(elemsPerChunk * sizeof(double) + BLOSC2_MAX_OVERHEAD);

			// Flush dirty cache slot back into compressed storage.
			auto flushSlot = [&](int slot) {
				if(slot < 0 || slot >= cacheSlots) return;
				if(!cacheDirty[(size_t)slot] || cacheChunkIdx[(size_t)slot] < 0) return;
				int64_t ci = cacheChunkIdx[(size_t)slot];
				size_t nElems = std::min(elemsPerChunk, allElems - (size_t)ci * elemsPerChunk);
				size_t srcBytes = nElems * sizeof(double);
				int csz = blosc2_compress_ctx(cctx, cacheBuf[(size_t)slot].data(), (int32_t)srcBytes,
					compressedTmp.data(), (int32_t)compressedTmp.size());
				if(csz > 0) {
					int64_t rc = blosc2_schunk_update_chunk(schunk_, ci, compressedTmp.data(), true);
					if(rc < 0) throw std::runtime_error("Blosc2: chunk update failed");
				}
				cacheDirty[(size_t)slot] = false;
			};

			// Get cache slot for a chunk, evicting least-recently-used when needed.
			auto getSlot = [&](int64_t chunkIdx) -> int {
				for(int slot = 0; slot < cacheSlots; ++slot) {
					if(cacheChunkIdx[(size_t)slot] == chunkIdx) {
						cacheAge[(size_t)slot] = ageCounter++;
						return slot;
					}
				}
				int evict = 0;
				bool foundFree = false;
				for(int slot = 0; slot < cacheSlots; ++slot) {
					if(cacheChunkIdx[(size_t)slot] < 0) {
						evict = slot;
						foundFree = true;
						break;
					}
				}
				if(!foundFree) {
					for(int slot = 1; slot < cacheSlots; ++slot) {
						if(cacheAge[(size_t)slot] < cacheAge[(size_t)evict]) evict = slot;
					}
				}
				flushSlot(evict);
				size_t nElems = std::min(elemsPerChunk, allElems - (size_t)chunkIdx * elemsPerChunk);
				int dsz = blosc2_schunk_decompress_chunk(
					schunk_, chunkIdx, cacheBuf[(size_t)evict].data(), (int32_t)(nElems * sizeof(double)));
				if(dsz < 0) throw std::runtime_error("Blosc2: chunk decompress failed");
				cacheChunkIdx[(size_t)evict] = chunkIdx;
				cacheDirty[(size_t)evict] = false;
				cacheAge[(size_t)evict] = ageCounter++;
				return evict;
			};

			for(unsigned int block = 0; block < numBlocks; ++block) {
				// Reconstruct base state for this block (non-target qubits fixed).
				unsigned int baseState = 0;
				unsigned int tempBlock = block;
				for(int bit = 0; bit < (int)numQubits; ++bit) {
					bool isTarget = false;
					for(int i = 0; i < k; ++i) {
						if((int)targetPos[(size_t)i] == bit) {
							isTarget = true;
							break;
						}
					}
					if(!isTarget) {
						baseState |= ((tempBlock & 1u) << bit);
						tempBlock >>= 1u;
					}
				}

				bool hasNonZero = false;
				for(unsigned int idx = 0; idx < blockSize; ++idx) {
					unsigned int state = baseState;
					for(int i = 0; i < k; ++i) {
						unsigned int bitVal = (idx >> (k - 1 - i)) & 1u;
						state |= (bitVal << targetPos[(size_t)i]);
					}
					stateIndices[idx] = state;
					int64_t ci = (int64_t)(state / statesPerChunk);
					size_t off = (state % statesPerChunk) * 2;
					int slot = getSlot(ci);
					localAmps[idx].real = cacheBuf[(size_t)slot][off];
					localAmps[idx].imag = cacheBuf[(size_t)slot][off + 1];
					if(localAmps[idx].real != 0.0 || localAmps[idx].imag != 0.0) hasNonZero = true;
				}

				// Skip block multiplication if all participating amplitudes are zero.
				if(!hasNonZero) continue;

				for(unsigned int row = 0; row < blockSize; ++row) {
					Amplitude newAmp{0.0, 0.0};
					for(unsigned int col = 0; col < blockSize; ++col) {
						Amplitude g = gate[row][col];
						Amplitude a = localAmps[col];
						newAmp.real += g.real * a.real - g.imag * a.imag;
						newAmp.imag += g.real * a.imag + g.imag * a.real;
					}
					unsigned int state = stateIndices[row];
					int64_t ci = (int64_t)(state / statesPerChunk);
					size_t off = (state % statesPerChunk) * 2;
					int slot = getSlot(ci);
					cacheBuf[(size_t)slot][off] = newAmp.real;
					cacheBuf[(size_t)slot][off + 1] = newAmp.imag;
					cacheDirty[(size_t)slot] = true;
				}
			}

			for(int slot = 0; slot < cacheSlots; ++slot) flushSlot(slot);
			blosc2_free_ctx(cctx);
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

// Stub hooks used when Blosc2 support is not compiled in.
bool isBloscBackendCompiled() {
	return false;
}

std::unique_ptr<IStateBackend> createBloscBackend(unsigned int, const RegisterConfig &) {
	return nullptr;
}

#endif
