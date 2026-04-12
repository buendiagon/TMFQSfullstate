#ifndef TMFQS_EXPERIMENT_COMPARISON_H
#define TMFQS_EXPERIMENT_COMPARISON_H

#include <cstddef>
#include <string>

#include "tmfqs/core/types.h"
#include "tmfqs/state/quantum_state.h"

namespace tmfqs {
namespace experiment {

enum class BackendAccuracyKind {
	Reference,
	Lossless,
	Lossy
};

struct StateComparison {
	bool bitwiseEqual = false;
	double maxAbsAmplitudeError = 0.0;
	double maxAbsComponentError = 0.0;
	double rmseAmplitude = 0.0;
	double relL2 = 0.0;
	double maxAbsProbabilityError = 0.0;
	double totalProbabilityDiff = 0.0;
	StateIndex worstState = 0u;
};

StateComparison compareStates(const state::QuantumState &reference, const state::QuantumState &candidate);
std::string toCsvRow(const std::string &circuit, const std::string &scenario, unsigned int qubits, const StateComparison &comparison);
std::string toJson(const StateComparison &comparison);

} // namespace experiment
} // namespace tmfqs

#endif // TMFQS_EXPERIMENT_COMPARISON_H
