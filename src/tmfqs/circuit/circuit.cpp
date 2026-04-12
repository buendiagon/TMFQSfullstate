#include "tmfqs/circuit/circuit.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>

#include "tmfqs/core/constants.h"
#include "tmfqs/core/state_space.h"

namespace tmfqs {
namespace circuit {

Circuit::Circuit(unsigned int qubits) : qubits_(qubits) {}

unsigned int Circuit::qubitCount() const noexcept {
	return qubits_;
}

const std::vector<Operation> &Circuit::operations() const noexcept {
	return operations_;
}

size_t Circuit::operationCount() const noexcept {
	return operations_.size();
}

Circuit &Circuit::h(QubitIndex target) {
	return append(Hadamard{target});
}

Circuit &Circuit::x(QubitIndex target) {
	return append(PauliX{target});
}

Circuit &Circuit::cx(QubitIndex control, QubitIndex target) {
	return append(ControlledNot{control, target});
}

Circuit &Circuit::controlledPhase(QubitIndex control, QubitIndex target, double theta) {
	return append(ControlledPhaseShift{control, target, theta});
}

Circuit &Circuit::swap(QubitIndex first, QubitIndex second) {
	return append(Swap{first, second});
}

Circuit &Circuit::gate(QuantumGate gateValue, QubitList targets) {
	return append(MatrixGate{std::move(gateValue), std::move(targets)});
}

Circuit &Circuit::phaseFlip(StateIndex state) {
	return append(PhaseFlipBasisState{state});
}

Circuit &Circuit::inversionAboutMean(bool materialized) {
	return append(InversionAboutMean{materialized});
}

Circuit &Circuit::append(const Operation &operation) {
	operations_.push_back(operation);
	return *this;
}

Circuit &Circuit::append(Operation &&operation) {
	operations_.push_back(std::move(operation));
	return *this;
}

std::string operationName(const Operation &operation) {
	return std::visit(
		[](const auto &op) -> std::string {
			using Op = std::decay_t<decltype(op)>;
			if constexpr(std::is_same<Op, Hadamard>::value) return "h";
			if constexpr(std::is_same<Op, PauliX>::value) return "x";
			if constexpr(std::is_same<Op, ControlledNot>::value) return "cx";
			if constexpr(std::is_same<Op, ControlledPhaseShift>::value) return "controlled_phase";
			if constexpr(std::is_same<Op, Swap>::value) return "swap";
			if constexpr(std::is_same<Op, MatrixGate>::value) return "gate";
			if constexpr(std::is_same<Op, PhaseFlipBasisState>::value) return "phase_flip_basis_state";
			if constexpr(std::is_same<Op, InversionAboutMean>::value) return "inversion_about_mean";
			return "unknown";
		},
		operation);
}

void appendQft(Circuit &circuit) {
	const unsigned int qubits = circuit.qubitCount();
	for(unsigned int target = 0; target < qubits; ++target) {
		circuit.h(target);
		for(unsigned int control = target + 1u; control < qubits; ++control) {
			const unsigned int distance = control - target;
			const double theta = kPi / static_cast<double>(StateIndex{1u} << distance);
			circuit.controlledPhase(control, target, theta);
		}
	}
	for(unsigned int left = 0; left < qubits / 2u; ++left) {
		circuit.swap(left, qubits - left - 1u);
	}
}

Circuit makeQft(unsigned int qubits) {
	Circuit circuit(qubits);
	appendQft(circuit);
	return circuit;
}

unsigned int groverIterationCount(StateIndex stateCount, size_t markedCount) {
	if(markedCount == 0u) {
		throw std::invalid_argument("Grover circuit requires at least one marked state");
	}
	const double idealIterations =
		(kPi / 4.0) *
		std::sqrt(static_cast<double>(stateCount) / static_cast<double>(markedCount));
	return static_cast<unsigned int>(std::floor(idealIterations));
}

void appendGrover(Circuit &circuit, const GroverCircuitOptions &options) {
	std::vector<StateIndex> marked = options.markedStates.values();
	if(marked.empty()) {
		throw std::invalid_argument("Grover circuit requires at least one marked state");
	}
	std::sort(marked.begin(), marked.end());
	marked.erase(std::unique(marked.begin(), marked.end()), marked.end());

	for(unsigned int q = 0; q < circuit.qubitCount(); ++q) {
		circuit.h(q);
	}

	const StateIndex stateCount = checkedStateCount(circuit.qubitCount());
	const unsigned int iterations = groverIterationCount(stateCount, marked.size());
	for(unsigned int iteration = 0; iteration < iterations; ++iteration) {
		for(StateIndex state : marked) {
			if(state >= stateCount) {
				throw std::invalid_argument("Grover marked state is out of range");
			}
			circuit.phaseFlip(state);
		}
		circuit.inversionAboutMean(options.materializedDiffusion);
	}
}

Circuit makeGrover(unsigned int qubits, GroverCircuitOptions options) {
	Circuit circuit(qubits);
	appendGrover(circuit, options);
	return circuit;
}

} // namespace circuit
} // namespace tmfqs
