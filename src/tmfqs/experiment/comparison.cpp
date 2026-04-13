#include "tmfqs/experiment/comparison.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace tmfqs {
namespace experiment {

StateComparison compareStates(const state::QuantumState &reference, const state::QuantumState &candidate) {
	if(reference.stateCount() != candidate.stateCount()) {
		throw std::invalid_argument("Compared states must have the same state count");
	}

	StateComparison metrics;
	metrics.bitwiseEqual = true;

	double diffNormSq = 0.0;
	double refNormSq = 0.0;
	double refTotalProbability = 0.0;
	double gotTotalProbability = 0.0;
	const StateIndex stateCount = reference.stateCount();

	for(StateIndex state = 0; state < stateCount; ++state) {
		const Amplitude ref = reference.amplitude(state);
		const Amplitude got = candidate.amplitude(state);
		const double refReal = ref.real;
		const double refImag = ref.imag;
		const double gotReal = got.real;
		const double gotImag = got.imag;
		if(refReal != gotReal || refImag != gotImag) {
			metrics.bitwiseEqual = false;
		}
		const double diffReal = gotReal - refReal;
		const double diffImag = gotImag - refImag;
		const double stateDiffSq = diffReal * diffReal + diffImag * diffImag;
		const double stateAbsError = std::sqrt(stateDiffSq);
		if(stateAbsError > metrics.maxAbsAmplitudeError) {
			metrics.maxAbsAmplitudeError = stateAbsError;
			metrics.worstState = state;
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
