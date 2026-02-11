#include "quantumRegister.h"
#include "utils.h"


#include <cstring>

// Quantum Register class implementation
//
//Constructors ###################################
// Default contructor
QuantumRegister::QuantumRegister() {
}

// Parametrized Constructor 1
QuantumRegister::QuantumRegister(unsigned int numQubits) {
	this->numQubits = numQubits;
	this->numStates = pow(2,this->numQubits);
	this->amplitudes.resize(this->numStates*2, 0.0);
	this->amplitudes[0] = 1.0;
}


// Parametrized Constructor 2
QuantumRegister::QuantumRegister(unsigned int numQubits, unsigned int initState) {
	this->numQubits = numQubits;
	this->numStates = pow(2,this->numQubits);
	this->amplitudes.resize(this->numStates*2, 0.0);
	this->amplitudes[2*initState] = 1.0;
}


// Parametrized Constructor 3
QuantumRegister::QuantumRegister(unsigned int numQubits, unsigned int initState, Amplitude amp) {
	this->numQubits = numQubits;
	this->numStates = pow(2,this->numQubits);
	this->amplitudes.resize(this->numStates*2, 0.0);
	this->amplitudes[2*initState] = amp.real;
	this->amplitudes[2*initState + 1] = amp.imag;
}


// Parametrized Constructor 4
/*
QuantumRegister::QuantumRegister(unsigned int numQubits, bool isRandom) {
	this->numQubits = numQubits;
	this->numStates = pow(2,this->numQubits);
	this->amplitudes.resize(this->numStates*2, 0.0);

	if(isRandom == false) {
		this->amplitudes[0] = 1.0;
	} else {
		// Normalizamos el arreglo de amplitudes
		this->amplitudes = normalizeArray(this->amplitudes);
	}
	// TODO: separar la impresión de las amplitudes por aparte, un método para ello
	cout << endl << "Amplitudes iniciales: " << endl;
	for(unsigned int i=0; i<this->amplitudes.size()/2; i++) {
		cout << "\ta_" << i << " -> \t" ;
		cout << std::left << std::setw(20) << std::setprecision(16) << this->amplitudes[i*2] << " \t" ;
		cout << std::left << std::setw(20) << std::setprecision(16) << this->amplitudes[(i*2)+1] << endl;
	}
}
*/

QuantumRegister::QuantumRegister(unsigned int numQubits, string filePath) {
	this->numQubits = numQubits;
	this->numStates = pow(2,this->numQubits);
	this->amplitudes.resize(this->numStates*2, 0.0);

	unsigned int li=0;

	FILE *in_file;
	
	string dateTime;
	bool isData = true;
	in_file = fopen(filePath.c_str(), "r");

	if(in_file == NULL) {
        printErrors(5);
    } else {
		char buffer[20];
		while(fgets(buffer, sizeof(buffer), in_file)) {
			if(isData == true) {
				if(std::strstr(buffer, "*****") != nullptr) {
					isData = false;
				} else {
					this->amplitudes[li] = stod(buffer);
				}
			} else {
				dateTime = buffer;
			}
			++li;
        }
		fclose(in_file);
	}
/*
	cout << endl << "Amplitudes iniciales: " << endl;
	for(unsigned int i=0; i<this->amplitudes.size()/2; i++) {
			cout << "\ta_" << i << " -> \t" ;
			cout << std::left << std::setw(20) << std::setprecision(16) << this->amplitudes[i*2] << " \t" ;
			cout << std::left << std::setw(20) << std::setprecision(16) << this->amplitudes[(i*2)+1] << endl;
	}
*/
}

//Copy Constructor 
QuantumRegister::QuantumRegister(const QuantumRegister& qreg) {
	this->amplitudes = qreg.amplitudes;
}

int QuantumRegister::getSize(){
	return this->amplitudes.size();
}

//Get methods ####################################
//
//Get the element i-th of linearized amplitudes vector
Amplitude QuantumRegister::amplitude(unsigned int state){
	Amplitude amp;
	if (state <= this->numStates){
		amp.real = this->amplitudes[state*2];
		amp.imag = this->amplitudes[state*2 + 1];
	}
	else{
		amp.real = 0.0;
		amp.imag = 0.0;
	}
	return amp;
}




//Get the Magnitud or Modulus of the element i-th
double QuantumRegister::probability(unsigned int state){
	Amplitude amp = amplitude(state);
	return sqrt(pow(amp.real, 2) + pow(amp.imag, 2));
}

//Get the sum of magnitudes of the amplitudes
double QuantumRegister::probabilitySumatory(){
	unsigned int i;
	double sum = 0.0;
	for(i=0; i<this->numStates; i++){
		sum += pow(this->amplitudes[i*2], 2) + pow(this->amplitudes[i*2 + 1], 2);
	}
	return sum;
}


//Set methods ####################################
void QuantumRegister::setSize(unsigned int numQubits){
	this->numQubits = numQubits;
	this->states.resize(pow(2,numQubits), 0);
	this->amplitudes.resize(pow(2,numQubits)*2, 0.0);
}


// Fill the states vector ramdonly
void QuantumRegister::fillStatesVector(){
	int i;
	if ( this->states.size() < this->numQubits ){
		this->setSize(this->numQubits);
	}
	for (i=0; i < pow(2, this->numQubits); i++){
		this->states[i] = i;
		this->amplitudes[i*2] = i;
		this->amplitudes[i*2 + 1] = i;
	}
}

//Miscelaneous methods ###########################
//Print states vector
std::ostream &operator << (std::ostream &os, QuantumRegister &reg) {
	for(unsigned int i=0; i<reg.numStates; i++){
		if(reg.amplitudes[i*2] != 0.0 || reg.amplitudes[(i*2)+1] !=0.0) {
			cout << i << ": " << reg.amplitudes[i*2] << " + " << reg.amplitudes[i*2 + 1] << "i" << endl;
		/*
			cout << std::right << std::setw(2) << i << ": ";
			cout << std::right << std::setw(7) << std::setprecision(3) << reg.amplitudes[i*2]; 
			cout << std::right << std::setw(7) << std::setprecision(3) << reg.amplitudes[(i*2)+1] << endl;
		*/
		}
	}
	return os;
}

void QuantumRegister::printStatesVector(){
	cout << *this;
}


//Destructor #####################################
QuantumRegister::~QuantumRegister(){
}



//Quantum Gates operations

//Get all states according the number of qubits
StatesVector getAllStates(unsigned int qubits){
	StatesVector v;
	int i;

	for(i=0; i < (1 << qubits); i++){
		v.push_back(i);
	}
	return v;
}


string QuantumRegister::getNthBit(unsigned int state, unsigned int qubit){
	unsigned int pos, bit;
	pos = this->numQubits - qubit - 1;   
	bit = (state >> pos) & 1;
	return to_string(bit);
}

int QuantumRegister::findState(unsigned int state){
	int index;
	auto it = std::find(this->states.begin(), this->states.end(), state); 
	if (it != this->states.end()) {
		index = it - this->states.begin();
	}
	else {
		index = -1;
	}
	return index;
}



double QuantumRegister::getProbability(unsigned int state){
	Amplitude amp;
	amp.real = this->amplitudes[state];
	amp.imag = this->amplitudes[state+1];
	return absoluteValue(amp);
}



// Method to apply a quantum gate to quantum register
void QuantumRegister::applyGate(QuantumGate gate, IntegerVector qubits){

	if (gate.dimension != (unsigned int)(1 << qubits.size())) { // 1 << qubits.size is pow(2, qubits.size())
		printf("Unitary matrix dimension is not correct to be applied to the inputs qubits\n");
		return; 
	}

	unsigned int state, newState, pos, tempBit;
	string s;
	int i, j, l, r;
	StatesVector tempStates;
	AmplitudesVector oldAmplitudes;
	Amplitude auxAmp1, auxAmp2, auxAmp3;
	
	// Copy the current states
	oldAmplitudes = this->amplitudes;

	tempStates = getAllStates(qubits.size());


	// For all state do
	for( state = 0; state < this->numStates; ++state){
		if(oldAmplitudes[state*2] != 0.0 || oldAmplitudes[state*2+1] != 0.0){
			s = "";

			// Get a string containing the n-th bit of the state according the qubits vector
			for (unsigned int qubit : qubits){
				s += getNthBit(state, qubit);
			}
			// Find which number basis element s corresponds to.
			r = binaryToDecimal(s);

			//states[state] -= (1.0 - u[r][r]) * old[state];
			auxAmp1.real = 1.0 - gate[r][r].real;
			auxAmp1.imag =  0.0 - gate[r][r].imag;
			auxAmp2.real = oldAmplitudes[state*2];
			auxAmp2.imag = oldAmplitudes[state*2 + 1];
			auxAmp3 = amplitudeMult(auxAmp1, auxAmp2);
			this->amplitudes[state*2] = this->amplitudes[state*2] - auxAmp3.real;
			this->amplitudes[state*2+1] = this->amplitudes[state*2+1] - auxAmp3.imag;
			j = 0;
			for(int k : tempStates){
				if (j != r) {
					newState = state;
					// COPY ALL BITS FROM k TO newState AT POSITION pos WHICH CORRESPOND TO THE QUBIT WHERE TO APPLY THE GATE
					// qubits.size() IS THE NUMBER OF BITS OF k (BITS TO COPY)
					// For all bits of k
					// l Iterates backward over the bits of k (the most significative bits is on the right)
					// And i iterates forward over qubits vector
					for(l = qubits.size() - 1, i = 0; l >= 0, i < qubits.size(); l--, i++){ 
						// get the i-th bit
						tempBit = (k >> l)&1; 
						pos = this->numQubits - qubits[i] - 1;
						newState = copyBits(newState, tempBit, pos, 1);
					}
					//c = u[j][r] * old[state];
					auxAmp1.real = gate[j][r].real;
					auxAmp1.imag = gate[j][r].imag;
					auxAmp2.real = oldAmplitudes[state*2];
					auxAmp2.imag = oldAmplitudes[state*2 + 1];
					auxAmp3 = amplitudeMult(auxAmp1, auxAmp2);
					this->amplitudes[newState*2] += auxAmp3.real;
					this->amplitudes[newState*2+1] += auxAmp3.imag;
					//std::cout << "state: " << state << " newState: " << newState << std::endl;
				}
				j++;
			}
		}
	}
}



// Method to apply a Hadamard gate to specific qubit of a quantum register
void QuantumRegister::Hadamard(unsigned int qubit){
	IntegerVector v;
	v.push_back(qubit);
	applyGate(QuantumGate::Hadamard(), v);
}


// Method to apply a ControlledPhaseShift gate to specific qubit of a quantum register
void QuantumRegister::ControlledPhaseShift(unsigned int controlQubit, unsigned int targetQubit, double theta){
	IntegerVector v;
	v.push_back(controlQubit);
	v.push_back(targetQubit);
	applyGate(QuantumGate::ControlledPhaseShift(theta), v);
}


void QuantumRegister::ControlledNot(unsigned int controlQubit, unsigned int targetQubit) {
	IntegerVector v; v.push_back(controlQubit); v.push_back(targetQubit);
	applyGate(QuantumGate::ControlledNot(), v);
}


void QuantumRegister::Swap(unsigned int qubit1, unsigned int qubit2) {
	ControlledNot(qubit1, qubit2);
	ControlledNot(qubit2, qubit1);
	ControlledNot(qubit1, qubit2);
}
