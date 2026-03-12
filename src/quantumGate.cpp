#include "quantumGate.h"

// Constructors
QuantumGate::QuantumGate(){
	this->dimension = 0;
}
QuantumGate::QuantumGate(unsigned int dimension){
	this->dimension = dimension;
	unsigned int i, j;
	matrix = new Amplitude *[dimension];
	for (unsigned int i = 0; i < dimension; i++)
		matrix[i] = new Amplitude[dimension];
	for (i = 0; i < dimension; i++) {
		for (j = 0; j < dimension; j++) {
			matrix[i][j].real = 0.0;
			matrix[i][j].imag = 0.0;
		}
	}
}

// Operator []
Amplitude *QuantumGate::operator[](unsigned int i) {
	return matrix[i];
}


// Operator * for QuantumGate and Scalar multiplication 
QuantumGate QuantumGate::operator*(Amplitude x) {
	QuantumGate result(dimension);
	unsigned int i, j;
	for (i = 0; i < dimension; i++) {
		for (j = 0; j < dimension; j++) {
			//result[i][j].real = matrix[i][j].real * x.real - matrix[i][j].imag * x.imag;
			//result[i][j].imag = matrix[i][j].real * x.imag + matrix[i][j].imag * x.real;
			result[i][j] = amplitudeMult(result[i][j], x);
		}
	}
	return result;
}

// Left scalar multiplication, nonmember function
QuantumGate operator*(Amplitude x, QuantumGate &U) {
	return U * x;
}


// Operator * for two Quantum gates multiplication 
QuantumGate QuantumGate::operator*(QuantumGate &qg){
	QuantumGate result(dimension);
	unsigned int i, j;
   if (qg.dimension != dimension) {
      printf("Matrices cannot be multiplied; different dimensions\n");
      return result;
   }  
	for (i = 0; i < dimension; i++) {
		for (j = 0; j < dimension; j++) {
         for (unsigned int k = 0; k < dimension; k++) {
            //result[i][j] += matrix[i][k] * qg[k][j];
            result[i][j] = amplitudeAdd(result[i][j], amplitudeMult(matrix[i][k], qg[k][j]));
         }
      }
   }
   return result;
}


// Override operator to print a Quantum Gate
std::ostream &operator << (std::ostream &os, QuantumGate &qg) {
	unsigned int i, j;
	for (i = 0; i < qg.dimension; i++) {
		for (j = 0; j < qg.dimension; j++) {
			cout << qg[i][j].real << " " << qg[i][j].imag << "\t";
		}
		cout << "\n";
	}
	return os;
}


// Print Quantum Gate
void QuantumGate::printQuantumGate() {
   std::cout << *this;
}


// Specific Quantum Gates
QuantumGate QuantumGate::Identity(unsigned int dimension){
	QuantumGate g(dimension);
	unsigned int i;
	for (i = 0; i < dimension; i++){
		g[i][i].real = 1.0;
		g[i][i].imag = 0.0;
	}
	return g;
}



QuantumGate QuantumGate::Hadamard(){
	QuantumGate g(2);
	g[0][0].real = 1/sqrt(2); 
	g[0][0].imag = 0.0;
	g[0][1].real = 1/sqrt(2); 
	g[0][1].imag = 0.0;
	g[1][0].real = 1/sqrt(2); 
	g[1][0].imag = 0.0;
	g[1][1].real = -1/sqrt(2); 
	g[1][1].imag = 0.0;
	return g;
}


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

