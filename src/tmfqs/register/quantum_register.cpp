#include "tmfqs/register/quantum_register.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "tmfqs/core/random.h"
#include "tmfqs/core/state_space.h"
#include "tmfqs/core/validation.h"
#include "tmfqs/storage/factory/state_backend_factory.h"
#include "tmfqs/storage/i_state_backend.h"

namespace tmfqs {
namespace {
// Default print cutoff used by stream operator.
constexpr double kDefaultPrintEpsilon = 1e-12;

Amplitude addAmplitude(Amplitude lhs, Amplitude rhs) {
	return {lhs.real + rhs.real, lhs.imag + rhs.imag};
}

Amplitude subAmplitude(Amplitude lhs, Amplitude rhs) {
	return {lhs.real - rhs.real, lhs.imag - rhs.imag};
}

Amplitude negateAmplitude(Amplitude amplitude) {
	return {-amplitude.real, -amplitude.imag};
}

Amplitude scaleAmplitude(Amplitude amplitude, double scalar) {
	return {amplitude.real * scalar, amplitude.imag * scalar};
}

Amplitude multiplyAmplitude(Amplitude lhs, Amplitude rhs) {
	return {
		lhs.real * rhs.real - lhs.imag * rhs.imag,
		lhs.real * rhs.imag + lhs.imag * rhs.real
	};
}

Amplitude divideAmplitude(Amplitude numerator, Amplitude denominator) {
	const double denomNormSq = denominator.real * denominator.real + denominator.imag * denominator.imag;
	if(denomNormSq == 0.0) {
		throw std::logic_error("QuantumRegister affine overlay scale is zero");
	}
	return {
		(numerator.real * denominator.real + numerator.imag * denominator.imag) / denomNormSq,
		(numerator.imag * denominator.real - numerator.real * denominator.imag) / denomNormSq
	};
}

double probabilityOf(Amplitude amplitude) {
	return amplitude.real * amplitude.real + amplitude.imag * amplitude.imag;
}
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
	config_ = selection.config;
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
QuantumRegister::QuantumRegister(unsigned int qubits, StateIndex initState, const RegisterConfig &cfg) {
	config_ = cfg;
	initializeBackend(qubits);
	backend_->initBasis(numQubits_, initState, {1.0, 0.0});
}

/** @brief Constructs a register initialized with custom amplitude on one state. */
QuantumRegister::QuantumRegister(unsigned int qubits, StateIndex initState, Amplitude amp, const RegisterConfig &cfg) {
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
		if(!other.affineOverlayActive_) {
			// Deep-copy backend storage so register copies are independent.
			backend_ = other.backend_->clone();
		} else {
			BackendSelection selection = StateBackendFactory::createSelection(numQubits_, config_);
			config_ = selection.config;
			resolvedStrategy_ = selection.strategy;
			backend_ = std::move(selection.backend);
			backend_->loadAmplitudes(numQubits_, other.snapshotLogicalAmplitudes());
		}
	}
	resetAffineOverlay();
}

/** @brief Deep copy assignment using backend clone semantics. */
QuantumRegister& QuantumRegister::operator=(const QuantumRegister &other) {
	if(this == &other) return *this;
	numQubits_ = other.numQubits_;
	numStates_ = other.numStates_;
	config_ = other.config_;
	resolvedStrategy_ = other.resolvedStrategy_;
	if(!other.backend_) {
		backend_.reset();
		resetAffineOverlay();
		return *this;
	}
	if(!other.affineOverlayActive_) {
		backend_ = other.backend_->clone();
		resetAffineOverlay();
		return *this;
	}
	BackendSelection selection = StateBackendFactory::createSelection(numQubits_, config_);
	config_ = selection.config;
	resolvedStrategy_ = selection.strategy;
	backend_ = std::move(selection.backend);
	backend_->loadAmplitudes(numQubits_, other.snapshotLogicalAmplitudes());
	resetAffineOverlay();
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

/** @brief Resets lazy affine overlay state to identity. */
void QuantumRegister::resetAffineOverlay() {
	affineScale_ = {1.0, 0.0};
	affineBias_ = {0.0, 0.0};
	affineOverlayActive_ = false;
}

/** @brief Applies the lazy affine overlay to a backend amplitude. */
Amplitude QuantumRegister::applyAffineOverlay(Amplitude backendAmplitude) const {
	if(!affineOverlayActive_) {
		return backendAmplitude;
	}
	return addAmplitude(multiplyAmplitude(affineScale_, backendAmplitude), affineBias_);
}

/** @brief Converts a logical amplitude through the inverse affine overlay. */
Amplitude QuantumRegister::removeAffineOverlay(Amplitude logicalAmplitude) const {
	if(!affineOverlayActive_) {
		return logicalAmplitude;
	}
	return divideAmplitude(subAmplitude(logicalAmplitude, affineBias_), affineScale_);
}

/** @brief Computes the sum of all logical amplitudes. */
Amplitude QuantumRegister::logicalAmplitudeSum() const {
	Amplitude sum{0.0, 0.0};
	visitLogicalAmplitudeChunks([&](StateIndex, const double *data, size_t elemCount) {
		for(size_t elem = 0; elem + 1u < elemCount; elem += 2u) {
			sum.real += data[elem];
			sum.imag += data[elem + 1u];
		}
	});
	return sum;
}

/** @brief Visits logical amplitudes chunk by chunk, applying overlay lazily when needed. */
void QuantumRegister::visitLogicalAmplitudeChunks(
	const std::function<void(StateIndex baseState, const double *data, size_t elemCount)> &visitor) const {
	requireInitialized("amplitude chunk iteration");
	if(!affineOverlayActive_) {
		backend_->forEachAmplitudeChunk(visitor);
		return;
	}

	std::vector<double> logicalChunk;
	backend_->forEachAmplitudeChunk([&](StateIndex baseState, const double *data, size_t elemCount) {
		logicalChunk.resize(elemCount);
		for(size_t elem = 0; elem + 1u < elemCount; elem += 2u) {
			const Amplitude logical = applyAffineOverlay({data[elem], data[elem + 1u]});
			logicalChunk[elem] = logical.real;
			logicalChunk[elem + 1u] = logical.imag;
		}
		visitor(baseState, logicalChunk.data(), elemCount);
	});
}

/** @brief Exports the current logical state, including any active overlay. */
AmplitudesVector QuantumRegister::snapshotLogicalAmplitudes() const {
	AmplitudesVector snapshot(amplitudeElementCount(), 0.0);
	visitLogicalAmplitudeChunks([&](StateIndex baseState, const double *data, size_t elemCount) {
		const size_t beginElem = static_cast<size_t>(baseState) * 2u;
		std::copy(data, data + elemCount, snapshot.begin() + static_cast<std::ptrdiff_t>(beginElem));
	});
	return snapshot;
}

/** @brief Materializes the logical overlay back into backend storage. */
void QuantumRegister::flushAffineOverlay() {
	if(!affineOverlayActive_) {
		return;
	}
	backend_->loadAmplitudes(numQubits_, snapshotLogicalAmplitudes());
	resetAffineOverlay();
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
	return applyAffineOverlay(backend_->amplitude(state));
}

/** @brief Exports the full logical amplitude vector. */
AmplitudesVector QuantumRegister::amplitudes() const {
	requireInitialized("amplitude export");
	return snapshotLogicalAmplitudes();
}

/** @brief Computes probability mass for one basis state. */
double QuantumRegister::probability(StateIndex state) const {
	requireInitialized("probability query");
	validateStateIndex("QuantumRegister::probability", state, numStates_);
	return probabilityOf(amplitude(state));
}

/** @brief Computes total probability mass over all basis states. */
double QuantumRegister::totalProbability() const {
	requireInitialized("probability summation");
	if(!affineOverlayActive_) {
		return backend_->totalProbability();
	}
	double total = 0.0;
	visitLogicalAmplitudeChunks([&](StateIndex, const double *data, size_t elemCount) {
		for(size_t elem = 0; elem + 1u < elemCount; elem += 2u) {
			total += data[elem] * data[elem] + data[elem + 1u] * data[elem + 1u];
		}
	});
	return total;
}

/** @brief Samples one basis state using the provided random source. */
StateIndex QuantumRegister::measure(IRandomSource &randomSource) const {
	requireInitialized("measurement");
	if(!affineOverlayActive_) {
		return backend_->sampleMeasurement(randomSource.nextUnitDouble());
	}
	const double rnd = randomSource.nextUnitDouble();
	double cumulative = 0.0;
	StateIndex measuredState = numStates_;
	visitLogicalAmplitudeChunks([&](StateIndex baseState, const double *data, size_t elemCount) {
		if(measuredState != numStates_) {
			return;
		}
		const size_t statesInChunk = elemCount / 2u;
		for(size_t localState = 0; localState < statesInChunk; ++localState) {
			const size_t elem = localState * 2u;
			cumulative += data[elem] * data[elem] + data[elem + 1u] * data[elem + 1u];
			if(rnd <= cumulative) {
				measuredState = baseState + static_cast<StateIndex>(localState);
				return;
			}
		}
	});
	if(measuredState != numStates_) {
		return measuredState;
	}
	throw std::runtime_error("QuantumRegister::measure cumulative probability did not reach sample");
}

/** @brief Writes complex amplitude for one basis state. */
void QuantumRegister::setAmplitude(StateIndex state, Amplitude amp) {
	requireInitialized("state update");
	validateStateIndex("QuantumRegister::setAmplitude", state, numStates_);
	backend_->setAmplitude(state, removeAffineOverlay(amp));
}

/** @brief Loads complete amplitude data from an interleaved buffer. */
void QuantumRegister::loadAmplitudes(AmplitudesVector amplitudes) {
	requireInitialized("amplitude loading");
	if(amplitudes.size() != amplitudeElementCount()) {
		throw std::invalid_argument("QuantumRegister::loadAmplitudes amplitudes size mismatch");
	}
	resetAffineOverlay();
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
	resetAffineOverlay();
	backend_->initUniformSuperposition(numQubits_, basisStates);
}

/** @brief Returns resolved storage strategy used by this register. */
StorageStrategyKind QuantumRegister::storageStrategy() const {
	return resolvedStrategy_;
}

/** @brief Stream-print helper for non-zero register amplitudes. */
std::ostream &operator<<(std::ostream &os, const QuantumRegister &reg) {
	reg.requireInitialized("state printing");
	if(!reg.affineOverlayActive_) {
		reg.backend_->printNonZeroStates(os, kDefaultPrintEpsilon);
		return os;
	}
	reg.visitLogicalAmplitudeChunks([&](StateIndex baseState, const double *data, size_t elemCount) {
		const size_t statesInChunk = elemCount / 2u;
		for(size_t localState = 0; localState < statesInChunk; ++localState) {
			const double real = data[localState * 2u];
			const double imag = data[localState * 2u + 1u];
			if(std::abs(real) > kDefaultPrintEpsilon || std::abs(imag) > kDefaultPrintEpsilon) {
				os << (baseState + static_cast<StateIndex>(localState)) << ": "
				   << real << " + " << imag << "i\n";
			}
		}
	});
	return os;
}

/** @brief Prints non-negligible amplitudes to standard output. */
void QuantumRegister::printStatesVector(double epsilon) const {
	requireInitialized("state printing");
	if(epsilon < 0.0) {
		throw std::invalid_argument("QuantumRegister::printStatesVector epsilon must be non-negative");
	}
	if(!affineOverlayActive_) {
		backend_->printNonZeroStates(std::cout, epsilon);
		return;
	}
	visitLogicalAmplitudeChunks([&](StateIndex baseState, const double *data, size_t elemCount) {
		const size_t statesInChunk = elemCount / 2u;
		for(size_t localState = 0; localState < statesInChunk; ++localState) {
			const double real = data[localState * 2u];
			const double imag = data[localState * 2u + 1u];
			if(std::abs(real) > epsilon || std::abs(imag) > epsilon) {
				std::cout << (baseState + static_cast<StateIndex>(localState)) << ": "
				          << real << " + " << imag << "i\n";
			}
		}
	});
	return;
}

/** @brief Applies an arbitrary gate matrix to selected qubits. */
void QuantumRegister::applyGate(const QuantumGate &gate, const QubitList &qubits) {
	requireInitialized("gate application");
	validateGateTargets("QuantumRegister::applyGate", qubits, numQubits_, gate.dimension());
	if(affineOverlayActive_) {
		flushAffineOverlay();
	}
	backend_->applyGate(gate, qubits);
}

/** @brief Applies a phase flip to one basis state. */
void QuantumRegister::applyPhaseFlipBasisState(StateIndex state) {
	requireInitialized("basis-state phase flip");
	validateStateIndex("QuantumRegister::applyPhaseFlipBasisState", state, numStates_);
	if(affineOverlayActive_) {
		const Amplitude logical = amplitude(state);
		backend_->setAmplitude(state, removeAffineOverlay(negateAmplitude(logical)));
		return;
	}
	backend_->phaseFlipBasisState(state);
}

/** @brief Applies inversion-about-mean to all amplitudes. */
void QuantumRegister::applyInversionAboutMean() {
	requireInitialized("inversion about mean");
	const Amplitude sum = logicalAmplitudeSum();
	applyInversionAboutMean({
		sum.real / static_cast<double>(numStates_),
		sum.imag / static_cast<double>(numStates_)
	});
}

/** @brief Applies inversion-about-mean using a precomputed mean amplitude. */
void QuantumRegister::applyInversionAboutMean(Amplitude mean) {
	requireInitialized("inversion about mean");
	const Amplitude scale = affineOverlayActive_ ? affineScale_ : Amplitude{1.0, 0.0};
	const Amplitude bias = affineOverlayActive_ ? affineBias_ : Amplitude{0.0, 0.0};
	affineScale_ = negateAmplitude(scale);
	affineBias_ = subAmplitude(scaleAmplitude(mean, 2.0), bias);
	affineOverlayActive_ = true;
}

/** @brief Applies inversion-about-mean through the backend storage path. */
void QuantumRegister::applyInversionAboutMeanMaterialized(Amplitude mean) {
	requireInitialized("inversion about mean");
	if(affineOverlayActive_) {
		flushAffineOverlay();
	}
	backend_->inversionAboutMean(mean);
}

/** @brief Applies Hadamard to one qubit. */
void QuantumRegister::applyHadamard(QubitIndex qubit) {
	requireInitialized("Hadamard");
	validateQubitIndex("QuantumRegister::applyHadamard", qubit, numQubits_);
	if(affineOverlayActive_) {
		flushAffineOverlay();
	}
	backend_->applyHadamard(qubit);
}

/** @brief Applies Pauli-X to one qubit. */
void QuantumRegister::applyPauliX(QubitIndex qubit) {
	requireInitialized("PauliX");
	validateQubitIndex("QuantumRegister::applyPauliX", qubit, numQubits_);
	if(affineOverlayActive_) {
		flushAffineOverlay();
	}
	backend_->applyPauliX(qubit);
}

/** @brief Applies controlled phase-shift gate. */
void QuantumRegister::applyControlledPhaseShift(QubitIndex controlQubit, QubitIndex targetQubit, double theta) {
	requireInitialized("controlled phase shift");
	validateQubitIndex("QuantumRegister::applyControlledPhaseShift", controlQubit, numQubits_);
	validateQubitIndex("QuantumRegister::applyControlledPhaseShift", targetQubit, numQubits_);
	validateDistinctQubits("QuantumRegister::applyControlledPhaseShift", controlQubit, targetQubit);
	if(affineOverlayActive_) {
		flushAffineOverlay();
	}
	backend_->applyControlledPhaseShift(controlQubit, targetQubit, theta);
}

/** @brief Applies controlled-NOT gate. */
void QuantumRegister::applyControlledNot(QubitIndex controlQubit, QubitIndex targetQubit) {
	requireInitialized("controlled not");
	validateQubitIndex("QuantumRegister::applyControlledNot", controlQubit, numQubits_);
	validateQubitIndex("QuantumRegister::applyControlledNot", targetQubit, numQubits_);
	validateDistinctQubits("QuantumRegister::applyControlledNot", controlQubit, targetQubit);
	if(affineOverlayActive_) {
		flushAffineOverlay();
	}
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
