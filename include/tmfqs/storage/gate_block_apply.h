#ifndef TMFQS_STORAGE_GATE_BLOCK_APPLY_H
#define TMFQS_STORAGE_GATE_BLOCK_APPLY_H

#include <cstddef>
#include <vector>

#include "tmfqs/core/state_space.h"
#include "tmfqs/core/types.h"
#include "tmfqs/gates/quantum_gate.h"

namespace tmfqs {

struct GateBlockLayout {
	unsigned int numQubits = 0;
	unsigned int activeQubits = 0;
	unsigned int numBlocks = 0;
	unsigned int blockSize = 0;
	std::vector<unsigned int> targetPositions;
	std::vector<unsigned int> targetMasks;
	std::vector<unsigned int> nonTargetPositions;
	std::vector<unsigned int> blockOffsets;
};

struct GateBlockWorkspace {
	std::vector<Amplitude> localAmps;
	std::vector<StateIndex> stateIndices;
	std::vector<const Amplitude *> gateRows;

	void ensure(size_t blockSize) {
		if(localAmps.size() != blockSize) {
			localAmps.resize(blockSize);
			stateIndices.resize(blockSize);
		}
		if(gateRows.size() != blockSize) {
			gateRows.resize(blockSize);
		}
	}
};

inline unsigned int composeBaseStateFromBlock(unsigned int block, const std::vector<unsigned int> &nonTargetPositions) {
	unsigned int baseState = 0;
	for(size_t idx = 0; idx < nonTargetPositions.size(); ++idx) {
		baseState |= ((block >> static_cast<unsigned int>(idx)) & 1u) << nonTargetPositions[idx];
	}
	return baseState;
}

inline GateBlockLayout makeGateBlockLayout(const QubitList &qubits, unsigned int numQubits) {
	GateBlockLayout layout;
	layout.numQubits = numQubits;
	layout.activeQubits = static_cast<unsigned int>(qubits.size());
	layout.numBlocks = checkedStateCount(numQubits - layout.activeQubits);
	layout.blockSize = checkedStateCount(layout.activeQubits);
	layout.targetPositions.resize(layout.activeQubits);
	layout.targetMasks.resize(layout.activeQubits);
	layout.nonTargetPositions.reserve(numQubits - layout.activeQubits);
	layout.blockOffsets.resize(layout.blockSize, 0u);

	std::vector<bool> isTargetBit(numQubits, false);
	for(unsigned int idx = 0; idx < layout.activeQubits; ++idx) {
		const unsigned int targetBit = numQubits - qubits[idx] - 1u;
		layout.targetPositions[idx] = targetBit;
		layout.targetMasks[idx] = 1u << targetBit;
		isTargetBit[targetBit] = true;
	}
	for(unsigned int bit = 0; bit < numQubits; ++bit) {
		if(!isTargetBit[bit]) {
			layout.nonTargetPositions.push_back(bit);
		}
	}
	for(unsigned int idx = 0; idx < layout.blockSize; ++idx) {
		unsigned int offset = 0;
		for(unsigned int bit = 0; bit < layout.activeQubits; ++bit) {
			const unsigned int bitVal = (idx >> (layout.activeQubits - bit - 1u)) & 1u;
			if(bitVal != 0u) {
				offset |= layout.targetMasks[bit];
			}
		}
		layout.blockOffsets[idx] = offset;
	}

	return layout;
}

template <typename LoadAmplitudeFn, typename StoreAmplitudeFn>
inline void applyGateByBlocks(
	const QuantumGate &gate,
	const GateBlockLayout &layout,
	LoadAmplitudeFn loadAmplitude,
	StoreAmplitudeFn storeAmplitude,
	GateBlockWorkspace &workspace) {
	workspace.ensure(layout.blockSize);
	std::vector<Amplitude> &localAmps = workspace.localAmps;
	std::vector<StateIndex> &stateIndices = workspace.stateIndices;
	for(unsigned int row = 0; row < layout.blockSize; ++row) {
		workspace.gateRows[row] = gate[row];
	}
	const std::vector<const Amplitude *> &gateRows = workspace.gateRows;

	if(layout.activeQubits == 1u) {
		const unsigned int q0Mask = layout.targetMasks[0];
		const Amplitude *row0 = gateRows[0];
		const Amplitude *row1 = gateRows[1];
		for(unsigned int block = 0; block < layout.numBlocks; ++block) {
			const unsigned int baseState = composeBaseStateFromBlock(block, layout.nonTargetPositions);
			const StateIndex state0 = baseState;
			const StateIndex state1 = baseState | q0Mask;
			const Amplitude a0 = loadAmplitude(state0);
			const Amplitude a1 = loadAmplitude(state1);
			if((a0.real == 0.0 && a0.imag == 0.0) && (a1.real == 0.0 && a1.imag == 0.0)) continue;

			Amplitude out0{0.0, 0.0};
			Amplitude out1{0.0, 0.0};
			const Amplitude g00 = row0[0];
			const Amplitude g01 = row0[1];
			const Amplitude g10 = row1[0];
			const Amplitude g11 = row1[1];
			out0.real = g00.real * a0.real - g00.imag * a0.imag + g01.real * a1.real - g01.imag * a1.imag;
			out0.imag = g00.real * a0.imag + g00.imag * a0.real + g01.real * a1.imag + g01.imag * a1.real;
			out1.real = g10.real * a0.real - g10.imag * a0.imag + g11.real * a1.real - g11.imag * a1.imag;
			out1.imag = g10.real * a0.imag + g10.imag * a0.real + g11.real * a1.imag + g11.imag * a1.real;
			storeAmplitude(state0, out0);
			storeAmplitude(state1, out1);
		}
		return;
	}

	if(layout.activeQubits == 2u) {
		const unsigned int q0Mask = layout.targetMasks[0];
		const unsigned int q1Mask = layout.targetMasks[1];
		for(unsigned int block = 0; block < layout.numBlocks; ++block) {
			const unsigned int baseState = composeBaseStateFromBlock(block, layout.nonTargetPositions);

			const StateIndex state00 = baseState;
			const StateIndex state01 = baseState | q1Mask;
			const StateIndex state10 = baseState | q0Mask;
			const StateIndex state11 = baseState | q0Mask | q1Mask;
			localAmps[0] = loadAmplitude(state00);
			localAmps[1] = loadAmplitude(state01);
			localAmps[2] = loadAmplitude(state10);
			localAmps[3] = loadAmplitude(state11);
			if((localAmps[0].real == 0.0 && localAmps[0].imag == 0.0) &&
				(localAmps[1].real == 0.0 && localAmps[1].imag == 0.0) &&
				(localAmps[2].real == 0.0 && localAmps[2].imag == 0.0) &&
				(localAmps[3].real == 0.0 && localAmps[3].imag == 0.0)) {
				continue;
			}
			stateIndices[0] = state00;
			stateIndices[1] = state01;
			stateIndices[2] = state10;
			stateIndices[3] = state11;

			for(unsigned int row = 0; row < 4u; ++row) {
				Amplitude newAmp{0.0, 0.0};
				const Amplitude *gRow = gateRows[row];
				for(unsigned int col = 0; col < 4u; ++col) {
					const Amplitude g = gRow[col];
					const Amplitude a = localAmps[col];
					newAmp.real += g.real * a.real - g.imag * a.imag;
					newAmp.imag += g.real * a.imag + g.imag * a.real;
				}
				storeAmplitude(stateIndices[row], newAmp);
			}
		}
		return;
	}

	for(unsigned int block = 0; block < layout.numBlocks; ++block) {
		const unsigned int baseState = composeBaseStateFromBlock(block, layout.nonTargetPositions);

		bool hasNonZero = false;
		for(unsigned int idx = 0; idx < layout.blockSize; ++idx) {
			const unsigned int state = baseState | layout.blockOffsets[idx];
			stateIndices[idx] = state;
			localAmps[idx] = loadAmplitude(state);
			if(localAmps[idx].real != 0.0 || localAmps[idx].imag != 0.0) {
				hasNonZero = true;
			}
		}

		if(!hasNonZero) continue;

		for(unsigned int row = 0; row < layout.blockSize; ++row) {
			Amplitude newAmp{0.0, 0.0};
			const Amplitude *gRow = gateRows[row];
			for(unsigned int col = 0; col < layout.blockSize; ++col) {
				const Amplitude g = gRow[col];
				const Amplitude a = localAmps[col];
				newAmp.real += g.real * a.real - g.imag * a.imag;
				newAmp.imag += g.real * a.imag + g.imag * a.real;
			}
			storeAmplitude(stateIndices[row], newAmp);
		}
	}
}

} // namespace tmfqs

#endif // TMFQS_STORAGE_GATE_BLOCK_APPLY_H
