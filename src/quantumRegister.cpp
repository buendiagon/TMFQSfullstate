#include "quantumRegister.h"
#include "stateSpace.h"
#include "utils.h"
#include "validation.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr double kDefaultPrintEpsilon = 1e-12;
}

// QuantumRegister is a facade: all storage-specific behavior lives in the selected backend.
QuantumRegister::QuantumRegister() {
	initializeBackend(0);
	backend_->initBasis(numQubits_, 0, {1.0, 0.0});
}

// Create backend and resolve strategy (including Auto mode) from configuration.
void QuantumRegister::initializeBackend(unsigned int qubits) {
	numQubits_ = qubits;
	numStates_ = checkedStateCount(numQubits_);
	BackendSelection selection = createBackendSelection(numQubits_, config_);
	resolvedStrategy_ = selection.strategy;
	backend_ = std::move(selection.backend);
}

QuantumRegister::QuantumRegister(unsigned int qubits, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	backend_->initBasis(numQubits_, 0, {1.0, 0.0});
}

QuantumRegister::QuantumRegister(unsigned int qubits, unsigned int initState, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	backend_->initBasis(numQubits_, initState, {1.0, 0.0});
}

QuantumRegister::QuantumRegister(unsigned int qubits, unsigned int initState, Amplitude amp, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	backend_->initBasis(numQubits_, initState, amp);
}

QuantumRegister::QuantumRegister(unsigned int qubits, AmplitudesVector amplitudes, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	backend_->loadAmplitudes(numQubits_, std::move(amplitudes));
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

void QuantumRegister::requireInitialized(const char *operation) const {
	if(!backend_) {
		throw std::logic_error(std::string("QuantumRegister is not initialized for ") + operation);
	}
}

unsigned int QuantumRegister::qubitCount() const {
	return numQubits_;
}

StateIndex QuantumRegister::stateCount() const {
	return numStates_;
}

size_t QuantumRegister::amplitudeElementCount() const {
	return static_cast<size_t>(numStates_) * 2u;
}

void QuantumRegister::validateStateIndex(StateIndex state, const char *operation) const {
	if(state >= numStates_) {
		throw std::out_of_range(std::string("QuantumRegister::") + operation + " state index out of range");
	}
}

void QuantumRegister::validateSingleQubit(QubitIndex qubit, const char *operation) const {
	if(qubit >= numQubits_) {
		throw std::out_of_range(std::string("QuantumRegister::") + operation + " qubit index out of range");
	}
}

void QuantumRegister::validateTwoQubit(QubitIndex q0, QubitIndex q1, const char *operation) const {
	validateSingleQubit(q0, operation);
	validateSingleQubit(q1, operation);
	if(q0 == q1) {
		throw std::invalid_argument(std::string("QuantumRegister::") + operation + " requires distinct qubits");
	}
}

Amplitude QuantumRegister::amplitude(StateIndex state) const {
	requireInitialized("amplitude query");
	validateStateIndex(state, "amplitude");
	return backend_->amplitude(state);
}

double QuantumRegister::probability(StateIndex state) const {
	requireInitialized("probability query");
	validateStateIndex(state, "probability");
	return backend_->probability(state);
}

double QuantumRegister::totalProbability() const {
	requireInitialized("probability summation");
	return backend_->totalProbability();
}

StateIndex QuantumRegister::measure() const {
	requireInitialized("measurement");
	return backend_->sampleMeasurement(getRandomNumber());
}

void QuantumRegister::setAmplitude(StateIndex state, Amplitude amp) {
	requireInitialized("state update");
	validateStateIndex(state, "setAmplitude");
	backend_->setAmplitude(state, amp);
}

void QuantumRegister::loadAmplitudes(AmplitudesVector amplitudes) {
	requireInitialized("amplitude loading");
	backend_->loadAmplitudes(numQubits_, std::move(amplitudes));
}

// Initialize equal amplitudes on the provided basis states (sparse input).
void QuantumRegister::initUniformSuperposition(const BasisStateList &basisStates) {
	requireInitialized("uniform superposition initialization");
	if(basisStates.empty()) {
		throw std::invalid_argument("QuantumRegister::initUniformSuperposition requires at least one basis state");
	}
	for(StateIndex state : basisStates) {
		if(state >= numStates_) {
			throw std::out_of_range("QuantumRegister::initUniformSuperposition basis state index out of range");
		}
	}
	backend_->initUniformSuperposition(numQubits_, basisStates);
}

StorageStrategyKind QuantumRegister::storageStrategy() const {
	return resolvedStrategy_;
}

std::ostream &operator << (std::ostream &os, const QuantumRegister &reg) {
	reg.requireInitialized("state printing");
	reg.backend_->printNonZeroStates(os, kDefaultPrintEpsilon);
	return os;
}

void QuantumRegister::printStatesVector(double epsilon) const {
	requireInitialized("state printing");
	if(epsilon < 0.0) {
		throw std::invalid_argument("QuantumRegister::printStatesVector epsilon must be non-negative");
	}
	backend_->printNonZeroStates(std::cout, epsilon);
}

QuantumRegister::~QuantumRegister() = default;

// Apply an arbitrary k-qubit gate on the selected qubit indices.
void QuantumRegister::applyGate(const QuantumGate &gate, const QubitList &qubits){
	requireInitialized("gate application");
	validateGateTargets("QuantumRegister::applyGate", qubits, numQubits_, gate.dimension());
	backend_->applyGate(gate, qubits);
}

void QuantumRegister::phaseFlipBasisState(StateIndex state) {
	requireInitialized("basis-state phase flip");
	validateStateIndex(state, "phaseFlipBasisState");
	backend_->phaseFlipBasisState(state);
}

void QuantumRegister::inversionAboutMean() {
	requireInitialized("inversion about mean");
	backend_->inversionAboutMean();
}

void QuantumRegister::Hadamard(QubitIndex qubit){
	requireInitialized("Hadamard");
	validateSingleQubit(qubit, "Hadamard");
	backend_->applyHadamard(qubit);
}

void QuantumRegister::PauliX(QubitIndex qubit) {
	requireInitialized("PauliX");
	validateSingleQubit(qubit, "PauliX");
	backend_->applyPauliX(qubit);
}

void QuantumRegister::ControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta){
	requireInitialized("controlled phase shift");
	validateTwoQubit(controlQubit, targetQubit, "ControlledPhaseShift");
	backend_->applyControlledPhaseShift(controlQubit, targetQubit, theta);
}

void QuantumRegister::ControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) {
	requireInitialized("controlled not");
	validateTwoQubit(controlQubit, targetQubit, "ControlledNot");
	backend_->applyControlledNot(controlQubit, targetQubit);
}

// SWAP decomposition in terms of three CNOT operations.
void QuantumRegister::Swap(QubitIndex qubit1, QubitIndex qubit2) {
	requireInitialized("swap");
	validateTwoQubit(qubit1, qubit2, "Swap");
	ControlledNot(qubit1, qubit2);
	ControlledNot(qubit2, qubit1);
	ControlledNot(qubit1, qubit2);
}
