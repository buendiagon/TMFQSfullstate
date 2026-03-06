#include "quantumRegister.h"
#include "stateSpace.h"

#include <iostream>
#include <utility>

// QuantumRegister is a facade: all storage-specific behavior lives in the selected backend.
QuantumRegister::QuantumRegister() = default;

// Create backend and resolve strategy (including Auto mode) from configuration.
void QuantumRegister::initializeBackend(unsigned int qubits) {
	numQubits_ = qubits;
	numStates_ = checkedStateCount(numQubits_);
	backend_ = createBackend(numQubits_, config_);

	if(config_.strategy == StorageStrategyKind::Auto) {
		size_t estimatedBytes = checkedAmplitudeElementCount(numQubits_) * sizeof(double);
		if(estimatedBytes >= config_.autoThresholdBytes && isStrategyAvailable(StorageStrategyKind::Blosc)) {
			resolvedStrategy_ = StorageStrategyKind::Blosc;
		} else {
			resolvedStrategy_ = StorageStrategyKind::Dense;
		}
	} else {
		resolvedStrategy_ = config_.strategy;
	}
}

QuantumRegister::QuantumRegister(unsigned int qubits, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	if(backend_) backend_->initBasis(numQubits_, 0, {1.0, 0.0});
}

QuantumRegister::QuantumRegister(unsigned int qubits, unsigned int initState, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	if(backend_) backend_->initBasis(numQubits_, initState, {1.0, 0.0});
}

QuantumRegister::QuantumRegister(unsigned int qubits, unsigned int initState, Amplitude amp, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	if(backend_) backend_->initBasis(numQubits_, initState, amp);
}

QuantumRegister::QuantumRegister(unsigned int qubits, AmplitudesVector amplitudes, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	if(backend_) backend_->loadAmplitudes(numQubits_, std::move(amplitudes));
}

QuantumRegister::QuantumRegister(const QuantumRegister &other) {
	numQubits_ = other.numQubits_;
	numStates_ = other.numStates_;
	config_ = other.config_;
	resolvedStrategy_ = other.resolvedStrategy_;
	if(other.backend_) {
		backend_ = other.backend_->clone();
	}
}

QuantumRegister& QuantumRegister::operator=(const QuantumRegister &other) {
	if(this == &other) return *this;
	numQubits_ = other.numQubits_;
	numStates_ = other.numStates_;
	config_ = other.config_;
	resolvedStrategy_ = other.resolvedStrategy_;
	backend_ = other.backend_ ? other.backend_->clone() : nullptr;
	return *this;
}

unsigned int QuantumRegister::qubitCount() const {
	return numQubits_;
}

unsigned int QuantumRegister::stateCount() const {
	return numStates_;
}

int QuantumRegister::getSize() const {
	return (int)(numStates_ * 2u);
}

Amplitude QuantumRegister::amplitude(unsigned int state) const {
	if(!backend_) return {0.0, 0.0};
	return backend_->amplitude(state);
}

double QuantumRegister::probability(unsigned int state) const {
	if(!backend_) return 0.0;
	return backend_->probability(state);
}

double QuantumRegister::probabilitySumatory() const {
	if(!backend_) return 0.0;
	return backend_->probabilitySumatory();
}

void QuantumRegister::setAmplitude(unsigned int state, Amplitude amp) {
	if(!backend_) return;
	backend_->setAmplitude(state, amp);
}

void QuantumRegister::loadAmplitudes(AmplitudesVector amplitudes) {
	if(!backend_) return;
	backend_->loadAmplitudes(numQubits_, std::move(amplitudes));
}

// Initialize equal amplitudes on the provided basis states (sparse input).
void QuantumRegister::initUniformSuperposition(const StatesVector &basisStates) {
	if(!backend_) return;
	backend_->initUniformSuperposition(numQubits_, basisStates);
}

StorageStrategyKind QuantumRegister::storageStrategy() const {
	return resolvedStrategy_;
}

std::ostream &operator << (std::ostream &os, const QuantumRegister &reg) {
	for(unsigned int i = 0; i < reg.numStates_; ++i){
		Amplitude amp = reg.amplitude(i);
		if(amp.real != 0.0 || amp.imag != 0.0) {
			os << i << ": " << amp.real << " + " << amp.imag << "i" << std::endl;
		}
	}
	return os;
}

void QuantumRegister::printStatesVector() const {
	std::cout << *this;
}

QuantumRegister::~QuantumRegister() = default;

// Apply an arbitrary k-qubit gate on the selected qubit indices.
void QuantumRegister::applyGate(const QuantumGate &gate, const IntegerVector &qubits){
	if(!backend_) return;
	backend_->applyGate(gate, qubits, numQubits_);
}

void QuantumRegister::Hadamard(unsigned int qubit){
	IntegerVector v;
	v.push_back(qubit);
	applyGate(QuantumGate::Hadamard(), v);
}

void QuantumRegister::ControlledPhaseShift(unsigned int controlQubit, unsigned int targetQubit, double theta){
	IntegerVector v;
	v.push_back(controlQubit);
	v.push_back(targetQubit);
	applyGate(QuantumGate::ControlledPhaseShift(theta), v);
}

void QuantumRegister::ControlledNot(unsigned int controlQubit, unsigned int targetQubit) {
	IntegerVector v;
	v.push_back(controlQubit);
	v.push_back(targetQubit);
	applyGate(QuantumGate::ControlledNot(), v);
}

// SWAP decomposition in terms of three CNOT operations.
void QuantumRegister::Swap(unsigned int qubit1, unsigned int qubit2) {
	ControlledNot(qubit1, qubit2);
	ControlledNot(qubit2, qubit1);
	ControlledNot(qubit1, qubit2);
}
