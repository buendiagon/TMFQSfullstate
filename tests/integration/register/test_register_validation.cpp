#include "tmfqsfs.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

#include "../common/test_helpers.h"

namespace {

void testDefaultConstructedRegisterIsValid() {
	using namespace tmfqs;
	std::cout << "=== default register validity ===\n";
	QuantumRegister qreg;
	assert(qreg.qubitCount() == 0u);
	assert(qreg.stateCount() == 1u);
	assert(qreg.amplitudeElementCount() == 2u);
	const Amplitude amp = qreg.amplitude(0);
	assert(tmfqs_test::approxEqual(amp.real, 1.0, 1e-12));
	assert(tmfqs_test::approxEqual(amp.imag, 0.0, 1e-12));
	assert(tmfqs_test::approxEqual(qreg.totalProbability(), 1.0, 1e-12));
	Mt19937RandomSource randomSource(1u);
	assert(qreg.measure(randomSource) == 0u);

	bool threw = false;
	try {
		qreg.applyHadamard(0);
	} catch(const std::out_of_range &) {
		threw = true;
	}
	assert(threw);
}

void testOutOfRangeQueriesThrow() {
	using namespace tmfqs;
	std::cout << "=== out-of-range queries ===\n";
	QuantumRegister qreg(2, 0);
	bool ampThrew = false;
	try {
		(void)qreg.amplitude(4);
	} catch(const std::out_of_range &) {
		ampThrew = true;
	}
	assert(ampThrew);

	bool probThrew = false;
	try {
		(void)qreg.probability(4);
	} catch(const std::out_of_range &) {
		probThrew = true;
	}
	assert(probThrew);
}

void testPublicGateValidation() {
	using namespace tmfqs;
	std::cout << "=== public gate validation ===\n";
	QuantumRegister qreg(2, 0);

	bool hadamardThrew = false;
	try {
		qreg.applyHadamard(2);
	} catch(const std::out_of_range &) {
		hadamardThrew = true;
	}
	assert(hadamardThrew);

	bool pauliXThrew = false;
	try {
		qreg.applyPauliX(2);
	} catch(const std::out_of_range &) {
		pauliXThrew = true;
	}
	assert(pauliXThrew);

	bool cnotRangeThrew = false;
	try {
		qreg.applyControlledNot(0, 2);
	} catch(const std::out_of_range &) {
		cnotRangeThrew = true;
	}
	assert(cnotRangeThrew);

	bool cnotDistinctThrew = false;
	try {
		qreg.applyControlledNot(1, 1);
	} catch(const std::invalid_argument &) {
		cnotDistinctThrew = true;
	}
	assert(cnotDistinctThrew);

	bool swapDistinctThrew = false;
	try {
		qreg.applySwap(0, 0);
	} catch(const std::invalid_argument &) {
		swapDistinctThrew = true;
	}
	assert(swapDistinctThrew);
}

void testApplyGateValidation() {
	using namespace tmfqs;
	std::cout << "=== applyGate validation ===\n";
	QuantumRegister qreg(2, 0);

	bool duplicateThrew = false;
	try {
		qreg.applyGate(QuantumGate::ControlledNot(), QubitList{1, 1});
	} catch(const std::invalid_argument &) {
		duplicateThrew = true;
	}
	assert(duplicateThrew);

	bool rangeThrew = false;
	try {
		qreg.applyGate(QuantumGate::Hadamard(), QubitList{2});
	} catch(const std::out_of_range &) {
		rangeThrew = true;
	}
	assert(rangeThrew);
}

void testUniformSuperpositionValidation() {
	using namespace tmfqs;
	std::cout << "=== uniform superposition validation ===\n";
	QuantumRegister qreg(3, 0);

	bool emptyThrew = false;
	try {
		qreg.initUniformSuperposition(BasisStateList{});
	} catch(const std::invalid_argument &) {
		emptyThrew = true;
	}
	assert(emptyThrew);

	bool outOfRangeThrew = false;
	try {
		qreg.initUniformSuperposition(BasisStateList{0, 8});
	} catch(const std::out_of_range &) {
		outOfRangeThrew = true;
	}
	assert(outOfRangeThrew);
}

} // namespace

int main() {
	testDefaultConstructedRegisterIsValid();
	testOutOfRangeQueriesThrow();
	testPublicGateValidation();
	testApplyGateValidation();
	testUniformSuperpositionValidation();
	std::cout << "Register validation tests passed.\n";
	return 0;
}
