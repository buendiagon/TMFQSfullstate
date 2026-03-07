#ifndef STORAGE_GATE_BLOCK_APPLY_INCLUDE
#define STORAGE_GATE_BLOCK_APPLY_INCLUDE

#include <cstddef>
#include <vector>

#include "quantumGate.h"
#include "stateSpace.h"
#include "types.h"

struct GateBlockLayout {
	unsigned int numQubits = 0;
	unsigned int activeQubits = 0;
	unsigned int numBlocks = 0;
	unsigned int blockSize = 0;
	std::vector<unsigned int> targetPositions;
	std::vector<bool> isTargetBit;
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

inline GateBlockLayout makeGateBlockLayout(const QubitList &qubits, unsigned int numQubits) {
	GateBlockLayout layout;
	layout.numQubits = numQubits;
	layout.activeQubits = static_cast<unsigned int>(qubits.size());
	layout.numBlocks = checkedStateCount(numQubits - layout.activeQubits);
	layout.blockSize = checkedStateCount(layout.activeQubits);
	layout.targetPositions.resize(layout.activeQubits);
	layout.isTargetBit.assign(numQubits, false);

	for(unsigned int idx = 0; idx < layout.activeQubits; ++idx) {
		unsigned int targetBit = numQubits - qubits[idx] - 1u;
		layout.targetPositions[idx] = targetBit;
		layout.isTargetBit[targetBit] = true;
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
	const int k = static_cast<int>(layout.activeQubits);
	workspace.ensure(layout.blockSize);
	std::vector<Amplitude> &localAmps = workspace.localAmps;
	std::vector<StateIndex> &stateIndices = workspace.stateIndices;
	for(unsigned int row = 0; row < layout.blockSize; ++row) {
		workspace.gateRows[row] = gate[row];
	}
	const std::vector<const Amplitude *> &gateRows = workspace.gateRows;

	if(layout.activeQubits == 1u) {
		const unsigned int q0Mask = 1u << layout.targetPositions[0];
		const Amplitude *row0 = gateRows[0];
		const Amplitude *row1 = gateRows[1];
		for(unsigned int block = 0; block < layout.numBlocks; ++block) {
			unsigned int baseState = 0;
			unsigned int tempBlock = block;
			for(unsigned int bit = 0; bit < layout.numQubits; ++bit) {
				if(layout.isTargetBit[bit]) continue;
				baseState |= (tempBlock & 1u) << bit;
				tempBlock >>= 1u;
			}
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
		const unsigned int q0Mask = 1u << layout.targetPositions[0];
		const unsigned int q1Mask = 1u << layout.targetPositions[1];
		for(unsigned int block = 0; block < layout.numBlocks; ++block) {
			unsigned int baseState = 0;
			unsigned int tempBlock = block;
			for(unsigned int bit = 0; bit < layout.numQubits; ++bit) {
				if(layout.isTargetBit[bit]) continue;
				baseState |= (tempBlock & 1u) << bit;
				tempBlock >>= 1u;
			}

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
		unsigned int baseState = 0;
		unsigned int tempBlock = block;
		for(unsigned int bit = 0; bit < layout.numQubits; ++bit) {
			if(layout.isTargetBit[bit]) continue;
			baseState |= (tempBlock & 1u) << bit;
			tempBlock >>= 1u;
		}

		bool hasNonZero = false;
		for(unsigned int idx = 0; idx < layout.blockSize; ++idx) {
			unsigned int state = baseState;
			for(int i = 0; i < k; ++i) {
				unsigned int bitVal = (idx >> (k - 1 - i)) & 1u;
				state |= bitVal << layout.targetPositions[static_cast<size_t>(i)];
			}
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
				Amplitude g = gRow[col];
				Amplitude a = localAmps[col];
				newAmp.real += g.real * a.real - g.imag * a.imag;
				newAmp.imag += g.real * a.imag + g.imag * a.real;
			}
			storeAmplitude(stateIndices[row], newAmp);
		}
	}
}

#endif // STORAGE_GATE_BLOCK_APPLY_INCLUDE
