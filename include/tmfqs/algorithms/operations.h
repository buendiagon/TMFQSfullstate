#ifndef TMFQS_ALGORITHMS_OPERATIONS_H
#define TMFQS_ALGORITHMS_OPERATIONS_H

#include <variant>
#include <vector>

#include "tmfqs/core/types.h"
#include "tmfqs/register/quantum_register.h"

namespace tmfqs {
namespace algorithms {

struct HadamardOp { QubitIndex targetQubit = 0; };
struct PauliXOp { QubitIndex targetQubit = 0; };
struct ControlledPhaseShiftOp {
	QubitIndex controlQubit = 0;
	QubitIndex targetQubit = 0;
	double theta = 0.0;
};
struct ControlledNotOp {
	QubitIndex controlQubit = 0;
	QubitIndex targetQubit = 0;
};
struct SwapOp {
	QubitIndex qubitA = 0;
	QubitIndex qubitB = 0;
};
struct PhaseFlipBasisStateOp { StateIndex basisState = 0; };
struct InversionAboutMeanOp {};

using AlgorithmOperation = std::variant<
	HadamardOp,
	PauliXOp,
	ControlledPhaseShiftOp,
	ControlledNotOp,
	SwapOp,
	PhaseFlipBasisStateOp,
	InversionAboutMeanOp>;

struct RepeatBlockStep {
	std::vector<AlgorithmOperation> operations;
	unsigned int repeatCount = 0;
};

using CompiledAlgorithmStep = std::variant<AlgorithmOperation, RepeatBlockStep>;

struct CompiledAlgorithmPlan {
	std::vector<CompiledAlgorithmStep> steps;

	void addOperation(const AlgorithmOperation &operation);
	void addRepeatBlock(std::vector<AlgorithmOperation> operations, unsigned int repeatCount);
};

void executeOperation(QuantumRegister &quantumRegister, const AlgorithmOperation &operation);
void executeOperations(QuantumRegister &quantumRegister, const std::vector<AlgorithmOperation> &operations);
void executePlan(QuantumRegister &quantumRegister, const CompiledAlgorithmPlan &plan);

} // namespace algorithms
} // namespace tmfqs

#endif // TMFQS_ALGORITHMS_OPERATIONS_H
