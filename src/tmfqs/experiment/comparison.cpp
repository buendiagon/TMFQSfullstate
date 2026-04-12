#include "tmfqs/experiment/comparison.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace tmfqs {
namespace experiment {

StateComparison compareStates(const state::QuantumState &reference, const state::QuantumState &candidate) {
	const AmplitudesVector ref = reference.amplitudes();
	const AmplitudesVector got = candidate.amplitudes();
	if(ref.size() != got.size()) {
		throw std::invalid_argument("Compared states must have the same amplitude vector size");
	}
	if(ref.size() % 2u != 0u) {
		throw std::invalid_argument("Amplitude vector length must be even");
	}

	StateComparison metrics;
	metrics.bitwiseEqual = ref.size() == got.size() &&
		std::memcmp(ref.data(), got.data(), ref.size() * sizeof(double)) == 0;

	double diffNormSq = 0.0;
	double refNormSq = 0.0;
	double refTotalProbability = 0.0;
	double gotTotalProbability = 0.0;
	const size_t stateCount = ref.size() / 2u;

	for(size_t state = 0; state < stateCount; ++state) {
		const size_t elem = state * 2u;
		const double refReal = ref[elem];
		const double refImag = ref[elem + 1u];
		const double gotReal = got[elem];
		const double gotImag = got[elem + 1u];
		const double diffReal = gotReal - refReal;
		const double diffImag = gotImag - refImag;
		const double stateDiffSq = diffReal * diffReal + diffImag * diffImag;
		const double stateAbsError = std::sqrt(stateDiffSq);
		if(stateAbsError > metrics.maxAbsAmplitudeError) {
			metrics.maxAbsAmplitudeError = stateAbsError;
			metrics.worstState = static_cast<StateIndex>(state);
		}
		metrics.maxAbsComponentError = std::max(
			metrics.maxAbsComponentError,
			std::max(std::abs(diffReal), std::abs(diffImag)));
		diffNormSq += stateDiffSq;
		refNormSq += refReal * refReal + refImag * refImag;

		const double refProbability = refReal * refReal + refImag * refImag;
		const double gotProbability = gotReal * gotReal + gotImag * gotImag;
		refTotalProbability += refProbability;
		gotTotalProbability += gotProbability;
		metrics.maxAbsProbabilityError = std::max(
			metrics.maxAbsProbabilityError,
			std::abs(gotProbability - refProbability));
	}

	metrics.rmseAmplitude = std::sqrt(diffNormSq / static_cast<double>(stateCount));
	metrics.relL2 = refNormSq == 0.0 ? 0.0 : std::sqrt(diffNormSq / refNormSq);
	metrics.totalProbabilityDiff = std::abs(gotTotalProbability - refTotalProbability);
	return metrics;
}

std::string toCsvRow(const std::string &circuit, const std::string &scenario, unsigned int qubits, const StateComparison &comparison) {
	std::ostringstream out;
	out << std::setprecision(17)
	    << circuit << ','
	    << scenario << ','
	    << qubits << ','
	    << (comparison.bitwiseEqual ? "true" : "false") << ','
	    << comparison.maxAbsAmplitudeError << ','
	    << comparison.maxAbsComponentError << ','
	    << comparison.relL2 << ','
	    << comparison.rmseAmplitude << ','
	    << comparison.maxAbsProbabilityError << ','
	    << comparison.totalProbabilityDiff << ','
	    << comparison.worstState;
	return out.str();
}

std::string toJson(const StateComparison &comparison) {
	std::ostringstream out;
	out << std::setprecision(17)
	    << "{"
	    << "\"bitwiseEqual\":" << (comparison.bitwiseEqual ? "true" : "false") << ','
	    << "\"maxAbsAmplitudeError\":" << comparison.maxAbsAmplitudeError << ','
	    << "\"maxAbsComponentError\":" << comparison.maxAbsComponentError << ','
	    << "\"relL2\":" << comparison.relL2 << ','
	    << "\"rmseAmplitude\":" << comparison.rmseAmplitude << ','
	    << "\"maxAbsProbabilityError\":" << comparison.maxAbsProbabilityError << ','
	    << "\"totalProbabilityDiff\":" << comparison.totalProbabilityDiff << ','
	    << "\"worstState\":" << comparison.worstState
	    << "}";
	return out.str();
}

} // namespace experiment
} // namespace tmfqs
