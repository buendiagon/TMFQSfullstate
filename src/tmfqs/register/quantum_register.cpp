#include "tmfqs/register/quantum_register.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "tmfqs/core/random.h"
#include "tmfqs/core/state_space.h"
#include "tmfqs/core/validation.h"
#include "tmfqs/storage/factory/state_backend_factory.h"
#include "tmfqs/storage/i_state_backend.h"

namespace tmfqs {
namespace {
// Default print cutoff used by stream operator.
constexpr double kDefaultPrintEpsilon = 1e-12;
}

/** @brief Constructs a default register initialized to `|0>`. */
QuantumRegister::QuantumRegister() {
	initializeBackend(0);
	backend_->initBasis(numQubits_, 0, {1.0, 0.0});
}

/** @brief Initializes backend selection and size metadata for a qubit count. */
void QuantumRegister::initializeBackend(unsigned int qubits) {
	numQubits_ = qubits;
	numStates_ = checkedStateCount(numQubits_);
	// Selection captures both resolved strategy and matching backend instance.
	BackendSelection selection = StateBackendFactory::createSelection(numQubits_, config_);
	resolvedStrategy_ = selection.strategy;
	backend_ = std::move(selection.backend);
}

/** @brief Constructs a register initialized to the zero basis state. */
QuantumRegister::QuantumRegister(unsigned int qubits, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	backend_->initBasis(numQubits_, 0, {1.0, 0.0});
}

/** @brief Constructs a register initialized to one selected basis state. */
QuantumRegister::QuantumRegister(unsigned int qubits, unsigned int initState, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	backend_->initBasis(numQubits_, initState, {1.0, 0.0});
}

/** @brief Constructs a register initialized with custom amplitude on one state. */
QuantumRegister::QuantumRegister(unsigned int qubits, unsigned int initState, Amplitude amp, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	backend_->initBasis(numQubits_, initState, amp);
}

/** @brief Constructs a register from a full interleaved amplitude vector. */
QuantumRegister::QuantumRegister(unsigned int qubits, AmplitudesVector amplitudes, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	backend_->loadAmplitudes(numQubits_, std::move(amplitudes));
}

/** @brief Deep copy constructor using backend clone semantics. */
QuantumRegister::QuantumRegister(const QuantumRegister &other) {
	numQubits_ = other.numQubits_;
	numStates_ = other.numStates_;
	config_ = other.config_;
	resolvedStrategy_ = other.resolvedStrategy_;
	if(other.backend_) {
		// Deep-copy backend storage so register copies are independent.
		backend_ = other.backend_->clone();
	}
}

/** @brief Deep copy assignment using backend clone semantics. */
QuantumRegister& QuantumRegister::operator=(const QuantumRegister &other) {
	if(this == &other) return *this;
	numQubits_ = other.numQubits_;
	numStates_ = other.numStates_;
	config_ = other.config_;
	resolvedStrategy_ = other.resolvedStrategy_;
	backend_ = other.backend_ ? other.backend_->clone() : nullptr;
	return *this;
}

/** @brief Default destructor. */
QuantumRegister::~QuantumRegister() = default;

/** @brief Throws when backend storage is unavailable for an operation. */
void QuantumRegister::requireInitialized(const char *operation) const {
	if(!backend_) {
		throw std::logic_error(std::string("QuantumRegister is not initialized for ") + operation);
	}
}

/** @brief Returns number of qubits in the register. */
unsigned int QuantumRegister::qubitCount() const {
	return numQubits_;
}

/** @brief Returns number of basis states represented by the register. */
StateIndex QuantumRegister::stateCount() const {
	return numStates_;
}

/** @brief Returns amplitude buffer length in doubles (`2 * stateCount`). */
size_t QuantumRegister::amplitudeElementCount() const {
	return static_cast<size_t>(numStates_) * 2u;
}

/** @brief Reads complex amplitude for one basis state. */
Amplitude QuantumRegister::amplitude(StateIndex state) const {
	requireInitialized("amplitude query");
	validateStateIndex("QuantumRegister::amplitude", state, numStates_);
	return backend_->amplitude(state);
}

/** @brief Computes probability mass for one basis state. */
double QuantumRegister::probability(StateIndex state) const {
	requireInitialized("probability query");
	validateStateIndex("QuantumRegister::probability", state, numStates_);
	return backend_->probability(state);
}

/** @brief Computes total probability mass over all basis states. */
double QuantumRegister::totalProbability() const {
	requireInitialized("probability summation");
	return backend_->totalProbability();
}

/** @brief Samples one basis state using the provided random source. */
StateIndex QuantumRegister::measure(IRandomSource &randomSource) const {
	requireInitialized("measurement");
	return backend_->sampleMeasurement(randomSource.nextUnitDouble());
}

/** @brief Writes complex amplitude for one basis state. */
void QuantumRegister::setAmplitude(StateIndex state, Amplitude amp) {
	requireInitialized("state update");
	validateStateIndex("QuantumRegister::setAmplitude", state, numStates_);
	backend_->setAmplitude(state, amp);
}

/** @brief Loads complete amplitude data from an interleaved buffer. */
void QuantumRegister::loadAmplitudes(AmplitudesVector amplitudes) {
	requireInitialized("amplitude loading");
	backend_->loadAmplitudes(numQubits_, std::move(amplitudes));
}

/** @brief Initializes equal superposition over provided basis states. */
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

/** @brief Returns resolved storage strategy used by this register. */
StorageStrategyKind QuantumRegister::storageStrategy() const {
	return resolvedStrategy_;
}

/** @brief Stream-print helper for non-zero register amplitudes. */
std::ostream &operator<<(std::ostream &os, const QuantumRegister &reg) {
	reg.requireInitialized("state printing");
	reg.backend_->printNonZeroStates(os, kDefaultPrintEpsilon);
	return os;
}

/** @brief Prints non-negligible amplitudes to standard output. */
void QuantumRegister::printStatesVector(double epsilon) const {
	requireInitialized("state printing");
	if(epsilon < 0.0) {
		throw std::invalid_argument("QuantumRegister::printStatesVector epsilon must be non-negative");
	}
	backend_->printNonZeroStates(std::cout, epsilon);
}

/** @brief Applies an arbitrary gate matrix to selected qubits. */
void QuantumRegister::applyGate(const QuantumGate &gate, const QubitList &qubits) {
	requireInitialized("gate application");
	validateGateTargets("QuantumRegister::applyGate", qubits, numQubits_, gate.dimension());
	backend_->applyGate(gate, qubits);
}

/** @brief Applies a phase flip to one basis state. */
void QuantumRegister::applyPhaseFlipBasisState(StateIndex state) {
	requireInitialized("basis-state phase flip");
	validateStateIndex("QuantumRegister::applyPhaseFlipBasisState", state, numStates_);
	backend_->phaseFlipBasisState(state);
}

/** @brief Applies inversion-about-mean to all amplitudes. */
void QuantumRegister::applyInversionAboutMean() {
	requireInitialized("inversion about mean");
	backend_->inversionAboutMean();
}

/** @brief Applies Hadamard to one qubit. */
void QuantumRegister::applyHadamard(QubitIndex qubit) {
	requireInitialized("Hadamard");
	validateQubitIndex("QuantumRegister::applyHadamard", qubit, numQubits_);
	backend_->applyHadamard(qubit);
}

/** @brief Applies Pauli-X to one qubit. */
void QuantumRegister::applyPauliX(QubitIndex qubit) {
	requireInitialized("PauliX");
	validateQubitIndex("QuantumRegister::applyPauliX", qubit, numQubits_);
	backend_->applyPauliX(qubit);
}

/** @brief Applies controlled phase-shift gate. */
void QuantumRegister::applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) {
	requireInitialized("controlled phase shift");
	validateQubitIndex("QuantumRegister::applyControlledPhaseShift", controlQubit, numQubits_);
	validateQubitIndex("QuantumRegister::applyControlledPhaseShift", targetQubit, numQubits_);
	validateDistinctQubits("QuantumRegister::applyControlledPhaseShift", controlQubit, targetQubit);
	backend_->applyControlledPhaseShift(controlQubit, targetQubit, theta);
}

/** @brief Applies controlled-NOT gate. */
void QuantumRegister::applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) {
	requireInitialized("controlled not");
	validateQubitIndex("QuantumRegister::applyControlledNot", controlQubit, numQubits_);
	validateQubitIndex("QuantumRegister::applyControlledNot", targetQubit, numQubits_);
	validateDistinctQubits("QuantumRegister::applyControlledNot", controlQubit, targetQubit);
	backend_->applyControlledNot(controlQubit, targetQubit);
}

/** @brief Applies SWAP using three controlled-NOT operations. */
void QuantumRegister::applySwap(QubitIndex qubit1, QubitIndex qubit2) {
	requireInitialized("swap");
	validateQubitIndex("QuantumRegister::applySwap", qubit1, numQubits_);
	validateQubitIndex("QuantumRegister::applySwap", qubit2, numQubits_);
	validateDistinctQubits("QuantumRegister::applySwap", qubit1, qubit2);
	// SWAP decomposition into three CNOTs.
	applyControlledNot(qubit1, qubit2);
	applyControlledNot(qubit2, qubit1);
	applyControlledNot(qubit1, qubit2);
}

/** @brief Opens a backend mutation batch. */
void QuantumRegister::beginOperationBatch() {
	requireInitialized("operation batch begin");
	backend_->beginOperationBatch();
}

/** @brief Closes a backend mutation batch. */
void QuantumRegister::endOperationBatch() {
	requireInitialized("operation batch end");
	backend_->endOperationBatch();
}

} // namespace tmfqs
