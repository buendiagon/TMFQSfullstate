#ifndef QUANTUM_REGISTER_INCLUDE
#define QUANTUM_REGISTER_INCLUDE

#include <iosfwd>
#include <memory>

#include "quantumGate.h"
#include "storage/IStateBackend.h"
#include "storage/StateBackendFactory.h"
#include "types.h"

class QuantumRegister {

	public:
		QuantumRegister();
		explicit QuantumRegister(unsigned int numQubits, const RegisterConfig &cfg = {});
		QuantumRegister(unsigned int numQubits, unsigned int initState, const RegisterConfig &cfg = {});
		QuantumRegister(unsigned int numQubits, unsigned int initState, Amplitude amp, const RegisterConfig &cfg = {});
		QuantumRegister(unsigned int numQubits, AmplitudesVector amplitudes, const RegisterConfig &cfg = {});
		QuantumRegister(const QuantumRegister &);
		QuantumRegister(QuantumRegister&&) noexcept = default;
		QuantumRegister& operator=(const QuantumRegister &);
		QuantumRegister& operator=(QuantumRegister&&) noexcept = default;

		unsigned int qubitCount() const;
		unsigned int stateCount() const;
		int getSize() const;
		double probability(unsigned int state) const;
		double probabilitySumatory() const;
		Amplitude amplitude(unsigned int state) const;
		void setAmplitude(unsigned int state, Amplitude amp);
		void loadAmplitudes(AmplitudesVector amplitudes);
		void initUniformSuperposition(const StatesVector &basisStates);
		StorageStrategyKind storageStrategy() const;

		void printStatesVector() const;
		friend std::ostream &operator << (std::ostream &os, const QuantumRegister &reg);

		~QuantumRegister();

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

		void initializeBackend(unsigned int qubits);
};

#endif //QUANTUM_REGISTER_INCLUDE
