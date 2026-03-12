#ifndef TMFQS_ALGORITHMS_OPERATIONS_H
#define TMFQS_ALGORITHMS_OPERATIONS_H

#include <variant>
#include <vector>

#include "tmfqs/core/types.h"
#include "tmfqs/register/quantum_register.h"

namespace tmfqs {
namespace algorithms {

/** @brief Descriptor for a single-qubit Hadamard operation. */
struct HadamardOp { QubitIndex targetQubit = 0; };
/** @brief Descriptor for a single-qubit Pauli-X operation. */
struct PauliXOp { QubitIndex targetQubit = 0; };
/** @brief Descriptor for a controlled phase-shift operation. */
struct ControlledPhaseShiftOp {
	/** @brief Control qubit index. */
	QubitIndex controlQubit = 0;
	/** @brief Target qubit index. */
	QubitIndex targetQubit = 0;
	/** @brief Rotation angle in radians. */
	double theta = 0.0;
};
/** @brief Descriptor for a controlled-NOT operation. */
struct ControlledNotOp {
	/** @brief Control qubit index. */
	QubitIndex controlQubit = 0;
	/** @brief Target qubit index. */
	QubitIndex targetQubit = 0;
};
/** @brief Descriptor for a SWAP operation. */
struct SwapOp {
	/** @brief First qubit to swap. */
	QubitIndex qubitA = 0;
	/** @brief Second qubit to swap. */
	QubitIndex qubitB = 0;
};
/** @brief Descriptor for flipping phase of one basis state. */
struct PhaseFlipBasisStateOp { StateIndex basisState = 0; };
/** @brief Descriptor for inversion-about-mean (Grover diffusion). */
struct InversionAboutMeanOp {};

/**
 * @brief Tagged union that stores one supported algorithm operation descriptor.
 */
using AlgorithmOperation = std::variant<
	HadamardOp,
	PauliXOp,
	ControlledPhaseShiftOp,
	ControlledNotOp,
	SwapOp,
	PhaseFlipBasisStateOp,
	InversionAboutMeanOp>;

/**
 * @brief Plan step that repeats a linear operation block.
 */
struct RepeatBlockStep {
	/** @brief Operations executed in each iteration. */
	std::vector<AlgorithmOperation> operations;
	/** @brief Number of times to execute `operations`. */
	unsigned int repeatCount = 0;
};

/** @brief One compiled step: either one operation or one repeat block. */
using CompiledAlgorithmStep = std::variant<AlgorithmOperation, RepeatBlockStep>;

/**
 * @brief Executable operation plan with optional repeated blocks.
 */
struct CompiledAlgorithmPlan {
	/** @brief Ordered list of executable steps. */
	std::vector<CompiledAlgorithmStep> steps;

	/**
	 * @brief Appends one operation step to the plan.
	 * @param operation Operation descriptor to append.
	 */
	void addOperation(const AlgorithmOperation &operation);
	/**
	 * @brief Appends a repeated operation block.
	 * @param operations Operations that compose one iteration.
	 * @param repeatCount Number of iterations to execute.
	 */
	void addRepeatBlock(std::vector<AlgorithmOperation> operations, unsigned int repeatCount);
};

/**
 * @brief Executes one operation descriptor.
 * @param quantumRegister Register to mutate.
 * @param operation Operation to execute.
 */
void executeOperation(QuantumRegister &quantumRegister, const AlgorithmOperation &operation);
/**
 * @brief Executes a linear operation list.
 * @param quantumRegister Register to mutate.
 * @param operations Ordered operation list.
 */
void executeOperations(QuantumRegister &quantumRegister, const std::vector<AlgorithmOperation> &operations);
/**
 * @brief Executes all steps in a compiled plan.
 * @param quantumRegister Register to mutate.
 * @param plan Compiled plan to execute.
 */
void executePlan(QuantumRegister &quantumRegister, const CompiledAlgorithmPlan &plan);

} // namespace algorithms
} // namespace tmfqs

#endif // TMFQS_ALGORITHMS_OPERATIONS_H
