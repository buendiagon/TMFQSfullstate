#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "quantumAlgorithms.h"
#include "stateSpace.h"

void CompiledAlgorithmPlan::addOperation(const AlgorithmOperation &op) {
	steps.push_back({CompiledAlgorithmStepKind::Operation, op, {}, 0});
}

void CompiledAlgorithmPlan::addRepeatBlock(std::vector<AlgorithmOperation> repeatedOps, unsigned int repeatCount) {
	if(repeatCount == 0) return;
	if(repeatedOps.empty()) return;
	steps.push_back({CompiledAlgorithmStepKind::RepeatBlock, {}, std::move(repeatedOps), repeatCount});
}

namespace {
void executeAlgorithmOperation(QuantumRegister &qureg, const AlgorithmOperation &op) {
	switch(op.kind) {
		case AlgorithmOperationKind::Hadamard:
			qureg.Hadamard(op.q0);
			break;
		case AlgorithmOperationKind::PauliX:
			qureg.PauliX(op.q0);
			break;
		case AlgorithmOperationKind::ControlledPhaseShift:
			qureg.ControlledPhaseShift(op.q0, op.q1, op.theta);
			break;
		case AlgorithmOperationKind::ControlledNot:
			qureg.ControlledNot(op.q0, op.q1);
			break;
		case AlgorithmOperationKind::Swap:
			qureg.Swap(op.q0, op.q1);
			break;
		case AlgorithmOperationKind::PhaseFlipBasisState:
			qureg.phaseFlipBasisState(op.q0);
			break;
		case AlgorithmOperationKind::InversionAboutMean:
			qureg.inversionAboutMean();
			break;
	}
}
} // namespace

void executeAlgorithmOperations(QuantumRegister &qureg, const std::vector<AlgorithmOperation> &ops) {
	for(const AlgorithmOperation &op : ops) {
		executeAlgorithmOperation(qureg, op);
	}
}

void executeCompiledAlgorithmPlan(QuantumRegister &qureg, const CompiledAlgorithmPlan &plan) {
	for(const CompiledAlgorithmStep &step : plan.steps) {
		switch(step.kind) {
			case CompiledAlgorithmStepKind::Operation:
				executeAlgorithmOperation(qureg, step.operation);
				break;
			case CompiledAlgorithmStepKind::RepeatBlock:
				for(unsigned int iteration = 0; iteration < step.repeatCount; ++iteration) {
					for(const AlgorithmOperation &op : step.repeatedOperations) {
						executeAlgorithmOperation(qureg, op);
					}
				}
				break;
		}
	}
}

// In-place QFT using the standard sequence:
// Hadamard on each qubit, controlled phase rotations, then final bit-reversal swaps.
void quantumFourierTransform(QuantumRegister &qureg) {
	unsigned int numQubits = qureg.qubitCount();
	std::vector<AlgorithmOperation> ops;
	ops.reserve(static_cast<size_t>(numQubits) * (static_cast<size_t>(numQubits) + 1u) / 2u
		+ static_cast<size_t>(numQubits / 2u));
	for(unsigned int j = 0; j < numQubits; ++j) {
		ops.push_back({AlgorithmOperationKind::Hadamard, j});
		for(unsigned int k = 1; j + k < numQubits; ++k) {
			ops.push_back({
				AlgorithmOperationKind::ControlledPhaseShift,
				j + k,
				j,
				pi / static_cast<double>(1u << k)
			});
		}
	}

	for(unsigned int i = 0; i < (numQubits / 2u); ++i) {
		ops.push_back({AlgorithmOperationKind::Swap, i, numQubits - i - 1});
	}
	executeAlgorithmOperations(qureg, ops);
}

// Basic Grover search implementation over 2^numBits states.
unsigned int Grover(unsigned int omega, unsigned int numBits, bool verbose) {
	unsigned int N = checkedStateCount(numBits);
	if(omega >= N) {
		throw std::invalid_argument("Grover: marked state index is out of range for numBits");
	}

	QuantumRegister qureg(numBits);
	CompiledAlgorithmPlan plan;
	for(unsigned int i = 0; i < numBits; ++i) {
		plan.addOperation({AlgorithmOperationKind::Hadamard, i});
	}
	const std::vector<AlgorithmOperation> groverIterationOps = {
		{AlgorithmOperationKind::PhaseFlipBasisState, omega},
		{AlgorithmOperationKind::InversionAboutMean}
	};
	unsigned int iterations = static_cast<unsigned int>(
		std::round((pi / 4.0) * std::sqrt(static_cast<double>(N))));
	plan.addRepeatBlock(groverIterationOps, iterations);
	executeCompiledAlgorithmPlan(qureg, plan);

	if(verbose) qureg.printStatesVector();
	return qureg.measure();
}
