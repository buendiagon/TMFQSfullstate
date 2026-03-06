#ifndef I_STATE_BACKEND_INCLUDE
#define I_STATE_BACKEND_INCLUDE

#include <memory>
#include "types.h"
#include "quantumGate.h"

// Storage strategy interface used by QuantumRegister.
// Implementations may store amplitudes densely, compressed, or with other layouts.
class IStateBackend {

	public:
		virtual ~IStateBackend() = default;

		// Polymorphic deep copy used by QuantumRegister copy operations.
		virtual std::unique_ptr<IStateBackend> clone() const = 0;

		// Initialization routines.
		virtual void initZero(unsigned int numQubits) = 0;
		virtual void initBasis(unsigned int numQubits, unsigned int initState, Amplitude amp) = 0;
		// Native sparse/uniform initialization (no temporary dense buffer required).
		virtual void initUniformSuperposition(unsigned int numQubits, const StatesVector &basisStates) = 0;
		// Dense import path for compatibility with previous API.
		virtual void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) = 0;

		// Point queries and updates.
		virtual Amplitude amplitude(unsigned int state) const = 0;
		virtual void setAmplitude(unsigned int state, Amplitude amp) = 0;
		virtual double probability(unsigned int state) const = 0;
		virtual double probabilitySumatory() const = 0;

		// Applies a k-qubit gate on the provided target/control qubit indexes.
		virtual void applyGate(const QuantumGate &gate, const IntegerVector &qubits, unsigned int numQubits) = 0;
};

#endif // I_STATE_BACKEND_INCLUDE
