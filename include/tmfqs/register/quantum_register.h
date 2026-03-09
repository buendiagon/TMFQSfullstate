#ifndef TMFQS_REGISTER_QUANTUM_REGISTER_H
#define TMFQS_REGISTER_QUANTUM_REGISTER_H

#include <cstddef>
#include <iosfwd>
#include <memory>

#include "tmfqs/core/types.h"
#include "tmfqs/gates/quantum_gate.h"
#include "tmfqs/storage/i_state_backend.h"
#include "tmfqs/storage/state_backend_factory.h"

namespace tmfqs {

class IRandomSource;

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
		~QuantumRegister();

		unsigned int qubitCount() const;
		StateIndex stateCount() const;
		size_t amplitudeElementCount() const;

		double probability(StateIndex state) const;
		double totalProbability() const;
		Amplitude amplitude(StateIndex state) const;
		StateIndex measure(IRandomSource &randomSource) const;

		void setAmplitude(StateIndex state, Amplitude amp);
		void loadAmplitudes(AmplitudesVector amplitudes);
		void initUniformSuperposition(const BasisStateList &basisStates);

		StorageStrategyKind storageStrategy() const;

		void printStatesVector(double epsilon = 1e-12) const;
		friend std::ostream &operator<<(std::ostream &os, const QuantumRegister &reg);

		void applyGate(const QuantumGate &gate, const QubitList &qubits);
		void applyPhaseFlipBasisState(StateIndex state);
		void applyInversionAboutMean();
		void applyHadamard(QubitIndex qubit);
		void applyPauliX(QubitIndex qubit);
		void applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta);
		void applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit);
		void applySwap(QubitIndex qubit1, QubitIndex qubit2);
		void beginOperationBatch();
		void endOperationBatch();

	private:
		unsigned int numQubits_ = 0;
		unsigned int numStates_ = 0;
		RegisterConfig config_{};
		StorageStrategyKind resolvedStrategy_ = StorageStrategyKind::Dense;
		std::unique_ptr<IStateBackend> backend_;

		void initializeBackend(unsigned int qubits);
		void requireInitialized(const char *operation) const;
};

} // namespace tmfqs

#endif // TMFQS_REGISTER_QUANTUM_REGISTER_H
