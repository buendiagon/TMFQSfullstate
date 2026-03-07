#ifndef QUANTUM_GATE_INCLUDE
#define QUANTUM_GATE_INCLUDE

#include <iosfwd>
#include <vector>

#include "types.h"

// Dense matrix representation of a quantum gate.
// Matrix entries are stored in row-major order.
class QuantumGate {

	private:
		std::vector<Amplitude> matrix_;
		unsigned int dimension_ = 0;

	public:
		QuantumGate() = default;
		// Builds an empty square gate matrix of size dimension x dimension.
		explicit QuantumGate(unsigned int dimension);
		QuantumGate(const QuantumGate&) = default;
		QuantumGate(QuantumGate&&) noexcept = default;
		QuantumGate& operator=(const QuantumGate&) = default;
		QuantumGate& operator=(QuantumGate&&) noexcept = default;
		~QuantumGate() = default;

		// Row access helper so callers can use gate[i][j] style indexing.
		Amplitude * operator[](unsigned int i);
		const Amplitude * operator[](unsigned int i) const;
		// Matrix dimension (e.g., 2 for single-qubit, 4 for two-qubit gates).
		unsigned int dimension() const noexcept;

		// Scalar and matrix multiplication.
		QuantumGate operator*(Amplitude x) const;
		QuantumGate operator*(const QuantumGate &qg) const;

		// Human-readable printing utilities.
		void printQuantumGate() const;
		friend std::ostream &operator<<(std::ostream &os, const QuantumGate &qg);

		// Standard gates and gate builders.
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
		// Full n-qubit Fourier transform matrices.
		static QuantumGate QFT(unsigned int num_qubits);
		static QuantumGate IQFT(unsigned int num_qubits);
};

// For left multiplication.
QuantumGate operator*(Amplitude x, const QuantumGate &U);

#endif // QUANTUM_GATE_INCLUDE
