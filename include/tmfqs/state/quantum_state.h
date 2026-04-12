#ifndef TMFQS_STATE_QUANTUM_STATE_H
#define TMFQS_STATE_QUANTUM_STATE_H

#include <memory>
#include <vector>

#include "tmfqs/core/random.h"
#include "tmfqs/core/types.h"

namespace tmfqs {

struct RegisterConfig;
class QuantumRegister;

namespace sim {
class Simulator;
}

namespace state {

enum class NormalizationPolicy {
	Validate,
	Normalize,
	AllowUnnormalized
};

struct StateValidation {
	double probabilityTolerance = 1e-9;
	NormalizationPolicy normalization = NormalizationPolicy::Validate;
};

class QuantumState {
	public:
		QuantumState();
		QuantumState(const QuantumState &);
		QuantumState &operator=(const QuantumState &);
		QuantumState(QuantumState &&) noexcept;
		QuantumState &operator=(QuantumState &&) noexcept;
		~QuantumState();

		unsigned int qubitCount() const;
		StateIndex stateCount() const;
		Amplitude amplitude(StateIndex state) const;
		double totalProbability() const;
		AmplitudesVector amplitudes() const;
		StateIndex measure(IRandomSource &randomSource) const;

		void setAmplitude(StateIndex state, Amplitude amplitude, const StateValidation &validation = {});
		void loadAmplitudes(AmplitudesVector amplitudes, const StateValidation &validation = {});

		static QuantumState basis(unsigned int qubits, StateIndex state = 0u);
		static QuantumState basisAmplitude(unsigned int qubits, StateIndex state, Amplitude amplitude);
		static QuantumState uniformSubset(unsigned int qubits, BasisStateList states);
		static QuantumState fromAmplitudes(unsigned int qubits, AmplitudesVector amplitudes, const StateValidation &validation = {});
		static QuantumState randomPhase(unsigned int qubits, unsigned int seed);
		static QuantumState sparsePattern(unsigned int qubits);

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;

		explicit QuantumState(std::unique_ptr<Impl> impl);

		QuantumRegister materialize(const RegisterConfig &config) const;
		static QuantumState fromRegister(const QuantumRegister &reg);

		friend class tmfqs::sim::Simulator;
};

} // namespace state
} // namespace tmfqs

#endif // TMFQS_STATE_QUANTUM_STATE_H
