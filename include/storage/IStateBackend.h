#ifndef I_STATE_BACKEND_INCLUDE
#define I_STATE_BACKEND_INCLUDE

#include <memory>
#include <iosfwd>
#include "types.h"
#include "quantumGate.h"

// Storage strategy interface used by QuantumRegister.
// Implementations may store amplitudes densely, compressed, or with other layouts.
// Contract: invalid public inputs must throw (typically std::invalid_argument or std::out_of_range).
class IStateBackend {

	public:
		virtual ~IStateBackend() = default;

		// Polymorphic deep copy used by QuantumRegister copy operations.
		virtual std::unique_ptr<IStateBackend> clone() const = 0;

		// Initialization routines.
		virtual void initZero(unsigned int numQubits) = 0;
		virtual void initBasis(unsigned int numQubits, StateIndex initState, Amplitude amp) = 0;
		// Native sparse/uniform initialization (no temporary dense buffer required).
		virtual void initUniformSuperposition(unsigned int numQubits, const BasisStateList &basisStates) = 0;
		// Dense import path for compatibility with previous API.
		virtual void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) = 0;

		// Point queries and updates.
		virtual Amplitude amplitude(StateIndex state) const = 0;
		virtual void setAmplitude(StateIndex state, Amplitude amp) = 0;
		virtual double probability(StateIndex state) const = 0;
		virtual double totalProbability() const = 0;
		virtual StateIndex sampleMeasurement(double rnd) const = 0;
		virtual void printNonZeroStates(std::ostream &os, double epsilon) const = 0;

		// Algorithm-native transforms that avoid dense gate materialization.
		virtual void phaseFlipBasisState(StateIndex state) = 0;
		virtual void inversionAboutMean() = 0;
		virtual void applyHadamard(QubitIndex qubit) = 0;
		virtual void applyPauliX(QubitIndex qubit) = 0;
		virtual void applyControlledPhaseShift(
			QubitIndex controlQubit, QubitIndex targetQubit, double theta) = 0;
		virtual void applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) = 0;

		// Applies a k-qubit gate on the provided target/control qubit indexes.
		virtual void applyGate(const QuantumGate &gate, const QubitList &qubits) = 0;
};

#endif // I_STATE_BACKEND_INCLUDE
