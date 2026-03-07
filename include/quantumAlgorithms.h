#ifndef QUANTUM_ALGORITHMS_INCLUDE
#define QUANTUM_ALGORITHMS_INCLUDE

#include <vector>

#include "quantumRegister.h"

// Shared math constant for angle-based algorithms and gates.
inline constexpr double pi = 3.14159265358979323846;

// Backend-friendly operation set used by algorithm executors.
enum class AlgorithmOperationKind {
	Hadamard,
	PauliX,
	ControlledPhaseShift,
	ControlledNot,
	Swap,
	PhaseFlipBasisState,
	InversionAboutMean
};

struct AlgorithmOperation {
	AlgorithmOperationKind kind;
	unsigned int q0 = 0;
	unsigned int q1 = 0;
	double theta = 0.0;
};

enum class CompiledAlgorithmStepKind {
	Operation,
	RepeatBlock
};

struct CompiledAlgorithmStep {
	CompiledAlgorithmStepKind kind = CompiledAlgorithmStepKind::Operation;
	AlgorithmOperation operation{};
	std::vector<AlgorithmOperation> repeatedOperations;
	unsigned int repeatCount = 0;
};

struct CompiledAlgorithmPlan {
	std::vector<CompiledAlgorithmStep> steps;

	void addOperation(const AlgorithmOperation &op);
	void addRepeatBlock(std::vector<AlgorithmOperation> repeatedOps, unsigned int repeatCount);
};

// Executes a sequence of algorithm operations against a register.
void executeAlgorithmOperations(QuantumRegister &qureg, const std::vector<AlgorithmOperation> &ops);
// Executes a compiled algorithm plan containing single operations and repeat blocks.
void executeCompiledAlgorithmPlan(QuantumRegister &qureg, const CompiledAlgorithmPlan &plan);

// In-place quantum Fourier transform over the whole register.
void quantumFourierTransform(QuantumRegister &qureg);
// Runs Grover search and returns the measured candidate index.
unsigned int Grover(unsigned int omega, unsigned int numBits, bool verbose);


#endif //QUANTUM_ALGORITHMS_INCLUDE
