#ifndef TMFQS_REGISTER_QUANTUM_REGISTER_H
#define TMFQS_REGISTER_QUANTUM_REGISTER_H

#include <cstddef>
#include <iosfwd>
#include <memory>

#include "tmfqs/config/register_config.h"
#include "tmfqs/core/types.h"
#include "tmfqs/gates/quantum_gate.h"

namespace tmfqs {

class IRandomSource;
class IStateBackend;

/**
 * @brief High-level quantum register facade over a pluggable state backend.
 *
 * This class owns backend state, validates inputs, and exposes the gate and
 * measurement operations used by algorithms.
 */
class QuantumRegister {
	public:
		/** @brief Creates a default register initialized to `|0>`. */
		QuantumRegister();
		/**
		 * @brief Creates a register initialized to basis state `|0...0>`.
		 * @param numQubits Number of qubits.
		 * @param cfg Backend and compression configuration.
		 */
		explicit QuantumRegister(unsigned int numQubits, const RegisterConfig &cfg = {});
		/**
		 * @brief Creates a register initialized to one basis state with amplitude 1.
		 * @param numQubits Number of qubits.
		 * @param initState Basis-state index to initialize.
		 * @param cfg Backend and compression configuration.
		 */
		QuantumRegister(unsigned int numQubits, unsigned int initState, const RegisterConfig &cfg = {});
		/**
		 * @brief Creates a register initialized with custom amplitude on one state.
		 * @param numQubits Number of qubits.
		 * @param initState Basis-state index to initialize.
		 * @param amp Complex amplitude for `initState`.
		 * @param cfg Backend and compression configuration.
		 */
		QuantumRegister(unsigned int numQubits, unsigned int initState, Amplitude amp, const RegisterConfig &cfg = {});
		/**
		 * @brief Creates a register from a full interleaved amplitude vector.
		 * @param numQubits Number of qubits.
		 * @param amplitudes Interleaved amplitudes `[real0, imag0, ...]`.
		 * @param cfg Backend and compression configuration.
		 */
		QuantumRegister(unsigned int numQubits, AmplitudesVector amplitudes, const RegisterConfig &cfg = {});
		/** @brief Deep copy constructor. */
		QuantumRegister(const QuantumRegister &);
		/** @brief Move constructor. */
		QuantumRegister(QuantumRegister&&) noexcept = default;
		/** @brief Deep copy assignment. */
		QuantumRegister& operator=(const QuantumRegister &);
		/** @brief Move assignment. */
		QuantumRegister& operator=(QuantumRegister&&) noexcept = default;
		/** @brief Destructor. */
		~QuantumRegister();

		/**
		 * @brief Returns the number of qubits.
		 * @return Register qubit count.
		 */
		unsigned int qubitCount() const;
		/**
		 * @brief Returns total number of basis states.
		 * @return `2^qubitCount()`.
		 */
		StateIndex stateCount() const;
		/**
		 * @brief Returns number of `double` entries required to store all amplitudes.
		 * @return `2 * stateCount()`.
		 */
		size_t amplitudeElementCount() const;

		/**
		 * @brief Returns probability mass of one basis state.
		 * @param state Basis-state index.
		 * @return `|amplitude(state)|^2`.
		 */
		double probability(StateIndex state) const;
		/**
		 * @brief Returns total probability mass of the register.
		 * @return Sum of per-state probabilities.
		 */
		double totalProbability() const;
		/**
		 * @brief Returns complex amplitude of a basis state.
		 * @param state Basis-state index.
		 * @return Complex amplitude value.
		 */
		Amplitude amplitude(StateIndex state) const;
		/**
		 * @brief Samples one basis state according to current probabilities.
		 * @param randomSource Random source used to generate a sample in [0, 1).
		 * @return Measured basis-state index.
		 */
		StateIndex measure(IRandomSource &randomSource) const;

		/**
		 * @brief Sets the amplitude of one basis state.
		 * @param state Basis-state index.
		 * @param amp New complex amplitude value.
		 */
		void setAmplitude(StateIndex state, Amplitude amp);
		/**
		 * @brief Replaces all amplitudes.
		 * @param amplitudes Interleaved amplitudes `[real0, imag0, ...]`.
		 */
		void loadAmplitudes(AmplitudesVector amplitudes);
		/**
		 * @brief Initializes an equal superposition over selected basis states.
		 * @param basisStates States that receive equal real amplitude.
		 */
		void initUniformSuperposition(const BasisStateList &basisStates);

		/**
		 * @brief Returns the resolved storage strategy used by this instance.
		 * @return Active backend strategy.
		 */
		StorageStrategyKind storageStrategy() const;

		/**
		 * @brief Prints non-negligible amplitudes to standard output.
		 * @param epsilon Absolute threshold below which values are omitted.
		 */
		void printStatesVector(double epsilon = 1e-12) const;
		/**
		 * @brief Streams non-negligible amplitudes to an output stream.
		 * @param os Destination stream.
		 * @param reg Register to print.
		 * @return Reference to `os`.
		 */
		friend std::ostream &operator<<(std::ostream &os, const QuantumRegister &reg);

		/**
		 * @brief Applies an arbitrary gate matrix to selected qubits.
		 * @param gate Gate matrix.
		 * @param qubits Ordered target qubits.
		 */
		void applyGate(const QuantumGate &gate, const QubitList &qubits);
		/**
		 * @brief Applies a phase flip (`-1`) to one basis state.
		 * @param state Basis-state index.
		 */
		void applyPhaseFlipBasisState(StateIndex state);
		/** @brief Applies inversion about mean to the full register. */
		void applyInversionAboutMean();
		/**
		 * @brief Applies inversion about mean using a caller-provided mean amplitude.
		 * @param mean Mean complex amplitude across the full register.
		 */
		void applyInversionAboutMean(Amplitude mean);
		/**
		 * @brief Applies a single-qubit Hadamard gate.
		 * @param qubit Target qubit.
		 */
		void applyHadamard(QubitIndex qubit);
		/**
		 * @brief Applies a single-qubit Pauli-X gate.
		 * @param qubit Target qubit.
		 */
		void applyPauliX(QubitIndex qubit);
		/**
		 * @brief Applies a controlled phase-shift gate.
		 * @param controlQubit Control qubit index.
		 * @param targetQubit Target qubit index.
		 * @param theta Rotation angle in radians.
		 */
		void applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta);
		/**
		 * @brief Applies a controlled-NOT gate.
		 * @param controlQubit Control qubit index.
		 * @param targetQubit Target qubit index.
		 */
		void applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit);
		/**
		 * @brief Applies a SWAP gate (implemented as three CNOTs).
		 * @param qubit1 First qubit.
		 * @param qubit2 Second qubit.
		 */
		void applySwap(QubitIndex qubit1, QubitIndex qubit2);
		/** @brief Starts a backend-specific mutation batch. */
		void beginOperationBatch();
		/** @brief Ends a mutation batch and flushes pending updates if needed. */
		void endOperationBatch();

	private:
		/** @brief Number of logical qubits represented by the register. */
		unsigned int numQubits_ = 0;
		/** @brief Number of basis states (`2^numQubits_`). */
		unsigned int numStates_ = 0;
		/** @brief User-provided configuration options. */
		RegisterConfig config_{};
		/** @brief Strategy resolved after availability and auto-selection logic. */
		StorageStrategyKind resolvedStrategy_ = StorageStrategyKind::Dense;
		/** @brief Active state backend implementation. */
		std::unique_ptr<IStateBackend> backend_;
		/** @brief Global affine scale used to lazily represent full-register transforms. */
		Amplitude affineScale_{1.0, 0.0};
		/** @brief Global affine bias used to lazily represent full-register transforms. */
		Amplitude affineBias_{0.0, 0.0};
		/** @brief Indicates whether the lazy affine overlay is active. */
		bool affineOverlayActive_ = false;

		/**
		 * @brief Creates backend instance and updates cached size metadata.
		 * @param qubits Register size used for initialization.
		 */
		void initializeBackend(unsigned int qubits);
		/**
		 * @brief Validates that backend storage exists.
		 * @param operation Name of the operation requesting initialized state.
		 */
		void requireInitialized(const char *operation) const;
		/** @brief Resets lazy affine overlay state to identity. */
		void resetAffineOverlay();
		/** @brief Materializes the logical overlay back into backend storage. */
		void flushAffineOverlay();
		/** @brief Applies the lazy affine overlay to one backend amplitude. */
		Amplitude applyAffineOverlay(Amplitude backendAmplitude) const;
		/** @brief Converts one logical amplitude through the inverse affine overlay. */
		Amplitude removeAffineOverlay(Amplitude logicalAmplitude) const;
		/** @brief Computes the sum of all logical amplitudes. */
		Amplitude logicalAmplitudeSum() const;
		/** @brief Exports the current logical state, including any active overlay. */
		AmplitudesVector snapshotLogicalAmplitudes() const;
};

} // namespace tmfqs

#endif // TMFQS_REGISTER_QUANTUM_REGISTER_H
