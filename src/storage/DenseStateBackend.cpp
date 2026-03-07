#include "storage/IStateBackend.h"
#include "storage/GateBlockApply.h"
#include "stateSpace.h"
#include "validation.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Dense backend: full uncompressed state vector in memory.
class DenseStateBackend : public IStateBackend {

	private:
		unsigned int numQubits_ = 0;
		unsigned int numStates_ = 0;
		AmplitudesVector amplitudes_;
		GateBlockWorkspace gateWorkspace_;

		void ensureInitialized(const char *operation) const {
			if(numStates_ == 0 || amplitudes_.empty()) {
				throw std::logic_error(std::string("DenseStateBackend is not initialized for ") + operation);
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
				throw std::out_of_range("DenseStateBackend single-qubit operation qubit index out of range");
			}
		}

		void validateTwoQubitOperation(
			QubitIndex controlQubit, QubitIndex targetQubit, const char *opName) const {
			ensureInitialized(opName);
			if(controlQubit >= numQubits_ || targetQubit >= numQubits_) {
				throw std::out_of_range("DenseStateBackend two-qubit operation qubit index out of range");
			}
			if(controlQubit == targetQubit) {
				throw std::invalid_argument("DenseStateBackend two-qubit operation requires distinct qubits");
			}
		}

		void validateGateApplicationInputs(const QuantumGate &gate, const QubitList &qubits) const {
			ensureInitialized("gate application");
			validateGateTargets("DenseStateBackend::applyGate", qubits, numQubits_, gate.dimension());
		}

		static unsigned int qubitMask(unsigned int qubit, unsigned int numQubits) {
			return 1u << (numQubits - qubit - 1u);
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

		void initBasis(unsigned int numQubits, StateIndex initState, Amplitude amp) override {
			initZero(numQubits);
			if(initState >= numStates_) {
				throw std::out_of_range("DenseStateBackend::initBasis state index out of range");
			}
			amplitudes_[2 * initState] = amp.real;
			amplitudes_[2 * initState + 1] = amp.imag;
		}

		void initUniformSuperposition(unsigned int numQubits, const BasisStateList &basisStates) override {
			numQubits_ = numQubits;
			numStates_ = checkedStateCount(numQubits_);
			amplitudes_.assign(checkedAmplitudeElementCount(numQubits_), 0.0);

			std::vector<StateIndex> selected;
			selected.reserve(basisStates.size());
			for(StateIndex state : basisStates) {
				if(state >= numStates_) {
					throw std::out_of_range("DenseStateBackend::initUniformSuperposition basis state index out of range");
				}
				selected.push_back(state);
			}
			std::sort(selected.begin(), selected.end());
			selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
			if(selected.empty()) {
				throw std::invalid_argument("DenseStateBackend: no valid basis states for uniform initialization");
			}

			const double realAmplitude = 1.0 / std::sqrt(static_cast<double>(selected.size()));
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

		Amplitude amplitude(StateIndex state) const override {
			ensureInitialized("amplitude query");
			validateState(state);
			return {amplitudes_[state * 2], amplitudes_[state * 2 + 1]};
		}

		void setAmplitude(StateIndex state, Amplitude amp) override {
			ensureInitialized("state update");
			validateState(state);
			amplitudes_[state * 2] = amp.real;
			amplitudes_[state * 2 + 1] = amp.imag;
		}

		double probability(StateIndex state) const override {
			Amplitude amp = amplitude(state);
			return amp.real * amp.real + amp.imag * amp.imag;
		}

		double totalProbability() const override {
			ensureInitialized("total probability query");
			double sum = 0.0;
			for(unsigned int i = 0; i < numStates_; ++i) {
				double real = amplitudes_[i * 2];
				double imag = amplitudes_[i * 2 + 1];
				sum += real * real + imag * imag;
			}
			return sum;
		}

		StateIndex sampleMeasurement(double rnd) const override {
			ensureInitialized("measurement");
			if(rnd < 0.0 || rnd >= 1.0) {
				throw std::invalid_argument("DenseStateBackend::sampleMeasurement requires rnd in [0,1)");
			}
			double cumulative = 0.0;
			for(StateIndex state = 0; state < numStates_; ++state) {
				const double real = amplitudes_[state * 2];
				const double imag = amplitudes_[state * 2 + 1];
				cumulative += real * real + imag * imag;
				if(rnd <= cumulative) {
					return state;
				}
			}
			throw std::runtime_error("DenseStateBackend::sampleMeasurement cumulative probability did not reach sample");
		}

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

		void phaseFlipBasisState(StateIndex state) override {
			validateState(state);
			amplitudes_[2 * state] = -amplitudes_[2 * state];
			amplitudes_[2 * state + 1] = -amplitudes_[2 * state + 1];
		}

		void inversionAboutMean() override {
			ensureInitialized("inversion about mean");
			double sumReal = 0.0;
			double sumImag = 0.0;
			for(unsigned int state = 0; state < numStates_; ++state) {
				sumReal += amplitudes_[2 * state];
				sumImag += amplitudes_[2 * state + 1];
			}
			const double meanReal = sumReal / static_cast<double>(numStates_);
			const double meanImag = sumImag / static_cast<double>(numStates_);
			for(unsigned int state = 0; state < numStates_; ++state) {
				double real = amplitudes_[2 * state];
				double imag = amplitudes_[2 * state + 1];
				amplitudes_[2 * state] = 2.0 * meanReal - real;
				amplitudes_[2 * state + 1] = 2.0 * meanImag - imag;
			}
		}

		void applyHadamard(QubitIndex qubit) override {
			validateSingleQubitOperation(qubit, "Hadamard");
			const unsigned int targetMask = qubitMask(qubit, numQubits_);
			const double invSqrt2 = 1.0 / std::sqrt(2.0);

			for(unsigned int state = 0; state < numStates_; ++state) {
				if((state & targetMask) != 0) continue;
				const unsigned int pairedState = state | targetMask;
				const double aReal = amplitudes_[2 * state];
				const double aImag = amplitudes_[2 * state + 1];
				const double bReal = amplitudes_[2 * pairedState];
				const double bImag = amplitudes_[2 * pairedState + 1];

				amplitudes_[2 * state] = (aReal + bReal) * invSqrt2;
				amplitudes_[2 * state + 1] = (aImag + bImag) * invSqrt2;
				amplitudes_[2 * pairedState] = (aReal - bReal) * invSqrt2;
				amplitudes_[2 * pairedState + 1] = (aImag - bImag) * invSqrt2;
			}
		}

		void applyPauliX(QubitIndex qubit) override {
			validateSingleQubitOperation(qubit, "PauliX");
			const unsigned int targetMask = qubitMask(qubit, numQubits_);

			for(unsigned int state = 0; state < numStates_; ++state) {
				if((state & targetMask) != 0) continue;
				const unsigned int pairedState = state | targetMask;
				std::swap(amplitudes_[2 * state], amplitudes_[2 * pairedState]);
				std::swap(amplitudes_[2 * state + 1], amplitudes_[2 * pairedState + 1]);
			}
		}

		void applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) override {
			validateTwoQubitOperation(controlQubit, targetQubit, "controlled phase shift");
			const unsigned int controlMask = qubitMask(controlQubit, numQubits_);
			const unsigned int targetMask = qubitMask(targetQubit, numQubits_);
			const double phaseReal = std::cos(theta);
			const double phaseImag = std::sin(theta);

			for(unsigned int state = 0; state < numStates_; ++state) {
				if((state & controlMask) == 0 || (state & targetMask) == 0) continue;
				const double real = amplitudes_[2 * state];
				const double imag = amplitudes_[2 * state + 1];
				amplitudes_[2 * state] = phaseReal * real - phaseImag * imag;
				amplitudes_[2 * state + 1] = phaseReal * imag + phaseImag * real;
			}
		}

		void applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) override {
			validateTwoQubitOperation(controlQubit, targetQubit, "controlled not");
			const unsigned int controlMask = qubitMask(controlQubit, numQubits_);
			const unsigned int targetMask = qubitMask(targetQubit, numQubits_);

			for(unsigned int state = 0; state < numStates_; ++state) {
				if((state & controlMask) == 0 || (state & targetMask) != 0) continue;
				const unsigned int pairedState = state | targetMask;
				std::swap(amplitudes_[2 * state], amplitudes_[2 * pairedState]);
				std::swap(amplitudes_[2 * state + 1], amplitudes_[2 * pairedState + 1]);
			}
		}

		void applyGate(const QuantumGate &gate, const QubitList &qubits) override {
			validateGateApplicationInputs(gate, qubits);
			GateBlockLayout layout = makeGateBlockLayout(qubits, numQubits_);

			auto loadAmplitude = [&](unsigned int state) -> Amplitude {
				return {amplitudes_[state * 2], amplitudes_[state * 2 + 1]};
			};
			auto storeAmplitude = [&](unsigned int state, Amplitude amp) {
				amplitudes_[state * 2] = amp.real;
				amplitudes_[state * 2 + 1] = amp.imag;
			};
			applyGateByBlocks(gate, layout, loadAmplitude, storeAmplitude, gateWorkspace_);
		}
};

std::unique_ptr<IStateBackend> createDenseBackend(unsigned int numQubits, const RegisterConfig &) {
	(void)numQubits;
	return std::make_unique<DenseStateBackend>();
}
