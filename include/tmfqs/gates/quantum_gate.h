#ifndef TMFQS_GATES_QUANTUM_GATE_H
#define TMFQS_GATES_QUANTUM_GATE_H

#include <iosfwd>
#include <vector>

#include "tmfqs/core/types.h"

namespace tmfqs {

/**
 * @brief Square complex matrix representing a quantum gate.
 *
 * The matrix is stored in row-major order and usually has dimension `2^k` for
 * a gate acting on `k` qubits.
 */
class QuantumGate {
	private:
		/** @brief Flattened row-major matrix entries. */
		std::vector<Amplitude> matrix_;
		/** @brief Matrix side length. */
		unsigned int dimension_ = 0;

	public:
		/** @brief Creates an empty gate with dimension 0. */
		QuantumGate() = default;
		/**
		 * @brief Creates a zero-initialized square gate matrix.
		 * @param dimension Matrix side length.
		 */
		explicit QuantumGate(unsigned int dimension);
		/** @brief Copy constructor. */
		QuantumGate(const QuantumGate&) = default;
		/** @brief Move constructor. */
		QuantumGate(QuantumGate&&) noexcept = default;
		/** @brief Copy assignment operator. */
		QuantumGate& operator=(const QuantumGate&) = default;
		/** @brief Move assignment operator. */
		QuantumGate& operator=(QuantumGate&&) noexcept = default;
		/** @brief Destructor. */
		~QuantumGate() = default;

		/**
		 * @brief Returns mutable access to matrix row `i`.
		 * @param i Row index.
		 * @return Pointer to first element in row `i`.
		 * @throws std::out_of_range If `i >= dimension()`.
		 */
		Amplitude *operator[](unsigned int i);
		/**
		 * @brief Returns read-only access to matrix row `i`.
		 * @param i Row index.
		 * @return Pointer to first element in row `i`.
		 * @throws std::out_of_range If `i >= dimension()`.
		 */
		const Amplitude *operator[](unsigned int i) const;
		/**
		 * @brief Returns the matrix side length.
		 * @return Number of rows (and columns).
		 */
		unsigned int dimension() const noexcept;

		/**
		 * @brief Multiplies this gate by a complex scalar.
		 * @param x Scalar value.
		 * @return New gate equal to `x * this`.
		 */
		QuantumGate operator*(Amplitude x) const;
		/**
		 * @brief Multiplies two gates (matrix product).
		 * @param qg Right-hand gate.
		 * @return Product gate `this * qg`.
		 * @throws std::invalid_argument If dimensions differ.
		 */
		QuantumGate operator*(const QuantumGate &qg) const;

		/**
		 * @brief Prints the full matrix to stdout.
		 */
		void printQuantumGate() const;
		/**
		 * @brief Streams all matrix entries to an output stream.
		 * @param os Destination stream.
		 * @param qg Gate to print.
		 * @return Reference to `os`.
		 */
		friend std::ostream &operator<<(std::ostream &os, const QuantumGate &qg);

		/**
		 * @brief Creates an identity gate of arbitrary dimension.
		 * @param dimension Matrix side length.
		 * @return Identity matrix.
		 */
		static QuantumGate Identity(unsigned int dimension);
		/** @brief Creates the 1-qubit Hadamard gate. */
		static QuantumGate Hadamard();
		/** @brief Creates the 1-qubit Pauli-X gate. */
		static QuantumGate PauliX();
		/** @brief Creates the 1-qubit Pauli-Y gate. */
		static QuantumGate PauliY();
		/** @brief Creates the 1-qubit Pauli-Z gate. */
		static QuantumGate PauliZ();
		/**
		 * @brief Creates a 1-qubit phase-shift gate.
		 * @param theta Phase angle in radians.
		 * @return Phase gate `diag(1, e^(i*theta))`.
		 */
		static QuantumGate PhaseShift(double theta);
		/** @brief Creates the 1-qubit pi/8 (T) gate. */
		static QuantumGate PiOverEight();
		/** @brief Creates the 2-qubit controlled-NOT gate. */
		static QuantumGate ControlledNot();
		/** @brief Creates the 3-qubit Toffoli (CCNOT) gate. */
		static QuantumGate Toffoli();
		/**
		 * @brief Creates a 2-qubit controlled phase-shift gate.
		 * @param theta Phase angle in radians.
		 * @return Controlled phase gate.
		 */
		static QuantumGate ControlledPhaseShift(double theta);
		/** @brief Creates the 2-qubit SWAP gate. */
		static QuantumGate Swap();
		/**
		 * @brief Creates a 2-qubit Ising interaction gate.
		 * @param theta Interaction angle in radians.
		 * @return Ising gate matrix.
		 */
		static QuantumGate Ising(double theta);
};

/**
 * @brief Scalar-left multiplication helper.
 * @param x Scalar value.
 * @param U Gate matrix.
 * @return Product `x * U`.
 */
QuantumGate operator*(Amplitude x, const QuantumGate &U);

} // namespace tmfqs

#endif // TMFQS_GATES_QUANTUM_GATE_H
