#include "tmfqs/storage/dense/dense_state_backend.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "tmfqs/core/state_space.h"
#include "tmfqs/storage/common/backend_validation.h"
#include "tmfqs/storage/common/gate_apply_engine.h"
#include "tmfqs/storage/common/pair_kernel_executor.h"

namespace tmfqs {
/**
 * @brief Dense backend that stores the full state vector uncompressed in memory.
 */
class DenseStateBackend final : public IStateBackend {
	private:
		/** @brief Register size metadata cached for validation and masks. */
		unsigned int numQubits_ = 0;
		unsigned int numStates_ = 0;
		/** @brief Interleaved amplitudes `[real0, imag0, real1, imag1, ...]`. */
		AmplitudesVector amplitudes_;
		/** @brief Reused scratch buffers for arbitrary gate application. */
		GateBlockWorkspace gateWorkspace_;

		/** @brief Verifies that backend storage is initialized before an operation. */
		void ensureInitialized(const char *operation) const {
			storage::ensureBackendInitialized(numStates_ > 0 && !amplitudes_.empty(), "DenseStateBackend", operation);
		}

		/** @brief Validates a basis-state index against current backend size. */
		void validateState(StateIndex state, const char *scopeName) const {
			storage::validateBackendStateIndex(scopeName, state, numStates_);
		}

		/** @brief Validates one qubit index for a single-qubit operation. */
		void validateSingleQubitOperation(QubitIndex qubit, const char *scopeName) const {
			ensureInitialized(scopeName);
			storage::validateBackendSingleQubit(scopeName, qubit, numQubits_);
		}

		/** @brief Validates two distinct qubits for two-qubit operations. */
		void validateTwoQubitOperation(QubitIndex q0, QubitIndex q1, const char *scopeName) const {
			ensureInitialized(scopeName);
			storage::validateBackendTwoQubits(scopeName, q0, q1, numQubits_);
		}

		template <typename PairFn>
		/** @brief Iterates state pairs directly over the contiguous dense buffer. */
		void runPairKernel(unsigned int targetMask, PairFn pairFn) {
			storage::PairKernelExecutor::runFallback(numStates_, targetMask, [&](StateIndex state0, StateIndex state1) {
				pairFn(
					state0,
					state1,
					amplitudes_.data() + static_cast<size_t>(state0) * 2u,
					amplitudes_.data() + static_cast<size_t>(state1) * 2u);
			});
		}

	public:
		/** @brief Constructs backend and optionally initializes to `|0...0>`. */
		explicit DenseStateBackend(const RegisterConfig & = {}) {}

		DenseStateBackend(const DenseStateBackend &) = default;
		DenseStateBackend &operator=(const DenseStateBackend &) = default;

		/** @brief Clones backend state for copy operations. */
		std::unique_ptr<IStateBackend> clone() const override {
			return std::make_unique<DenseStateBackend>(*this);
		}

		/** @brief Allocates zero-initialized amplitude storage for a register size. */
		void initZero(unsigned int numQubits) override {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			amplitudes_.assign(checkedAmplitudeElementCount(numQubits_), 0.0);
		}

		/** @brief Initializes one basis state with a custom amplitude. */
		void initBasis(unsigned int numQubits, StateIndex initState, Amplitude amp) override {
			initZero(numQubits);
			validateState(initState, "DenseStateBackend::initBasis");
			amplitudes_[2 * initState] = amp.real;
			amplitudes_[2 * initState + 1] = amp.imag;
		}

		/** @brief Initializes equal superposition over the provided basis states. */
		void initUniformSuperposition(unsigned int numQubits, const BasisStateList &basisStates) override {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			amplitudes_.assign(checkedAmplitudeElementCount(numQubits_), 0.0);

			std::vector<StateIndex> selected;
			selected.reserve(basisStates.size());
			for(StateIndex state : basisStates) {
				validateState(state, "DenseStateBackend::initUniformSuperposition");
				selected.push_back(state);
			}
			// De-duplicate inputs to preserve normalized amplitude calculation.
			std::sort(selected.begin(), selected.end());
			selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
			if(selected.empty()) {
				throw std::invalid_argument("DenseStateBackend::initUniformSuperposition requires at least one state");
			}

			const double realAmplitude = 1.0 / std::sqrt(static_cast<double>(selected.size()));
			for(StateIndex state : selected) {
				amplitudes_[2 * state] = realAmplitude;
				amplitudes_[2 * state + 1] = 0.0;
			}
		}

		/** @brief Loads a full interleaved amplitude buffer into backend storage. */
		void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) override {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			if(amplitudes.size() != checkedAmplitudeElementCount(numQubits_)) {
				throw std::invalid_argument("DenseStateBackend: amplitudes size mismatch");
			}
			amplitudes_ = std::move(amplitudes);
		}

		/** @brief Copies the full interleaved amplitude buffer into `out`. */
		void exportAmplitudes(AmplitudesVector &out) const override {
			ensureInitialized("amplitude export");
			out = amplitudes_;
		}

		/** @brief Visits the dense amplitude buffer as one contiguous chunk. */
		void forEachAmplitudeChunk(const AmplitudeChunkVisitor &visitor) const override {
			ensureInitialized("amplitude chunk iteration");
			visitor(0u, amplitudes_.data(), amplitudes_.size());
		}

		/** @brief Returns complex amplitude for one basis state. */
		Amplitude amplitude(StateIndex state) const override {
			ensureInitialized("amplitude query");
			validateState(state, "DenseStateBackend::amplitude");
			return {amplitudes_[state * 2], amplitudes_[state * 2 + 1]};
		}

		/** @brief Writes complex amplitude for one basis state. */
		void setAmplitude(StateIndex state, Amplitude amp) override {
			ensureInitialized("state update");
			validateState(state, "DenseStateBackend::setAmplitude");
			amplitudes_[state * 2] = amp.real;
			amplitudes_[state * 2 + 1] = amp.imag;
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
			const double *amp = amplitudes_.data();
			const double *end = amp + amplitudes_.size();
			for(; amp != end; amp += 2) {
				sum += amp[0] * amp[0] + amp[1] * amp[1];
			}
			return sum;
		}

		/** @brief Samples one basis state from cumulative probability. */
		StateIndex sampleMeasurement(double rnd) const override {
			ensureInitialized("measurement");
			if(rnd < 0.0 || rnd >= 1.0) {
				throw std::invalid_argument("DenseStateBackend::sampleMeasurement requires rnd in [0,1)");
			}
			double cumulative = 0.0;
			const double *amp = amplitudes_.data();
			for(StateIndex state = 0; state < numStates_; ++state, amp += 2) {
				cumulative += amp[0] * amp[0] + amp[1] * amp[1];
				if(rnd <= cumulative) {
					return state;
				}
			}
			throw std::runtime_error("DenseStateBackend::sampleMeasurement cumulative probability did not reach sample");
		}

		/** @brief Prints states whose amplitudes exceed the epsilon threshold. */
		void printNonZeroStates(std::ostream &os, double epsilon) const override {
			ensureInitialized("state printing");
			if(epsilon < 0.0) {
				throw std::invalid_argument("DenseStateBackend::printNonZeroStates epsilon must be non-negative");
			}
			for(StateIndex state = 0; state < numStates_; ++state) {
				const double real = amplitudes_[state * 2];
				const double imag = amplitudes_[state * 2 + 1];
				if(std::abs(real) > epsilon || std::abs(imag) > epsilon) {
					os << state << ": " << real << " + " << imag << "i\n";
				}
			}
		}

		/** @brief Applies a `-1` phase to one basis state amplitude. */
		void phaseFlipBasisState(StateIndex state) override {
			ensureInitialized("basis-state phase flip");
			validateState(state, "DenseStateBackend::phaseFlipBasisState");
			amplitudes_[2 * state] = -amplitudes_[2 * state];
			amplitudes_[2 * state + 1] = -amplitudes_[2 * state + 1];
		}

		/** @brief Applies inversion-about-mean to all amplitudes. */
		void inversionAboutMean() override {
			ensureInitialized("inversion about mean");
			double sumReal = 0.0;
			double sumImag = 0.0;
			double *amp = amplitudes_.data();
			double *end = amp + amplitudes_.size();
			for(; amp != end; amp += 2) {
				sumReal += amp[0];
				sumImag += amp[1];
			}
			inversionAboutMean({
				sumReal / static_cast<double>(numStates_),
				sumImag / static_cast<double>(numStates_)
			});
		}

		/** @brief Applies inversion-about-mean using a precomputed mean amplitude. */
		void inversionAboutMean(Amplitude mean) override {
			ensureInitialized("inversion about mean");
			double *amp = amplitudes_.data();
			double *end = amp + amplitudes_.size();
			for(; amp != end; amp += 2) {
				const double real = amp[0];
				const double imag = amp[1];
				amp[0] = 2.0 * mean.real - real;
				amp[1] = 2.0 * mean.imag - imag;
			}
		}

		void applyAffineTransform(Amplitude scale, Amplitude bias) override {
			ensureInitialized("affine transform");
			double *amp = amplitudes_.data();
			double *end = amp + amplitudes_.size();
			for(; amp != end; amp += 2) {
				const double real = amp[0];
				const double imag = amp[1];
				amp[0] = scale.real * real - scale.imag * imag + bias.real;
				amp[1] = scale.real * imag + scale.imag * real + bias.imag;
			}
		}

		/** @brief Applies Hadamard transform to one qubit. */
		void applyHadamard(QubitIndex qubit) override {
			validateSingleQubitOperation(qubit, "DenseStateBackend::applyHadamard");
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(qubit, numQubits_);
			const double invSqrt2 = 1.0 / std::sqrt(2.0);

			runPairKernel(targetMask, [&](unsigned int, unsigned int, double *a0, double *a1) {
				const double aReal = a0[0];
				const double aImag = a0[1];
				const double bReal = a1[0];
				const double bImag = a1[1];
				if(aReal == 0.0 && aImag == 0.0 && bReal == 0.0 && bImag == 0.0) {
					return;
				}
				a0[0] = (aReal + bReal) * invSqrt2;
				a0[1] = (aImag + bImag) * invSqrt2;
				a1[0] = (aReal - bReal) * invSqrt2;
				a1[1] = (aImag - bImag) * invSqrt2;
			});
		}

		/** @brief Applies Pauli-X to one qubit by swapping paired amplitudes. */
		void applyPauliX(QubitIndex qubit) override {
			validateSingleQubitOperation(qubit, "DenseStateBackend::applyPauliX");
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(qubit, numQubits_);
			runPairKernel(targetMask, [&](unsigned int, unsigned int, double *a0, double *a1) {
				if(a0[0] == a1[0] && a0[1] == a1[1]) {
					return;
				}
				std::swap(a0[0], a1[0]);
				std::swap(a0[1], a1[1]);
			});
		}

		/** @brief Applies a controlled phase-shift gate. */
		void applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) override {
			validateTwoQubitOperation(controlQubit, targetQubit, "DenseStateBackend::applyControlledPhaseShift");
			const unsigned int controlMask = storage::qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(targetQubit, numQubits_);
			const double phaseReal = std::cos(theta);
			const double phaseImag = std::sin(theta);
			runPairKernel(targetMask, [&](unsigned int, unsigned int state1, double *, double *a1) {
				if((state1 & controlMask) == 0u) return;
				const double real = a1[0];
				const double imag = a1[1];
				if(real == 0.0 && imag == 0.0) return;
				a1[0] = phaseReal * real - phaseImag * imag;
				a1[1] = phaseReal * imag + phaseImag * real;
			});
		}

		/** @brief Applies controlled-NOT by swapping target-paired amplitudes. */
		void applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) override {
			validateTwoQubitOperation(controlQubit, targetQubit, "DenseStateBackend::applyControlledNot");
			const unsigned int controlMask = storage::qubitMaskFromMsbIndex(controlQubit, numQubits_);
			const unsigned int targetMask = storage::qubitMaskFromMsbIndex(targetQubit, numQubits_);
			runPairKernel(targetMask, [&](unsigned int state0, unsigned int, double *a0, double *a1) {
				if((state0 & controlMask) == 0u) return;
				if(a0[0] == a1[0] && a0[1] == a1[1]) {
					return;
				}
				std::swap(a0[0], a1[0]);
				std::swap(a0[1], a1[1]);
			});
		}

		/** @brief Applies an arbitrary gate through the shared block gate engine. */
		void applyGate(const QuantumGate &gate, const QubitList &qubits) override {
			ensureInitialized("gate application");
			// Adapt dense storage to the generic block gate engine callbacks.
			auto loadAmplitude = [&](StateIndex state) -> Amplitude {
				return {amplitudes_[state * 2], amplitudes_[state * 2 + 1]};
			};
			auto storeAmplitude = [&](StateIndex state, Amplitude amp) {
				amplitudes_[state * 2] = amp.real;
				amplitudes_[state * 2 + 1] = amp.imag;
			};
			storage::GateApplyEngine::apply(
				"DenseStateBackend::applyGate",
				gate,
				qubits,
				numQubits_,
				loadAmplitude,
				storeAmplitude,
				gateWorkspace_);
		}
};

/**
 * @brief Factory helper that builds a dense backend instance.
 */
std::unique_ptr<IStateBackend> createDenseStateBackend(const RegisterConfig &cfg) {
	return std::make_unique<DenseStateBackend>(cfg);
}

} // namespace tmfqs
