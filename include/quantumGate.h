#ifndef QUANTUM_GATE_INCLUDE
#define QUANTUM_GATE_INCLUDE

#include <iosfwd>
#include <vector>

#include "types.h"

class QuantumGate {

	private:
		std::vector<Amplitude> matrix_;

	public:
		unsigned int dimension = 0;

		QuantumGate() = default;
		explicit QuantumGate(unsigned int dimension);
		QuantumGate(const QuantumGate&) = default;
		QuantumGate(QuantumGate&&) noexcept = default;
		QuantumGate& operator=(const QuantumGate&) = default;
		QuantumGate& operator=(QuantumGate&&) noexcept = default;
		~QuantumGate() = default;

		Amplitude * operator[](unsigned int i);
		const Amplitude * operator[](unsigned int i) const;
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
		static QuantumGate QFT(unsigned int num_qubits);
		static QuantumGate IQFT(unsigned int num_qubits);
};

// For left multiplication.
QuantumGate operator*(Amplitude x, const QuantumGate &U);

#endif // QUANTUM_GATE_INCLUDE
