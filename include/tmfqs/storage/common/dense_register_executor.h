#ifndef TMFQS_STORAGE_COMMON_DENSE_REGISTER_EXECUTOR_H
#define TMFQS_STORAGE_COMMON_DENSE_REGISTER_EXECUTOR_H

#include <cmath>
#include <cstddef>
#include <iosfwd>
#include <stdexcept>
#include <utility>

#include "tmfqs/core/state_space.h"
#include "tmfqs/storage/common/backend_validation.h"
#include "tmfqs/storage/common/gate_apply_engine.h"
#include "tmfqs/storage/common/pair_kernel_executor.h"

namespace tmfqs {
namespace storage {

inline Amplitude readAmplitude(const AmplitudesVector &amplitudes, StateIndex state) {
	const size_t offset = static_cast<size_t>(state) * 2u;
	return {amplitudes[offset], amplitudes[offset + 1u]};
}

inline void writeAmplitude(AmplitudesVector &amplitudes, StateIndex state, Amplitude amp) {
	const size_t offset = static_cast<size_t>(state) * 2u;
	amplitudes[offset] = amp.real;
	amplitudes[offset + 1u] = amp.imag;
}

inline double totalProbability(const AmplitudesVector &amplitudes) {
	double sum = 0.0;
	for(size_t idx = 0; idx < amplitudes.size(); idx += 2u) {
		sum += amplitudes[idx] * amplitudes[idx] + amplitudes[idx + 1u] * amplitudes[idx + 1u];
	}
	return sum;
}

inline StateIndex sampleMeasurement(const AmplitudesVector &amplitudes, double rnd) {
	if(rnd < 0.0 || rnd >= 1.0) {
		throw std::invalid_argument("DenseRegisterExecutor::sampleMeasurement requires rnd in [0,1)");
	}
	double cumulative = 0.0;
	const unsigned int stateCount = static_cast<unsigned int>(amplitudes.size() / 2u);
	for(StateIndex state = 0; state < stateCount; ++state) {
		const size_t offset = static_cast<size_t>(state) * 2u;
		cumulative += amplitudes[offset] * amplitudes[offset] + amplitudes[offset + 1u] * amplitudes[offset + 1u];
		if(rnd <= cumulative) {
			return state;
		}
	}
	throw std::runtime_error("DenseRegisterExecutor::sampleMeasurement cumulative probability did not reach sample");
}

inline void printNonZeroStates(std::ostream &os, const AmplitudesVector &amplitudes, double epsilon) {
	if(epsilon < 0.0) {
		throw std::invalid_argument("DenseRegisterExecutor::printNonZeroStates epsilon must be non-negative");
	}
	const unsigned int stateCount = static_cast<unsigned int>(amplitudes.size() / 2u);
	for(StateIndex state = 0; state < stateCount; ++state) {
		const size_t offset = static_cast<size_t>(state) * 2u;
		const double real = amplitudes[offset];
		const double imag = amplitudes[offset + 1u];
		if(std::abs(real) > epsilon || std::abs(imag) > epsilon) {
			os << state << ": " << real << " + " << imag << "i\n";
		}
	}
}

inline void phaseFlipBasisState(AmplitudesVector &amplitudes, StateIndex state) {
	const size_t offset = static_cast<size_t>(state) * 2u;
	amplitudes[offset] = -amplitudes[offset];
	amplitudes[offset + 1u] = -amplitudes[offset + 1u];
}

inline Amplitude meanAmplitude(const AmplitudesVector &amplitudes) {
	double sumReal = 0.0;
	double sumImag = 0.0;
	for(size_t idx = 0; idx < amplitudes.size(); idx += 2u) {
		sumReal += amplitudes[idx];
		sumImag += amplitudes[idx + 1u];
	}
	const double stateCount = static_cast<double>(amplitudes.size() / 2u);
	return {sumReal / stateCount, sumImag / stateCount};
}

inline void inversionAboutMean(AmplitudesVector &amplitudes, Amplitude mean) {
	for(size_t idx = 0; idx < amplitudes.size(); idx += 2u) {
		const double real = amplitudes[idx];
		const double imag = amplitudes[idx + 1u];
		amplitudes[idx] = 2.0 * mean.real - real;
		amplitudes[idx + 1u] = 2.0 * mean.imag - imag;
	}
}

template <typename PairFn>
inline void runPairKernel(AmplitudesVector &amplitudes, unsigned int numStates, unsigned int targetMask, PairFn pairFn) {
	PairKernelExecutor::runFallback(numStates, targetMask, [&](StateIndex state0, StateIndex state1) {
		double *a0 = amplitudes.data() + static_cast<size_t>(state0) * 2u;
		double *a1 = amplitudes.data() + static_cast<size_t>(state1) * 2u;
		pairFn(state0, state1, a0, a1);
	});
}

inline void applyHadamard(AmplitudesVector &amplitudes, unsigned int numQubits, QubitIndex qubit) {
	validateBackendSingleQubit("DenseRegisterExecutor::applyHadamard", qubit, numQubits);
	const unsigned int targetMask = qubitMaskFromMsbIndex(qubit, numQubits);
	const unsigned int numStates = checkedStateCount(numQubits);
	const double invSqrt2 = 1.0 / std::sqrt(2.0);
	runPairKernel(amplitudes, numStates, targetMask, [&](StateIndex, StateIndex, double *a0, double *a1) {
		const double aReal = a0[0];
		const double aImag = a0[1];
		const double bReal = a1[0];
		const double bImag = a1[1];
		if(aReal == 0.0 && aImag == 0.0 && bReal == 0.0 && bImag == 0.0) {
			return;
		}
		a0[0] = (aReal + bReal) * invSqrt2;
		a0[1] = (aImag + bImag) * invSqrt2;
		a1[0] = (aReal - bReal) * invSqrt2;
		a1[1] = (aImag - bImag) * invSqrt2;
	});
}

inline void applyPauliX(AmplitudesVector &amplitudes, unsigned int numQubits, QubitIndex qubit) {
	validateBackendSingleQubit("DenseRegisterExecutor::applyPauliX", qubit, numQubits);
	const unsigned int targetMask = qubitMaskFromMsbIndex(qubit, numQubits);
	const unsigned int numStates = checkedStateCount(numQubits);
	runPairKernel(amplitudes, numStates, targetMask, [&](StateIndex, StateIndex, double *a0, double *a1) {
		if(a0[0] == a1[0] && a0[1] == a1[1]) {
			return;
		}
		std::swap(a0[0], a1[0]);
		std::swap(a0[1], a1[1]);
	});
}

inline void applyControlledPhaseShift(
	AmplitudesVector &amplitudes,
	unsigned int numQubits,
	QubitIndex controlQubit,
	QubitIndex targetQubit,
	double theta) {
	validateBackendTwoQubits("DenseRegisterExecutor::applyControlledPhaseShift", controlQubit, targetQubit, numQubits);
	const unsigned int controlMask = qubitMaskFromMsbIndex(controlQubit, numQubits);
	const unsigned int targetMask = qubitMaskFromMsbIndex(targetQubit, numQubits);
	const unsigned int numStates = checkedStateCount(numQubits);
	const double phaseReal = std::cos(theta);
	const double phaseImag = std::sin(theta);
	runPairKernel(amplitudes, numStates, targetMask, [&](StateIndex, StateIndex state1, double *, double *a1) {
		if((state1 & controlMask) == 0u) {
			return;
		}
		const double real = a1[0];
		const double imag = a1[1];
		if(real == 0.0 && imag == 0.0) {
			return;
		}
		a1[0] = phaseReal * real - phaseImag * imag;
		a1[1] = phaseReal * imag + phaseImag * real;
	});
}

inline void applyControlledNot(
	AmplitudesVector &amplitudes,
	unsigned int numQubits,
	QubitIndex controlQubit,
	QubitIndex targetQubit) {
	validateBackendTwoQubits("DenseRegisterExecutor::applyControlledNot", controlQubit, targetQubit, numQubits);
	const unsigned int controlMask = qubitMaskFromMsbIndex(controlQubit, numQubits);
	const unsigned int targetMask = qubitMaskFromMsbIndex(targetQubit, numQubits);
	const unsigned int numStates = checkedStateCount(numQubits);
	runPairKernel(amplitudes, numStates, targetMask, [&](StateIndex state0, StateIndex, double *a0, double *a1) {
		if((state0 & controlMask) == 0u) {
			return;
		}
		if(a0[0] == a1[0] && a0[1] == a1[1]) {
			return;
		}
		std::swap(a0[0], a1[0]);
		std::swap(a0[1], a1[1]);
	});
}

inline void applyGate(
	AmplitudesVector &amplitudes,
	unsigned int numQubits,
	const QuantumGate &gate,
	const QubitList &qubits,
	GateBlockWorkspace &workspace) {
	auto loadAmplitude = [&](StateIndex state) { return readAmplitude(amplitudes, state); };
	auto storeAmplitude = [&](StateIndex state, Amplitude amp) { writeAmplitude(amplitudes, state, amp); };
	GateApplyEngine::apply(
		"DenseRegisterExecutor::applyGate",
		gate,
		qubits,
		numQubits,
		loadAmplitude,
		storeAmplitude,
		workspace);
}

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_COMMON_DENSE_REGISTER_EXECUTOR_H
