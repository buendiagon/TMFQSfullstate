#ifndef TMFQS_STORAGE_COMMON_GATE_APPLY_ENGINE_H
#define TMFQS_STORAGE_COMMON_GATE_APPLY_ENGINE_H

#include "tmfqs/storage/gate_block_apply.h"
#include "tmfqs/core/validation.h"

namespace tmfqs {
namespace storage {

/** @brief Shared gate-application helper that validates targets then applies by blocks. */
class GateApplyEngine {
	public:
		template <typename LoadAmplitudeFn, typename StoreAmplitudeFn>
		/** @brief Applies `gate` over `qubits` using caller-provided load/store callbacks. */
		static void apply(
			const char *scopeName,
			const QuantumGate &gate,
			const QubitList &qubits,
			unsigned int numQubits,
			LoadAmplitudeFn loadAmplitude,
			StoreAmplitudeFn storeAmplitude,
			GateBlockWorkspace &workspace) {
			validateGateTargets(scopeName, qubits, numQubits, gate.dimension());
			const GateBlockLayout layout = makeGateBlockLayout(qubits, numQubits);
			applyGateByBlocks(gate, layout, loadAmplitude, storeAmplitude, workspace);
		}
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_COMMON_GATE_APPLY_ENGINE_H
