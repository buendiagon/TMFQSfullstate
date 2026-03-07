#include "tmfqs/register/quantum_register.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "tmfqs/core/random.h"
#include "tmfqs/core/state_space.h"
#include "tmfqs/core/validation.h"

namespace tmfqs {
namespace {
constexpr double kDefaultPrintEpsilon = 1e-12;
}

QuantumRegister::QuantumRegister() {
	initializeBackend(0);
	backend_->initBasis(numQubits_, 0, {1.0, 0.0});
}

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

QuantumRegister::~QuantumRegister() = default;

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

Amplitude QuantumRegister::amplitude(StateIndex state) const {
	requireInitialized("amplitude query");
	validateStateIndex("QuantumRegister::amplitude", state, numStates_);
	return backend_->amplitude(state);
}

double QuantumRegister::probability(StateIndex state) const {
	requireInitialized("probability query");
	validateStateIndex("QuantumRegister::probability", state, numStates_);
	return backend_->probability(state);
}

double QuantumRegister::totalProbability() const {
	requireInitialized("probability summation");
	return backend_->totalProbability();
}

StateIndex QuantumRegister::measure(IRandomSource &randomSource) const {
	requireInitialized("measurement");
	return backend_->sampleMeasurement(randomSource.nextUnitDouble());
}

void QuantumRegister::setAmplitude(StateIndex state, Amplitude amp) {
	requireInitialized("state update");
	validateStateIndex("QuantumRegister::setAmplitude", state, numStates_);
	backend_->setAmplitude(state, amp);
}

void QuantumRegister::loadAmplitudes(AmplitudesVector amplitudes) {
	requireInitialized("amplitude loading");
	backend_->loadAmplitudes(numQubits_, std::move(amplitudes));
}

void QuantumRegister::initUniformSuperposition(const BasisStateList &basisStates) {
	requireInitialized("uniform superposition initialization");
	if(basisStates.empty()) {
		throw std::invalid_argument("QuantumRegister::initUniformSuperposition requires at least one basis state");
	}
	for(StateIndex state : basisStates) {
		validateStateIndex("QuantumRegister::initUniformSuperposition", state, numStates_);
	}
	backend_->initUniformSuperposition(numQubits_, basisStates);
}

StorageStrategyKind QuantumRegister::storageStrategy() const {
	return resolvedStrategy_;
}

std::ostream &operator<<(std::ostream &os, const QuantumRegister &reg) {
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

void QuantumRegister::applyGate(const QuantumGate &gate, const QubitList &qubits) {
	requireInitialized("gate application");
	validateGateTargets("QuantumRegister::applyGate", qubits, numQubits_, gate.dimension());
	backend_->applyGate(gate, qubits);
}

void QuantumRegister::applyPhaseFlipBasisState(StateIndex state) {
	requireInitialized("basis-state phase flip");
	validateStateIndex("QuantumRegister::applyPhaseFlipBasisState", state, numStates_);
	backend_->phaseFlipBasisState(state);
}

void QuantumRegister::applyInversionAboutMean() {
	requireInitialized("inversion about mean");
	backend_->inversionAboutMean();
}

void QuantumRegister::applyHadamard(QubitIndex qubit) {
	requireInitialized("Hadamard");
	validateQubitIndex("QuantumRegister::applyHadamard", qubit, numQubits_);
	backend_->applyHadamard(qubit);
}

void QuantumRegister::applyPauliX(QubitIndex qubit) {
	requireInitialized("PauliX");
	validateQubitIndex("QuantumRegister::applyPauliX", qubit, numQubits_);
	backend_->applyPauliX(qubit);
}

void QuantumRegister::applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) {
	requireInitialized("controlled phase shift");
	validateQubitIndex("QuantumRegister::applyControlledPhaseShift", controlQubit, numQubits_);
	validateQubitIndex("QuantumRegister::applyControlledPhaseShift", targetQubit, numQubits_);
	validateDistinctQubits("QuantumRegister::applyControlledPhaseShift", controlQubit, targetQubit);
	backend_->applyControlledPhaseShift(controlQubit, targetQubit, theta);
}

void QuantumRegister::applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) {
	requireInitialized("controlled not");
	validateQubitIndex("QuantumRegister::applyControlledNot", controlQubit, numQubits_);
	validateQubitIndex("QuantumRegister::applyControlledNot", targetQubit, numQubits_);
	validateDistinctQubits("QuantumRegister::applyControlledNot", controlQubit, targetQubit);
	backend_->applyControlledNot(controlQubit, targetQubit);
}

void QuantumRegister::applySwap(QubitIndex qubit1, QubitIndex qubit2) {
	requireInitialized("swap");
	validateQubitIndex("QuantumRegister::applySwap", qubit1, numQubits_);
	validateQubitIndex("QuantumRegister::applySwap", qubit2, numQubits_);
	validateDistinctQubits("QuantumRegister::applySwap", qubit1, qubit2);
	applyControlledNot(qubit1, qubit2);
	applyControlledNot(qubit2, qubit1);
	applyControlledNot(qubit1, qubit2);
}

} // namespace tmfqs
