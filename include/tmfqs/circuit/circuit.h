#ifndef TMFQS_CIRCUIT_CIRCUIT_H
#define TMFQS_CIRCUIT_CIRCUIT_H

#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "tmfqs/core/types.h"
#include "tmfqs/gates/quantum_gate.h"

namespace tmfqs {
namespace circuit {

struct Hadamard {
	QubitIndex target = 0;
};

struct PauliX {
	QubitIndex target = 0;
};

struct ControlledNot {
	QubitIndex control = 0;
	QubitIndex target = 0;
};

struct ControlledPhaseShift {
	QubitIndex control = 0;
	QubitIndex target = 0;
	double theta = 0.0;
};

struct Swap {
	QubitIndex first = 0;
	QubitIndex second = 0;
};

struct MatrixGate {
	QuantumGate gate;
	QubitList targets;
};

struct PhaseFlipBasisState {
	StateIndex state = 0;
};

struct InversionAboutMean {
	bool materialized = false;
};

using Operation = std::variant<
	Hadamard,
	PauliX,
	ControlledNot,
	ControlledPhaseShift,
	Swap,
	MatrixGate,
	PhaseFlipBasisState,
	InversionAboutMean>;

class Circuit {
	public:
		explicit Circuit(unsigned int qubits = 0);

		unsigned int qubitCount() const noexcept;
		const std::vector<Operation> &operations() const noexcept;
		size_t operationCount() const noexcept;

		Circuit &h(QubitIndex target);
		Circuit &x(QubitIndex target);
		Circuit &cx(QubitIndex control, QubitIndex target);
		Circuit &controlledPhase(QubitIndex control, QubitIndex target, double theta);
		Circuit &swap(QubitIndex first, QubitIndex second);
		Circuit &gate(QuantumGate gate, QubitList targets);
		Circuit &phaseFlip(StateIndex state);
		Circuit &inversionAboutMean(bool materialized = false);
		Circuit &append(const Operation &operation);
		Circuit &append(Operation &&operation);

	private:
		unsigned int qubits_ = 0;
		std::vector<Operation> operations_;
};

std::string operationName(const Operation &operation);

void appendQft(Circuit &circuit);
Circuit makeQft(unsigned int qubits);

struct GroverCircuitOptions {
	BasisStateList markedStates;
	bool materializedDiffusion = false;
};

unsigned int groverIterationCount(StateIndex stateCount, size_t markedCount);
void appendGrover(Circuit &circuit, const GroverCircuitOptions &options);
Circuit makeGrover(unsigned int qubits, GroverCircuitOptions options);

} // namespace circuit
} // namespace tmfqs

#endif // TMFQS_CIRCUIT_CIRCUIT_H
