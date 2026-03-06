#ifndef QUANTUM_REGISTER_INCLUDE
#define QUANTUM_REGISTER_INCLUDE

#include <iosfwd>
#include <memory>

#include "quantumGate.h"
#include "storage/IStateBackend.h"
#include "storage/StateBackendFactory.h"
#include "types.h"

// Main user-facing quantum state object.
// It delegates storage and gate-application details to a runtime-selected backend.
class QuantumRegister {

	public:
		// Empty register (zero qubits, no allocated state).
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
		unsigned int stateCount() const;
		// Backward-compatible alias for stateCount().
		int getSize() const;

		// State queries.
		double probability(unsigned int state) const;
		double probabilitySumatory() const;
		Amplitude amplitude(unsigned int state) const;

		// State mutation helpers.
		void setAmplitude(unsigned int state, Amplitude amp);
		void loadAmplitudes(AmplitudesVector amplitudes);
		// Initializes selected basis states with equal magnitudes.
		void initUniformSuperposition(const StatesVector &basisStates);

		// Resolved backend after Auto selection.
		StorageStrategyKind storageStrategy() const;

		// Debug printing.
		void printStatesVector() const;
		friend std::ostream &operator << (std::ostream &os, const QuantumRegister &reg);

		~QuantumRegister();

		// Gate application APIs.
		void applyGate(const QuantumGate &g, const IntegerVector &v);
		void Hadamard(unsigned int qubit);
		void ControlledPhaseShift(unsigned int controlQubit, unsigned int targetQubit, double theta);
		void ControlledNot(unsigned int controlQubit, unsigned int targetQubit);
		void Swap(unsigned int qubit1, unsigned int qubit2);

	private:
		unsigned int numQubits_ = 0;
		unsigned int numStates_ = 0;
		RegisterConfig config_{};
		StorageStrategyKind resolvedStrategy_ = StorageStrategyKind::Dense;
		std::unique_ptr<IStateBackend> backend_;

		// Creates and initializes the backend selected by config_.
		void initializeBackend(unsigned int qubits);
};

#endif //QUANTUM_REGISTER_INCLUDE
