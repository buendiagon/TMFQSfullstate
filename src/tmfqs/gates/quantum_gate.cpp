#include "tmfqs/gates/quantum_gate.h"

#include <cstdint>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "tmfqs/core/constants.h"
#include "tmfqs/core/math.h"
#include "tmfqs/core/state_space.h"

namespace tmfqs {

/** @brief Constructs a zero-initialized gate matrix of given dimension. */
QuantumGate::QuantumGate(unsigned int dimension)
	: matrix_(static_cast<size_t>(dimension) * dimension), dimension_(dimension) {
	for(Amplitude &cell : matrix_) {
		cell.real = 0.0;
		cell.imag = 0.0;
	}
}

/** @brief Returns mutable pointer to one matrix row. */
Amplitude *QuantumGate::operator[](unsigned int i) {
	if(i >= dimension_) {
		throw std::out_of_range("QuantumGate row index out of range");
	}
	return matrix_.data() + static_cast<size_t>(i) * dimension_;
}

/** @brief Returns const pointer to one matrix row. */
const Amplitude *QuantumGate::operator[](unsigned int i) const {
	if(i >= dimension_) {
		throw std::out_of_range("QuantumGate row index out of range");
	}
	return matrix_.data() + static_cast<size_t>(i) * dimension_;
}

/** @brief Returns gate matrix dimension. */
unsigned int QuantumGate::dimension() const noexcept {
	return dimension_;
}

/** @brief Multiplies this gate by a complex scalar. */
QuantumGate QuantumGate::operator*(Amplitude x) const {
	QuantumGate result(dimension_);
	for(unsigned int i = 0; i < dimension_; ++i) {
		for(unsigned int j = 0; j < dimension_; ++j) {
			result[i][j] = amplitudeMultiply((*this)[i][j], x);
		}
	}
	return result;
}

/** @brief Left scalar multiplication helper. */
QuantumGate operator*(Amplitude x, const QuantumGate &U) {
	return U * x;
}

/** @brief Multiplies two gate matrices. */
QuantumGate QuantumGate::operator*(const QuantumGate &qg) const {
	if(qg.dimension() != dimension_) {
		throw std::invalid_argument("QuantumGate dimensions differ in multiplication");
	}
	QuantumGate result(dimension_);
	// Classic triple-loop matrix multiplication with complex arithmetic.
	for(unsigned int i = 0; i < dimension_; ++i) {
		Amplitude *resultRow = result[i];
		const Amplitude *leftRow = (*this)[i];
		for(unsigned int k = 0; k < dimension_; ++k) {
			const Amplitude left = leftRow[k];
			const Amplitude *rightRow = qg[k];
			for(unsigned int j = 0; j < dimension_; ++j) {
				resultRow[j] = amplitudeAdd(resultRow[j], amplitudeMultiply(left, rightRow[j]));
			}
		}
	}
	return result;
}

/** @brief Streams gate matrix to an output stream. */
std::ostream &operator<<(std::ostream &os, const QuantumGate &qg) {
	for(unsigned int i = 0; i < qg.dimension(); ++i) {
		for(unsigned int j = 0; j < qg.dimension(); ++j) {
			os << qg[i][j].real << " " << qg[i][j].imag << "\t";
		}
		os << "\n";
	}
	return os;
}

/** @brief Prints gate matrix to standard output. */
void QuantumGate::printQuantumGate() const {
	std::cout << *this;
}

/** @brief Creates identity gate of arbitrary dimension. */
QuantumGate QuantumGate::Identity(unsigned int dimension) {
	QuantumGate g(dimension);
	for(unsigned int i = 0; i < dimension; ++i) {
		g[i][i].real = 1.0;
	}
	return g;
}

/** @brief Creates single-qubit Hadamard gate. */
QuantumGate QuantumGate::Hadamard() {
	QuantumGate g(2);
	const double invSqrt2 = 1.0 / std::sqrt(2.0);
	g[0][0].real = invSqrt2;
	g[0][1].real = invSqrt2;
	g[1][0].real = invSqrt2;
	g[1][1].real = -invSqrt2;
	return g;
}

/** @brief Creates controlled phase-shift gate. */
QuantumGate QuantumGate::ControlledPhaseShift(double theta) {
	QuantumGate g(4);
	const Amplitude phase = complexExp({0.0, theta});
	g[0][0].real = 1.0;
	g[1][1].real = 1.0;
	g[2][2].real = 1.0;
	g[3][3].real = phase.real;
	g[3][3].imag = phase.imag;
	return g;
}

/** @brief Creates controlled-NOT gate. */
QuantumGate QuantumGate::ControlledNot() {
	QuantumGate g(4);
	g[0][0].real = 1.0;
	g[1][1].real = 1.0;
	g[2][3].real = 1.0;
	g[3][2].real = 1.0;
	return g;
}

/** @brief Creates Pauli-X gate. */
QuantumGate QuantumGate::PauliX() {
	QuantumGate g(2);
	g[0][1].real = 1.0;
	g[1][0].real = 1.0;
	return g;
}

/** @brief Creates Pauli-Y gate. */
QuantumGate QuantumGate::PauliY() {
	QuantumGate g(2);
	g[0][1].imag = -1.0;
	g[1][0].imag = 1.0;
	return g;
}

/** @brief Creates Pauli-Z gate. */
QuantumGate QuantumGate::PauliZ() {
	QuantumGate g(2);
	g[0][0].real = 1.0;
	g[1][1].real = -1.0;
	return g;
}

/** @brief Creates single-qubit phase-shift gate. */
QuantumGate QuantumGate::PhaseShift(double theta) {
	QuantumGate g(2);
	const Amplitude phase = complexExp({0.0, theta});
	g[0][0].real = 1.0;
	g[1][1].real = phase.real;
	g[1][1].imag = phase.imag;
	return g;
}

/** @brief Creates pi-over-eight (T) gate. */
QuantumGate QuantumGate::PiOverEight() {
	return PhaseShift(kPi / 4.0);
}

/** @brief Creates Toffoli (CCNOT) gate. */
QuantumGate QuantumGate::Toffoli() {
	QuantumGate g = Identity(8);
	g[6][6].real = 0.0;
	g[7][7].real = 0.0;
	g[6][7].real = 1.0;
	g[7][6].real = 1.0;
	return g;
}

/** @brief Creates SWAP gate. */
QuantumGate QuantumGate::Swap() {
	QuantumGate g(4);
	g[0][0].real = 1.0;
	g[1][2].real = 1.0;
	g[2][1].real = 1.0;
	g[3][3].real = 1.0;
	return g;
}

/** @brief Creates two-qubit Ising interaction gate. */
QuantumGate QuantumGate::Ising(double theta) {
	QuantumGate g(4);
	const double c = std::cos(theta / 2.0);
	const double s = std::sin(theta / 2.0);
	g[0][0].real = c;
	g[1][1].real = c;
	g[2][2].real = c;
	g[3][3].real = c;
	g[0][3].imag = -s;
	g[1][2].imag = -s;
	g[2][1].imag = -s;
	g[3][0].imag = -s;
	return g;
}

/** @brief Creates dense Quantum Fourier Transform matrix. */
QuantumGate QuantumGate::QFT(unsigned int numQubits) {
	const unsigned int dimension = checkedStateCount(numQubits);
	QuantumGate g(dimension);
	// QFT matrix entries: exp(2*pi*i*row*col/N) / sqrt(N).
	const double norm = 1.0 / std::sqrt(static_cast<double>(dimension));
	const double twoPiOverDimension = 2.0 * kPi / static_cast<double>(dimension);
	for(unsigned int row = 0; row < dimension; ++row) {
		for(unsigned int col = 0; col < dimension; ++col) {
			const uint64_t indexProduct = static_cast<uint64_t>(row) * static_cast<uint64_t>(col);
			const double phase = twoPiOverDimension * static_cast<double>(indexProduct);
			g[row][col].real = norm * std::cos(phase);
			g[row][col].imag = norm * std::sin(phase);
		}
	}
	return g;
}

/** @brief Creates dense inverse Quantum Fourier Transform matrix. */
QuantumGate QuantumGate::IQFT(unsigned int numQubits) {
	const unsigned int dimension = checkedStateCount(numQubits);
	QuantumGate g(dimension);
	// Inverse QFT uses the conjugate phase sign.
	const double norm = 1.0 / std::sqrt(static_cast<double>(dimension));
	const double twoPiOverDimension = 2.0 * kPi / static_cast<double>(dimension);
	for(unsigned int row = 0; row < dimension; ++row) {
		for(unsigned int col = 0; col < dimension; ++col) {
			const uint64_t indexProduct = static_cast<uint64_t>(row) * static_cast<uint64_t>(col);
			const double phase = -twoPiOverDimension * static_cast<double>(indexProduct);
			g[row][col].real = norm * std::cos(phase);
			g[row][col].imag = norm * std::sin(phase);
		}
	}
	return g;
}

} // namespace tmfqs
