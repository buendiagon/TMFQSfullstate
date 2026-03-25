#include "tmfqs/algorithms/grover.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "tmfqs/algorithms/operations.h"
#include "tmfqs/core/constants.h"
#include "tmfqs/core/state_space.h"
#include "tmfqs/register/quantum_register.h"

namespace tmfqs {
namespace algorithms {
namespace {

class ScopedOperationBatch {
	public:
		explicit ScopedOperationBatch(QuantumRegister &quantumRegister) : quantumRegister_(quantumRegister) {
			quantumRegister_.beginOperationBatch();
		}

		ScopedOperationBatch(const ScopedOperationBatch &) = delete;
		ScopedOperationBatch &operator=(const ScopedOperationBatch &) = delete;

		~ScopedOperationBatch() {
			if(!open_) {
				return;
			}
			try {
				quantumRegister_.endOperationBatch();
			} catch(...) {}
		}

		void close() {
			if(!open_) {
				return;
			}
			quantumRegister_.endOperationBatch();
			open_ = false;
		}

	private:
		QuantumRegister &quantumRegister_;
		bool open_ = true;
};

std::vector<StateIndex> resolveMarkedStates(const GroverConfig &config, unsigned int stateCount) {
	std::vector<StateIndex> resolved;
	if(config.markedStates.empty()) {
		resolved.push_back(config.markedState);
	} else {
		resolved = config.markedStates.values();
	}

	if(resolved.empty()) {
		throw std::invalid_argument("groverSearch: requires at least one marked state");
	}
	for(StateIndex markedState : resolved) {
		if(markedState >= stateCount) {
			throw std::invalid_argument("groverSearch: marked state index is out of range for numQubits");
		}
	}
	std::sort(resolved.begin(), resolved.end());
	resolved.erase(std::unique(resolved.begin(), resolved.end()), resolved.end());
	return resolved;
}

unsigned int computeGroverIterations(unsigned int stateCount, size_t markedCount) {
	if(markedCount == 0u) {
		throw std::invalid_argument("groverSearch: requires at least one marked state");
	}
	// Integer floor keeps iteration count conservative to avoid over-rotation.
	const double idealIterations =
		(kPi / 4.0) *
		std::sqrt(static_cast<double>(stateCount) / static_cast<double>(markedCount));
	return static_cast<unsigned int>(std::floor(idealIterations));
}

} // namespace

/**
 * @brief Runs Grover's search procedure for a marked basis state.
 *
 * This routine prepares a uniform superposition, applies the oracle/diffusion
 * pair for approximately the optimal number of iterations, and measures.
 */
StateIndex groverSearch(const GroverConfig &config, IRandomSource &randomSource) {
	const unsigned int stateCount = checkedStateCount(config.numQubits);
	const std::vector<StateIndex> markedStates = resolveMarkedStates(config, stateCount);

	RegisterConfig registerConfig = config.registerConfig;
	if(registerConfig.workloadHint == StorageWorkloadHint::Generic) {
		registerConfig.workloadHint = StorageWorkloadHint::Grover;
	}
	QuantumRegister quantumRegister(config.numQubits, registerConfig);
	ScopedOperationBatch batch(quantumRegister);
	for(unsigned int q = 0; q < config.numQubits; ++q) {
		quantumRegister.applyHadamard(q);
	}

	const unsigned int iterations = computeGroverIterations(stateCount, markedStates.size());
	Amplitude amplitudeSum{std::sqrt(static_cast<double>(stateCount)), 0.0};
	for(unsigned int iteration = 0; iteration < iterations; ++iteration) {
		Amplitude markedAmplitudeSum{0.0, 0.0};
		for(StateIndex markedState : markedStates) {
			const Amplitude amp = quantumRegister.amplitude(markedState);
			markedAmplitudeSum.real += amp.real;
			markedAmplitudeSum.imag += amp.imag;
			quantumRegister.applyPhaseFlipBasisState(markedState);
		}
		amplitudeSum.real -= 2.0 * markedAmplitudeSum.real;
		amplitudeSum.imag -= 2.0 * markedAmplitudeSum.imag;
		quantumRegister.applyInversionAboutMean({
			amplitudeSum.real / static_cast<double>(stateCount),
			amplitudeSum.imag / static_cast<double>(stateCount)
		});
	}
	batch.close();

	if(config.verbose) {
		quantumRegister.printStatesVector();
	}
	return quantumRegister.measure(randomSource);
}

} // namespace algorithms
} // namespace tmfqs
