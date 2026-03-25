#ifndef TMFQS_STORAGE_I_STATE_BACKEND_H
#define TMFQS_STORAGE_I_STATE_BACKEND_H

#include <iosfwd>
#include <memory>

#include "tmfqs/core/types.h"
#include "tmfqs/gates/quantum_gate.h"

namespace tmfqs {

/**
 * @brief Polymorphic storage backend interface for quantum state operations.
 *
 * Concrete implementations can store amplitudes densely or in compressed form,
 * but must preserve identical logical behavior.
 */
class IStateBackend {
	public:
		/** @brief Virtual destructor for polymorphic use. */
		virtual ~IStateBackend() = default;

		/**
		 * @brief Creates a deep copy of the backend and its current state.
		 * @return Owning pointer to the cloned backend.
		 */
		virtual std::unique_ptr<IStateBackend> clone() const = 0;

		/**
		 * @brief Initializes all amplitudes to zero.
		 * @param numQubits Register size.
		 */
		virtual void initZero(unsigned int numQubits) = 0;
		/**
		 * @brief Initializes one basis state with a custom amplitude.
		 * @param numQubits Register size.
		 * @param initState Basis-state index to initialize.
		 * @param amp Initial amplitude for `initState`.
		 */
		virtual void initBasis(unsigned int numQubits, StateIndex initState, Amplitude amp) = 0;
		/**
		 * @brief Initializes equal superposition over selected basis states.
		 * @param numQubits Register size.
		 * @param basisStates Set of basis states to include.
		 */
		virtual void initUniformSuperposition(unsigned int numQubits, const BasisStateList &basisStates) = 0;
		/**
		 * @brief Loads complete amplitudes from an interleaved vector.
		 * @param numQubits Register size.
		 * @param amplitudes Interleaved amplitudes `[real0, imag0, ...]`.
		 */
		virtual void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) = 0;

		/**
		 * @brief Reads amplitude for one basis state.
		 * @param state Basis-state index.
		 * @return Complex amplitude value.
		 */
		virtual Amplitude amplitude(StateIndex state) const = 0;
		/**
		 * @brief Writes amplitude for one basis state.
		 * @param state Basis-state index.
		 * @param amp New complex amplitude value.
		 */
		virtual void setAmplitude(StateIndex state, Amplitude amp) = 0;
		/**
		 * @brief Computes probability mass for one basis state.
		 * @param state Basis-state index.
		 * @return Probability value.
		 */
		virtual double probability(StateIndex state) const = 0;
		/**
		 * @brief Computes total probability mass across all basis states.
		 * @return Sum of all per-state probabilities.
		 */
		virtual double totalProbability() const = 0;
		/**
		 * @brief Samples a basis state from cumulative probability.
		 * @param rnd Random sample in [0, 1).
		 * @return Sampled basis-state index.
		 */
		virtual StateIndex sampleMeasurement(double rnd) const = 0;
		/**
		 * @brief Prints amplitudes whose absolute value exceeds `epsilon`.
		 * @param os Output stream.
		 * @param epsilon Absolute threshold for printing.
		 */
		virtual void printNonZeroStates(std::ostream &os, double epsilon) const = 0;

		/**
		 * @brief Applies a `-1` phase to one basis state.
		 * @param state Basis-state index.
		 */
		virtual void phaseFlipBasisState(StateIndex state) = 0;
		/** @brief Applies inversion about mean to all amplitudes. */
		virtual void inversionAboutMean() = 0;
		/**
		 * @brief Applies inversion about mean using a caller-provided mean amplitude.
		 * @param mean Mean complex amplitude across the full register.
		 */
		virtual void inversionAboutMean(Amplitude mean) = 0;
		/**
		 * @brief Applies a single-qubit Hadamard gate.
		 * @param qubit Target qubit index.
		 */
		virtual void applyHadamard(QubitIndex qubit) = 0;
		/**
		 * @brief Applies a single-qubit Pauli-X gate.
		 * @param qubit Target qubit index.
		 */
		virtual void applyPauliX(QubitIndex qubit) = 0;
		/**
		 * @brief Applies a controlled phase-shift gate.
		 * @param controlQubit Control qubit index.
		 * @param targetQubit Target qubit index.
		 * @param theta Rotation angle in radians.
		 */
		virtual void applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) = 0;
		/**
		 * @brief Applies a controlled-NOT gate.
		 * @param controlQubit Control qubit index.
		 * @param targetQubit Target qubit index.
		 */
		virtual void applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) = 0;

		/**
		 * @brief Applies an arbitrary gate matrix to selected qubits.
		 * @param gate Gate matrix.
		 * @param qubits Ordered target qubits.
		 */
		virtual void applyGate(const QuantumGate &gate, const QubitList &qubits) = 0;

		/**
		 * @brief Optional hook to begin batching multiple mutations.
		 *
		 * Implementations may defer expensive flush/write-back work until
		 * @ref endOperationBatch is called.
		 */
		virtual void beginOperationBatch() {}
		/**
		 * @brief Optional hook to end a mutation batch and flush pending updates.
		 */
		virtual void endOperationBatch() {}
};

} // namespace tmfqs

#endif // TMFQS_STORAGE_I_STATE_BACKEND_H
