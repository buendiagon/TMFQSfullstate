#ifndef TMFQS_GATES_QUANTUM_GATE_H
#define TMFQS_GATES_QUANTUM_GATE_H

#include <iosfwd>
#include <vector>

#include "tmfqs/core/types.h"

namespace tmfqs {

class QuantumGate {
	private:
		std::vector<Amplitude> matrix_;
		unsigned int dimension_ = 0;

	public:
		QuantumGate() = default;
		explicit QuantumGate(unsigned int dimension);
		QuantumGate(const QuantumGate&) = default;
		QuantumGate(QuantumGate&&) noexcept = default;
		QuantumGate& operator=(const QuantumGate&) = default;
		QuantumGate& operator=(QuantumGate&&) noexcept = default;
		~QuantumGate() = default;

		Amplitude *operator[](unsigned int i);
		const Amplitude *operator[](unsigned int i) const;
		unsigned int dimension() const noexcept;

		QuantumGate operator*(Amplitude x) const;
		QuantumGate operator*(const QuantumGate &qg) const;

		void printQuantumGate() const;
		friend std::ostream &operator<<(std::ostream &os, const QuantumGate &qg);

		static QuantumGate Identity(unsigned int dimension);
		static QuantumGate Hadamard();
		static QuantumGate PauliX();
		static QuantumGate PauliY();
		static QuantumGate PauliZ();
		static QuantumGate PhaseShift(double theta);
		static QuantumGate PiOverEight();
		static QuantumGate ControlledNot();
		static QuantumGate Toffoli();
		static QuantumGate ControlledPhaseShift(double theta);
		static QuantumGate Swap();
		static QuantumGate Ising(double theta);
		static QuantumGate QFT(unsigned int numQubits);
		static QuantumGate IQFT(unsigned int numQubits);
};

QuantumGate operator*(Amplitude x, const QuantumGate &U);

} // namespace tmfqs

#endif // TMFQS_GATES_QUANTUM_GATE_H
