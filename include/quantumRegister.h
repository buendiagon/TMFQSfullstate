#ifndef QUANTUM_REGISTER_INCLUDE
#define QUANTUM_REGISTER_INCLUDE

#include <cstddef>
#include <iosfwd>
#include <memory>

#include "quantumGate.h"
#include "storage/IStateBackend.h"
#include "storage/StateBackendFactory.h"
#include "types.h"

// Main user-facing quantum state object.
// It delegates storage and gate-application details to a runtime-selected backend.
// Public API contract: invalid inputs throw exceptions instead of silently no-oping.
class QuantumRegister {

	public:
		// Valid 0-qubit register initialized to amplitude 1 for the only basis state.
		QuantumRegister();
		// Creates |0...0> with the requested backend strategy.
		explicit QuantumRegister(unsigned int numQubits, const RegisterConfig &cfg = {});
		// Creates a basis state |initState> with amplitude 1 + 0i.
		QuantumRegister(unsigned int numQubits, unsigned int initState, const RegisterConfig &cfg = {});
		// Creates a basis state |initState> with custom amplitude.
		QuantumRegister(unsigned int numQubits, unsigned int initState, Amplitude amp, const RegisterConfig &cfg = {});
		// Loads an interleaved dense amplitude buffer.
		QuantumRegister(unsigned int numQubits, AmplitudesVector amplitudes, const RegisterConfig &cfg = {});
		QuantumRegister(const QuantumRegister &);
		QuantumRegister(QuantumRegister&&) noexcept = default;
		QuantumRegister& operator=(const QuantumRegister &);
		QuantumRegister& operator=(QuantumRegister&&) noexcept = default;

		// Register shape.
		unsigned int qubitCount() const;
		StateIndex stateCount() const;
		size_t amplitudeElementCount() const;

		// State queries.
		double probability(StateIndex state) const;
		double totalProbability() const;
		Amplitude amplitude(StateIndex state) const;
		StateIndex measure() const;

		// State mutation helpers.
		void setAmplitude(StateIndex state, Amplitude amp);
		void loadAmplitudes(AmplitudesVector amplitudes);
		// Initializes selected basis states with equal magnitudes.
		void initUniformSuperposition(const BasisStateList &basisStates);

		// Resolved backend after Auto selection.
		StorageStrategyKind storageStrategy() const;

		// Debug printing.
		void printStatesVector(double epsilon = 1e-12) const;
		friend std::ostream &operator << (std::ostream &os, const QuantumRegister &reg);

		~QuantumRegister();

		// Gate application APIs.
		void applyGate(const QuantumGate &g, const QubitList &v);
		// Flips the phase of one computational basis state |state>.
		void phaseFlipBasisState(StateIndex state);
		// Reflection around the global mean amplitude (Grover diffusion step).
		void inversionAboutMean();
		void Hadamard(QubitIndex qubit);
		void PauliX(QubitIndex qubit);
		void ControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta);
		void ControlledNot(QubitIndex controlQubit, QubitIndex targetQubit);
		void Swap(QubitIndex qubit1, QubitIndex qubit2);

	private:
		unsigned int numQubits_ = 0;
		unsigned int numStates_ = 0;
		RegisterConfig config_{};
		StorageStrategyKind resolvedStrategy_ = StorageStrategyKind::Dense;
		std::unique_ptr<IStateBackend> backend_;

		// Creates and initializes the backend selected by config_.
		void initializeBackend(unsigned int qubits);
		// Ensures the register has an initialized backend before operations.
		void requireInitialized(const char *operation) const;
		void validateStateIndex(StateIndex state, const char *operation) const;
		void validateSingleQubit(QubitIndex qubit, const char *operation) const;
		void validateTwoQubit(QubitIndex q0, QubitIndex q1, const char *operation) const;
};

#endif //QUANTUM_REGISTER_INCLUDE
