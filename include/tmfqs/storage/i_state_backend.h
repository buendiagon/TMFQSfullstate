#ifndef TMFQS_STORAGE_I_STATE_BACKEND_H
#define TMFQS_STORAGE_I_STATE_BACKEND_H

#include <iosfwd>
#include <memory>

#include "tmfqs/core/types.h"
#include "tmfqs/gates/quantum_gate.h"

namespace tmfqs {

class IStateBackend {
	public:
		virtual ~IStateBackend() = default;

		virtual std::unique_ptr<IStateBackend> clone() const = 0;

		virtual void initZero(unsigned int numQubits) = 0;
		virtual void initBasis(unsigned int numQubits, StateIndex initState, Amplitude amp) = 0;
		virtual void initUniformSuperposition(unsigned int numQubits, const BasisStateList &basisStates) = 0;
		virtual void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) = 0;

		virtual Amplitude amplitude(StateIndex state) const = 0;
		virtual void setAmplitude(StateIndex state, Amplitude amp) = 0;
		virtual double probability(StateIndex state) const = 0;
		virtual double totalProbability() const = 0;
		virtual StateIndex sampleMeasurement(double rnd) const = 0;
		virtual void printNonZeroStates(std::ostream &os, double epsilon) const = 0;

		virtual void phaseFlipBasisState(StateIndex state) = 0;
		virtual void inversionAboutMean() = 0;
		virtual void applyHadamard(QubitIndex qubit) = 0;
		virtual void applyPauliX(QubitIndex qubit) = 0;
		virtual void applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) = 0;
		virtual void applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) = 0;

		virtual void applyGate(const QuantumGate &gate, const QubitList &qubits) = 0;
};

} // namespace tmfqs

#endif // TMFQS_STORAGE_I_STATE_BACKEND_H
