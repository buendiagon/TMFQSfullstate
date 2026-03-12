#ifndef TMFQS_STORAGE_COMMON_PAIR_KERNEL_EXECUTOR_H
#define TMFQS_STORAGE_COMMON_PAIR_KERNEL_EXECUTOR_H

#include <algorithm>
#include <cstddef>

#include "tmfqs/storage/common/chunk_layout.h"

namespace tmfqs {
namespace storage {

enum class PairKernelMode {
	Fallback,
	IntraChunk,
	InterChunk
};

class PairKernelExecutor {
	public:
		static PairKernelMode selectMode(const ChunkLayout &layout, unsigned int targetMask) {
			const size_t statesPerChunk = layout.statesPerChunk();
			if(statesPerChunk == 0u || (statesPerChunk & (statesPerChunk - 1u)) != 0u) {
				return PairKernelMode::Fallback;
			}
			if(static_cast<size_t>(targetMask) < statesPerChunk) {
				return PairKernelMode::IntraChunk;
			}
			if((static_cast<size_t>(targetMask) % statesPerChunk) == 0u) {
				return PairKernelMode::InterChunk;
			}
			return PairKernelMode::Fallback;
		}

		template <typename PairFn>
		static void runFallback(unsigned int numStates, unsigned int targetMask, PairFn pairFn) {
			const unsigned int stride = targetMask << 1u;
			for(unsigned int base = 0; base < numStates; base += stride) {
				for(unsigned int offset = 0; offset < targetMask; ++offset) {
					const unsigned int state0 = base + offset;
					pairFn(state0, state0 + targetMask);
				}
			}
		}

		template <typename PairFn>
		static void runIntraChunk(const ChunkLayout &layout, unsigned int targetMask, PairFn pairFn) {
			const size_t localMask = static_cast<size_t>(targetMask);
			const size_t stride = localMask << 1u;
			for(size_t chunkIndex = 0; chunkIndex < layout.chunkCount(); ++chunkIndex) {
				const size_t statesInChunk = layout.chunkElemCount(chunkIndex) / 2u;
				const size_t chunkBaseState = layout.chunkStateBegin(chunkIndex);
				for(size_t base = 0; base < statesInChunk; base += stride) {
					for(size_t offset = 0; offset < localMask; ++offset) {
						const size_t local0 = base + offset;
						if(local0 + localMask >= statesInChunk) break;
						const size_t local1 = local0 + localMask;
						const StateIndex state0 = static_cast<StateIndex>(chunkBaseState + local0);
						const StateIndex state1 = static_cast<StateIndex>(chunkBaseState + local1);
						pairFn(chunkIndex, local0, local1, state0, state1);
					}
				}
			}
		}

		template <typename PairFn>
		static void runInterChunk(const ChunkLayout &layout, unsigned int targetMask, PairFn pairFn) {
			const size_t statesPerChunk = layout.statesPerChunk();
			const size_t chunkDelta = static_cast<size_t>(targetMask) / statesPerChunk;
			const size_t chunkStride = chunkDelta << 1u;
			const size_t chunkCount = layout.chunkCount();
			for(size_t groupBase = 0; groupBase < chunkCount; groupBase += chunkStride) {
				for(size_t groupOffset = 0; groupOffset < chunkDelta; ++groupOffset) {
					const size_t chunk0 = groupBase + groupOffset;
					const size_t chunk1 = chunk0 + chunkDelta;
					if(chunk1 >= chunkCount) break;
					const size_t statesInChunk0 = layout.chunkElemCount(chunk0) / 2u;
					const size_t statesInChunk1 = layout.chunkElemCount(chunk1) / 2u;
					const size_t statesInPair = std::min(statesInChunk0, statesInChunk1);
					const size_t chunk0BaseState = layout.chunkStateBegin(chunk0);
					const size_t chunk1BaseState = layout.chunkStateBegin(chunk1);
					for(size_t local = 0; local < statesInPair; ++local) {
						const StateIndex state0 = static_cast<StateIndex>(chunk0BaseState + local);
						const StateIndex state1 = static_cast<StateIndex>(chunk1BaseState + local);
						pairFn(chunk0, chunk1, local, state0, state1);
					}
				}
			}
		}
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_COMMON_PAIR_KERNEL_EXECUTOR_H
