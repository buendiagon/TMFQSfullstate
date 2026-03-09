#include "tmfqs/algorithms/operations.h"

#include <type_traits>
#include <utility>

namespace tmfqs {
namespace algorithms {
namespace {

template <typename ExecuteFn>
void runWithOperationBatch(QuantumRegister &quantumRegister, ExecuteFn &&executeFn) {
	quantumRegister.beginOperationBatch();
	bool batchOpen = true;
	try {
		executeFn();
		quantumRegister.endOperationBatch();
		batchOpen = false;
	} catch(...) {
		if(batchOpen) {
			try {
				quantumRegister.endOperationBatch();
			} catch(...) {}
		}
		throw;
	}
}

} // namespace

void CompiledAlgorithmPlan::addOperation(const AlgorithmOperation &operation) {
	steps.push_back(operation);
}

void CompiledAlgorithmPlan::addRepeatBlock(std::vector<AlgorithmOperation> operations, unsigned int repeatCount) {
	if(repeatCount == 0 || operations.empty()) {
		return;
	}
	steps.push_back(RepeatBlockStep{std::move(operations), repeatCount});
}

void executeOperation(QuantumRegister &quantumRegister, const AlgorithmOperation &operation) {
	std::visit(
		[&](const auto &op) {
			using Op = std::decay_t<decltype(op)>;
			if constexpr(std::is_same<Op, HadamardOp>::value) {
				quantumRegister.applyHadamard(op.targetQubit);
			} else if constexpr(std::is_same<Op, PauliXOp>::value) {
				quantumRegister.applyPauliX(op.targetQubit);
			} else if constexpr(std::is_same<Op, ControlledPhaseShiftOp>::value) {
				quantumRegister.applyControlledPhaseShift(op.controlQubit, op.targetQubit, op.theta);
			} else if constexpr(std::is_same<Op, ControlledNotOp>::value) {
				quantumRegister.applyControlledNot(op.controlQubit, op.targetQubit);
			} else if constexpr(std::is_same<Op, SwapOp>::value) {
				quantumRegister.applySwap(op.qubitA, op.qubitB);
			} else if constexpr(std::is_same<Op, PhaseFlipBasisStateOp>::value) {
				quantumRegister.applyPhaseFlipBasisState(op.basisState);
			} else if constexpr(std::is_same<Op, InversionAboutMeanOp>::value) {
				quantumRegister.applyInversionAboutMean();
			}
		},
		operation);
}

void executeOperations(QuantumRegister &quantumRegister, const std::vector<AlgorithmOperation> &operations) {
	runWithOperationBatch(quantumRegister, [&]() {
		for(const AlgorithmOperation &operation : operations) {
			executeOperation(quantumRegister, operation);
		}
	});
}

void executePlan(QuantumRegister &quantumRegister, const CompiledAlgorithmPlan &plan) {
	runWithOperationBatch(quantumRegister, [&]() {
		for(const CompiledAlgorithmStep &step : plan.steps) {
			std::visit(
				[&](const auto &item) {
					using Item = std::decay_t<decltype(item)>;
					if constexpr(std::is_same<Item, AlgorithmOperation>::value) {
						executeOperation(quantumRegister, item);
					} else if constexpr(std::is_same<Item, RepeatBlockStep>::value) {
						for(unsigned int iteration = 0; iteration < item.repeatCount; ++iteration) {
							executeOperations(quantumRegister, item.operations);
						}
					}
				},
				step);
		}
	});
}

} // namespace algorithms
} // namespace tmfqs
