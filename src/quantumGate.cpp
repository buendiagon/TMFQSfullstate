#include "quantumGate.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

#include "utils.h"

// Matrix is stored in a contiguous row-major buffer.
QuantumGate::QuantumGate(unsigned int dimension) : matrix_((size_t)dimension * dimension), dimension(dimension) {
	for(Amplitude &cell : matrix_) {
		cell.real = 0.0;
		cell.imag = 0.0;
	}
}

// Row accessor for mutable operations like gate construction.
Amplitude *QuantumGate::operator[](unsigned int i) {
	if(i >= dimension) {
		throw std::out_of_range("QuantumGate row index out of range");
	}
	return matrix_.data() + (size_t)i * dimension;
}

// Const row accessor for read-only arithmetic paths.
const Amplitude *QuantumGate::operator[](unsigned int i) const {
	if(i >= dimension) {
		throw std::out_of_range("QuantumGate row index out of range");
	}
	return matrix_.data() + (size_t)i * dimension;
}

// Scalar multiplication of all matrix entries.
QuantumGate QuantumGate::operator*(Amplitude x) const {
	QuantumGate result(dimension);
	for(unsigned int i = 0; i < dimension; i++) {
		for(unsigned int j = 0; j < dimension; j++) {
			result[i][j] = amplitudeMult((*this)[i][j], x);
		}
	}
	return result;
}

QuantumGate operator*(Amplitude x, const QuantumGate &U) {
	return U * x;
}

// Standard matrix multiplication (this * qg).
QuantumGate QuantumGate::operator*(const QuantumGate &qg) const {
	QuantumGate result(dimension);
	if(qg.dimension != dimension) {
		throw std::invalid_argument("QuantumGate dimensions differ in multiplication");
	}
	for(unsigned int i = 0; i < dimension; i++) {
		for(unsigned int j = 0; j < dimension; j++) {
			for(unsigned int k = 0; k < dimension; k++) {
				result[i][j] = amplitudeAdd(result[i][j], amplitudeMult((*this)[i][k], qg[k][j]));
			}
		}
	}
	return result;
}

std::ostream &operator<<(std::ostream &os, const QuantumGate &qg) {
	for(unsigned int i = 0; i < qg.dimension; i++) {
		for(unsigned int j = 0; j < qg.dimension; j++) {
			os << qg[i][j].real << " " << qg[i][j].imag << "\t";
		}
		os << "\n";
	}
	return os;
}

void QuantumGate::printQuantumGate() const {
	std::cout << *this;
}

QuantumGate QuantumGate::Identity(unsigned int dimension){
	QuantumGate g(dimension);
	for(unsigned int i = 0; i < dimension; i++){
		g[i][i].real = 1.0;
		g[i][i].imag = 0.0;
	}
	return g;
}

QuantumGate QuantumGate::Hadamard(){
	QuantumGate g(2);
	g[0][0].real = 1 / std::sqrt(2.0);
	g[0][0].imag = 0.0;
	g[0][1].real = 1 / std::sqrt(2.0);
	g[0][1].imag = 0.0;
	g[1][0].real = 1 / std::sqrt(2.0);
	g[1][0].imag = 0.0;
	g[1][1].real = -1 / std::sqrt(2.0);
	g[1][1].imag = 0.0;
	return g;
}

// Controlled phase matrix diag(1,1,1,e^{i theta}).
QuantumGate QuantumGate::ControlledPhaseShift(double theta){
	QuantumGate g(4);
	Amplitude amp, ampResult;
	amp.real = 0;
	amp.imag = theta;
	ampResult = eRaisedToComplex(amp);
	g[0][0].real = 1.0;
	g[1][1].real = 1.0;
	g[2][2].real = 1.0;
	g[3][3].real = ampResult.real;
	g[3][3].imag = ampResult.imag;
	return g;
}

QuantumGate QuantumGate::ControlledNot() {
	QuantumGate g(4);
	g[0][0].real = 1.0;
	g[1][1].real = 1.0;
	g[2][3].real = 1.0;
	g[3][2].real = 1.0;
	return g;
}
