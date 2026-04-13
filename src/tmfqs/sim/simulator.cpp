#include "tmfqs/sim/simulator.h"

#include <chrono>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "tmfqs/register/quantum_register.h"
#include "tmfqs/storage/factory/storage_strategy_registry.h"

namespace tmfqs {
namespace sim {
namespace {

using Clock = std::chrono::steady_clock;

void executeOperation(QuantumRegister &reg, const circuit::Operation &operation) {
	std::visit(
		[&](const auto &op) {
			using Op = std::decay_t<decltype(op)>;
			if constexpr(std::is_same<Op, circuit::Hadamard>::value) {
				reg.applyHadamard(op.target);
			} else if constexpr(std::is_same<Op, circuit::PauliX>::value) {
				reg.applyPauliX(op.target);
			} else if constexpr(std::is_same<Op, circuit::ControlledNot>::value) {
				reg.applyControlledNot(op.control, op.target);
			} else if constexpr(std::is_same<Op, circuit::ControlledPhaseShift>::value) {
				reg.applyControlledPhaseShift(op.control, op.target, op.theta);
			} else if constexpr(std::is_same<Op, circuit::Swap>::value) {
				reg.applySwap(op.first, op.second);
			} else if constexpr(std::is_same<Op, circuit::MatrixGate>::value) {
				reg.applyGate(op.gate, op.targets);
			} else if constexpr(std::is_same<Op, circuit::PhaseFlipBasisState>::value) {
				reg.applyPhaseFlipBasisState(op.state);
			} else if constexpr(std::is_same<Op, circuit::InversionAboutMean>::value) {
				if(op.materialized) {
					reg.applyInversionAboutMeanMaterialized();
				} else {
					reg.applyInversionAboutMean();
				}
			}
		},
		operation);
}

} // namespace

Simulator::Simulator(ExecutionConfig config) : config_(std::move(config)) {}

const ExecutionConfig &Simulator::config() const noexcept {
	return config_;
}

RunResult Simulator::run(const circuit::Circuit &circuit, const state::QuantumState &initialState) const {
	if(circuit.qubitCount() != initialState.qubitCount()) {
		throw std::invalid_argument("Circuit and initial state qubit counts differ");
	}

	RegisterConfig effectiveConfig = config_.backend;
	QuantumRegister reg = initialState.materialize(effectiveConfig);

	experiment::RunReport report;
	report.qubits = circuit.qubitCount();
	report.operationCount = circuit.operationCount();
	report.strategy = StorageStrategyRegistry::resolve(circuit.qubitCount(), effectiveConfig);

	const auto runStart = Clock::now();
	reg.beginOperationBatch();
	bool batchOpen = true;
	try {
		const std::vector<circuit::Operation> &ops = circuit.operations();
		for(size_t idx = 0; idx < ops.size(); ++idx) {
			const auto opStart = Clock::now();
			executeOperation(reg, ops[idx]);
			if(config_.observability.traceOperations) {
				report.operations.push_back({
					idx,
					circuit::operationName(ops[idx]),
					std::chrono::duration<double>(Clock::now() - opStart).count()
				});
			}
		}
		reg.endOperationBatch();
		batchOpen = false;
	} catch(...) {
		if(batchOpen) {
			try {
				reg.endOperationBatch();
			} catch(...) {}
		}
		throw;
	}
	report.executionSeconds = std::chrono::duration<double>(Clock::now() - runStart).count();

	return {state::QuantumState::fromRegister(std::move(reg)), std::move(report)};
}

} // namespace sim
} // namespace tmfqs
