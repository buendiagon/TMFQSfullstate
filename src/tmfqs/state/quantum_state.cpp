#include "tmfqs/state/quantum_state.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

#include "tmfqs/config/register_config.h"
#include "tmfqs/core/constants.h"
#include "tmfqs/core/state_space.h"
#include "tmfqs/register/quantum_register.h"

namespace tmfqs {
namespace state {
namespace {

enum class StateKind {
	Basis,
	BasisAmplitude,
	UniformSubset,
	Amplitudes,
	Register
};

double probabilityMass(const AmplitudesVector &amplitudes) {
	double total = 0.0;
	for(size_t elem = 0; elem + 1u < amplitudes.size(); elem += 2u) {
		total += amplitudes[elem] * amplitudes[elem] + amplitudes[elem + 1u] * amplitudes[elem + 1u];
	}
	return total;
}

void normalize(AmplitudesVector &amplitudes) {
	const double total = probabilityMass(amplitudes);
	if(total <= 0.0) {
		throw std::invalid_argument("Cannot normalize an all-zero amplitude vector");
	}
	const double scale = 1.0 / std::sqrt(total);
	for(double &value : amplitudes) {
		value *= scale;
	}
}

void validateOrNormalize(AmplitudesVector &amplitudes, const StateValidation &validation) {
	if(amplitudes.size() % 2u != 0u) {
		throw std::invalid_argument("Amplitude vector length must be even");
	}
	if(validation.normalization == NormalizationPolicy::AllowUnnormalized) {
		return;
	}
	if(validation.normalization == NormalizationPolicy::Normalize) {
		normalize(amplitudes);
		return;
	}
	const double total = probabilityMass(amplitudes);
	if(std::abs(total - 1.0) > validation.probabilityTolerance) {
		throw std::invalid_argument("Amplitude vector is not normalized");
	}
}

} // namespace

struct QuantumState::Impl {
	unsigned int qubits = 0;
	StateKind kind = StateKind::Basis;
	StateIndex basisState = 0u;
	Amplitude basisAmplitude{1.0, 0.0};
	BasisStateList basisStates;
	AmplitudesVector amplitudes;
	std::unique_ptr<QuantumRegister> reg;

	Impl() = default;

	Impl(const Impl &other)
		: qubits(other.qubits),
		  kind(other.kind),
		  basisState(other.basisState),
		  basisAmplitude(other.basisAmplitude),
		  basisStates(other.basisStates),
		  amplitudes(other.amplitudes) {
		if(other.reg) {
			reg = std::make_unique<QuantumRegister>(*other.reg);
		}
	}

	Impl &operator=(const Impl &other) {
		if(this == &other) return *this;
		qubits = other.qubits;
		kind = other.kind;
		basisState = other.basisState;
		basisAmplitude = other.basisAmplitude;
		basisStates = other.basisStates;
		amplitudes = other.amplitudes;
		reg = other.reg ? std::make_unique<QuantumRegister>(*other.reg) : nullptr;
		return *this;
	}

	Impl(Impl &&) noexcept = default;
	Impl &operator=(Impl &&) noexcept = default;
};

QuantumState::QuantumState() : impl_(std::make_unique<Impl>()) {}

QuantumState::QuantumState(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

QuantumState::QuantumState(const QuantumState &other) : impl_(std::make_unique<Impl>(*other.impl_)) {}

QuantumState &QuantumState::operator=(const QuantumState &other) {
	if(this != &other) {
		impl_ = std::make_unique<Impl>(*other.impl_);
	}
	return *this;
}

QuantumState::QuantumState(QuantumState &&) noexcept = default;

QuantumState &QuantumState::operator=(QuantumState &&) noexcept = default;

QuantumState::~QuantumState() = default;

unsigned int QuantumState::qubitCount() const {
	return impl_->qubits;
}

StateIndex QuantumState::stateCount() const {
	return checkedStateCount(impl_->qubits);
}

Amplitude QuantumState::amplitude(StateIndex state) const {
	if(state >= stateCount()) {
		throw std::invalid_argument("Amplitude state index is out of range");
	}
	if(impl_->kind == StateKind::Amplitudes) {
		const size_t elem = static_cast<size_t>(state) * 2u;
		return {impl_->amplitudes[elem], impl_->amplitudes[elem + 1u]};
	}
	if(impl_->kind == StateKind::Basis || impl_->kind == StateKind::BasisAmplitude) {
		return state == impl_->basisState ? impl_->basisAmplitude : Amplitude{0.0, 0.0};
	}
	if(impl_->kind == StateKind::Register) {
		return impl_->reg->amplitude(state);
	}
	RegisterConfig cfg;
	cfg.strategy = StorageStrategyKind::Dense;
	return materialize(cfg).amplitude(state);
}

double QuantumState::totalProbability() const {
	if(impl_->kind == StateKind::Amplitudes) {
		return probabilityMass(impl_->amplitudes);
	}
	if(impl_->kind == StateKind::Basis || impl_->kind == StateKind::BasisAmplitude) {
		return impl_->basisAmplitude.real * impl_->basisAmplitude.real +
		       impl_->basisAmplitude.imag * impl_->basisAmplitude.imag;
	}
	if(impl_->kind == StateKind::Register) {
		return impl_->reg->totalProbability();
	}
	RegisterConfig cfg;
	cfg.strategy = StorageStrategyKind::Dense;
	return materialize(cfg).totalProbability();
}

AmplitudesVector QuantumState::amplitudes() const {
	if(impl_->kind == StateKind::Amplitudes) {
		return impl_->amplitudes;
	}
	if(impl_->kind == StateKind::Register) {
		return impl_->reg->amplitudes();
	}
	RegisterConfig cfg;
	cfg.strategy = StorageStrategyKind::Dense;
	return materialize(cfg).amplitudes();
}

StateIndex QuantumState::measure(IRandomSource &randomSource) const {
	if(impl_->kind == StateKind::Basis || impl_->kind == StateKind::BasisAmplitude) {
		return impl_->basisState;
	}
	if(impl_->kind == StateKind::Amplitudes) {
		const double rnd = randomSource.nextUnitDouble();
		double cumulative = 0.0;
		for(StateIndex state = 0; state < stateCount(); ++state) {
			const size_t elem = static_cast<size_t>(state) * 2u;
			cumulative += impl_->amplitudes[elem] * impl_->amplitudes[elem] +
			              impl_->amplitudes[elem + 1u] * impl_->amplitudes[elem + 1u];
			if(rnd <= cumulative) {
				return state;
			}
		}
		throw std::runtime_error("QuantumState::measure cumulative probability did not reach sample");
	}
	if(impl_->kind == StateKind::Register) {
		return impl_->reg->measure(randomSource);
	}
	RegisterConfig cfg;
	cfg.strategy = StorageStrategyKind::Dense;
	return materialize(cfg).measure(randomSource);
}

void QuantumState::setAmplitude(StateIndex state, Amplitude amplitude, const StateValidation &validation) {
	AmplitudesVector values = amplitudes();
	if(state >= stateCount()) {
		throw std::invalid_argument("Amplitude state index is out of range");
	}
	const size_t elem = static_cast<size_t>(state) * 2u;
	values[elem] = amplitude.real;
	values[elem + 1u] = amplitude.imag;
	loadAmplitudes(std::move(values), validation);
}

void QuantumState::loadAmplitudes(AmplitudesVector amplitudes, const StateValidation &validation) {
	if(amplitudes.size() != checkedAmplitudeElementCount(impl_->qubits)) {
		throw std::invalid_argument("Amplitude vector size does not match state qubit count");
	}
	validateOrNormalize(amplitudes, validation);
	impl_->kind = StateKind::Amplitudes;
	impl_->amplitudes = std::move(amplitudes);
}

QuantumState QuantumState::basis(unsigned int qubits, StateIndex state) {
	if(state >= checkedStateCount(qubits)) {
		throw std::invalid_argument("Basis state is out of range");
	}
	auto impl = std::make_unique<Impl>();
	impl->qubits = qubits;
	impl->kind = StateKind::Basis;
	impl->basisState = state;
	return QuantumState(std::move(impl));
}

QuantumState QuantumState::basisAmplitude(unsigned int qubits, StateIndex state, Amplitude amplitude) {
	if(state >= checkedStateCount(qubits)) {
		throw std::invalid_argument("Basis state is out of range");
	}
	auto impl = std::make_unique<Impl>();
	impl->qubits = qubits;
	impl->kind = StateKind::BasisAmplitude;
	impl->basisState = state;
	impl->basisAmplitude = amplitude;
	return QuantumState(std::move(impl));
}

QuantumState QuantumState::uniformSubset(unsigned int qubits, BasisStateList states) {
	auto impl = std::make_unique<Impl>();
	impl->qubits = qubits;
	impl->kind = StateKind::UniformSubset;
	impl->basisStates = std::move(states);
	return QuantumState(std::move(impl));
}

QuantumState QuantumState::fromAmplitudes(unsigned int qubits, AmplitudesVector amplitudes, const StateValidation &validation) {
	if(amplitudes.size() != checkedAmplitudeElementCount(qubits)) {
		throw std::invalid_argument("Amplitude vector size does not match qubit count");
	}
	validateOrNormalize(amplitudes, validation);
	auto impl = std::make_unique<Impl>();
	impl->qubits = qubits;
	impl->kind = StateKind::Amplitudes;
	impl->amplitudes = std::move(amplitudes);
	return QuantumState(std::move(impl));
}

QuantumState QuantumState::randomPhase(unsigned int qubits, unsigned int seed) {
	const StateIndex states = checkedStateCount(qubits);
	std::mt19937 randomGenerator(seed);
	std::uniform_real_distribution<double> phaseDistribution(0.0, 2.0 * std::acos(-1.0));
	const double norm = 1.0 / std::sqrt(static_cast<double>(states));
	AmplitudesVector amplitudes(static_cast<size_t>(states) * 2u, 0.0);
	for(StateIndex state = 0; state < states; ++state) {
		const double phase = phaseDistribution(randomGenerator);
		const size_t elem = static_cast<size_t>(state) * 2u;
		amplitudes[elem] = norm * std::cos(phase);
		amplitudes[elem + 1u] = norm * std::sin(phase);
	}
	return fromAmplitudes(qubits, std::move(amplitudes));
}

QuantumState QuantumState::sparsePattern(unsigned int qubits) {
	const StateIndex states = checkedStateCount(qubits);
	std::vector<StateIndex> selected;
	for(StateIndex x = 0;; ++x) {
		const StateIndex state = 8u * x + (x % 2u);
		if(state >= states) break;
		selected.push_back(state);
	}
	return uniformSubset(qubits, BasisStateList(std::move(selected)));
}

QuantumRegister QuantumState::materialize(const RegisterConfig &config) const {
	switch(impl_->kind) {
		case StateKind::Basis:
			return QuantumRegister(impl_->qubits, impl_->basisState, config);
		case StateKind::BasisAmplitude:
			return QuantumRegister(impl_->qubits, impl_->basisState, impl_->basisAmplitude, config);
		case StateKind::UniformSubset: {
			QuantumRegister reg(impl_->qubits, config);
			reg.initUniformSuperposition(impl_->basisStates);
			return reg;
		}
		case StateKind::Amplitudes:
			return QuantumRegister(impl_->qubits, impl_->amplitudes, config);
		case StateKind::Register:
			return *impl_->reg;
	}
	return QuantumRegister(impl_->qubits, config);
}

QuantumState QuantumState::fromRegister(QuantumRegister reg) {
	auto impl = std::make_unique<Impl>();
	impl->qubits = reg.qubitCount();
	impl->kind = StateKind::Register;
	impl->reg = std::make_unique<QuantumRegister>(std::move(reg));
	return QuantumState(std::move(impl));
}

} // namespace state
} // namespace tmfqs
