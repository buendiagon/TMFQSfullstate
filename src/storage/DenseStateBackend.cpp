#include "storage/IStateBackend.h"
#include "stateSpace.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

// Dense backend: full uncompressed state vector in memory.
class DenseStateBackend : public IStateBackend {

	private:
		unsigned int numQubits_ = 0;
		unsigned int numStates_ = 0;
		AmplitudesVector amplitudes_;

		void validateState(unsigned int state) const {
			if(state >= numStates_) {
				throw std::out_of_range("State index out of range");
			}
		}

	public:
		explicit DenseStateBackend(unsigned int numQubits = 0) {
			if(numQubits > 0) initBasis(numQubits, 0, {1.0, 0.0});
		}

		DenseStateBackend(const DenseStateBackend &other) = default;
		DenseStateBackend& operator=(const DenseStateBackend &other) = default;

		std::unique_ptr<IStateBackend> clone() const override {
			return std::make_unique<DenseStateBackend>(*this);
		}

		void initZero(unsigned int numQubits) override {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			amplitudes_.assign(checkedAmplitudeElementCount(numQubits_), 0.0);
		}

		// Initialize a basis state |initState> with custom complex amplitude.
		void initBasis(unsigned int numQubits, unsigned int initState, Amplitude amp) override {
			initZero(numQubits);
			if(initState < numStates_) {
				amplitudes_[2 * initState] = amp.real;
				amplitudes_[2 * initState + 1] = amp.imag;
			}
		}

		// Build uniform superposition over an arbitrary subset of basis states.
		void initUniformSuperposition(unsigned int numQubits, const StatesVector &basisStates) override {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			amplitudes_.assign(checkedAmplitudeElementCount(numQubits_), 0.0);

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
				throw std::invalid_argument("DenseStateBackend: no valid basis states for uniform initialization");
			}

			const double realAmplitude = 1.0 / std::sqrt((double)selected.size());
			for(unsigned int state : selected) {
				amplitudes_[2 * state] = realAmplitude;
				amplitudes_[2 * state + 1] = 0.0;
			}
		}

		void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) override {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			if(amplitudes.size() != checkedAmplitudeElementCount(numQubits_)) {
				throw std::invalid_argument("DenseStateBackend: amplitudes size mismatch");
			}
			amplitudes_ = std::move(amplitudes);
		}

		Amplitude amplitude(unsigned int state) const override {
			Amplitude amp{0.0, 0.0};
			if(state < numStates_) {
				amp.real = amplitudes_[state * 2];
				amp.imag = amplitudes_[state * 2 + 1];
			}
			return amp;
		}

		void setAmplitude(unsigned int state, Amplitude amp) override {
			validateState(state);
			amplitudes_[state * 2] = amp.real;
			amplitudes_[state * 2 + 1] = amp.imag;
		}

		double probability(unsigned int state) const override {
			Amplitude amp = amplitude(state);
			return amp.real * amp.real + amp.imag * amp.imag;
		}

		double probabilitySumatory() const override {
			double sum = 0.0;
			for(unsigned int i = 0; i < numStates_; ++i) {
				double real = amplitudes_[i * 2];
				double imag = amplitudes_[i * 2 + 1];
				sum += real * real + imag * imag;
			}
			return sum;
		}

		// Applies a k-qubit gate by iterating independent blocks of size 2^k.
		void applyGate(const QuantumGate &gate, const IntegerVector &qubits, unsigned int numQubits) override {
			if(qubits.size() > maxSupportedQubitsForU32States()) {
				return;
			}
			if(gate.dimension != checkedStateCount((unsigned int)qubits.size())) {
				return;
			}

			int k = (int)qubits.size();
			if((unsigned int)k > numQubits) {
				return;
			}
			unsigned int numBlocks = checkedStateCount(numQubits - (unsigned int)k);
			unsigned int blockSize = checkedStateCount((unsigned int)k);

			std::vector<unsigned int> targetPos((size_t)k);
			for(int i = 0; i < k; ++i) {
				targetPos[(size_t)i] = numQubits - qubits[(size_t)i] - 1;
			}

			std::vector<Amplitude> localAmps(blockSize);
			std::vector<unsigned int> stateIndices(blockSize);

			for(unsigned int block = 0; block < numBlocks; ++block) {
				// Reconstruct base state for this block (all non-target qubits fixed).
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
						unsigned int b = tempBlock & 1u;
						baseState |= (b << bit);
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
					localAmps[idx].real = amplitudes_[state * 2];
					localAmps[idx].imag = amplitudes_[state * 2 + 1];
					if(localAmps[idx].real != 0.0 || localAmps[idx].imag != 0.0) {
						hasNonZero = true;
					}
				}

				// Skip multiplication if all amplitudes in the block are zero.
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
					amplitudes_[state * 2] = newAmp.real;
					amplitudes_[state * 2 + 1] = newAmp.imag;
				}
			}
		}
};

std::unique_ptr<IStateBackend> createDenseBackend(unsigned int numQubits, const RegisterConfig &) {
	(void)numQubits;
	return std::make_unique<DenseStateBackend>();
}
