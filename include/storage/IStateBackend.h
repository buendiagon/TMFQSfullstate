#ifndef I_STATE_BACKEND_INCLUDE
#define I_STATE_BACKEND_INCLUDE

#include <memory>
#include "types.h"
#include "quantumGate.h"

class IStateBackend {

	public:
		virtual ~IStateBackend() = default;

		virtual std::unique_ptr<IStateBackend> clone() const = 0;
		virtual void initZero(unsigned int numQubits) = 0;
		virtual void initBasis(unsigned int numQubits, unsigned int initState, Amplitude amp) = 0;
		virtual void initUniformSuperposition(unsigned int numQubits, const StatesVector &basisStates) = 0;
		virtual void loadAmplitudes(unsigned int numQubits, AmplitudesVector amplitudes) = 0;
		virtual Amplitude amplitude(unsigned int state) const = 0;
		virtual void setAmplitude(unsigned int state, Amplitude amp) = 0;
		virtual double probability(unsigned int state) const = 0;
		virtual double probabilitySumatory() const = 0;
		virtual void applyGate(const QuantumGate &gate, const IntegerVector &qubits, unsigned int numQubits) = 0;
};

#endif // I_STATE_BACKEND_INCLUDE
