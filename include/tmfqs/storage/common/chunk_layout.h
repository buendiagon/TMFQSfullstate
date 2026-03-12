#ifndef TMFQS_STORAGE_COMMON_CHUNK_LAYOUT_H
#define TMFQS_STORAGE_COMMON_CHUNK_LAYOUT_H

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include "tmfqs/core/types.h"

namespace tmfqs {
namespace storage {

struct ChunkRef {
	size_t chunkIndex = 0;
	size_t elemOffset = 0;
};

class ChunkLayout {
	public:
		ChunkLayout() = default;

		ChunkLayout(size_t totalStates, size_t statesPerChunk)
			: totalStates_(totalStates), statesPerChunk_(statesPerChunk) {
			if(statesPerChunk_ == 0u) {
				throw std::invalid_argument("ChunkLayout: statesPerChunk must be >= 1");
			}
			totalElems_ = totalStates_ * 2u;
			elemsPerChunk_ = statesPerChunk_ * 2u;
			chunkCount_ = totalElems_ == 0u ? 0u : (totalElems_ + elemsPerChunk_ - 1u) / elemsPerChunk_;
		}

		size_t totalStates() const noexcept { return totalStates_; }
		size_t totalElems() const noexcept { return totalElems_; }
		size_t statesPerChunk() const noexcept { return statesPerChunk_; }
		size_t elemsPerChunk() const noexcept { return elemsPerChunk_; }
		size_t chunkCount() const noexcept { return chunkCount_; }

		size_t chunkElemCount(size_t chunkIndex) const {
			if(chunkIndex >= chunkCount_) {
				throw std::out_of_range("ChunkLayout: chunk index out of range");
			}
			const size_t chunkBegin = chunkIndex * elemsPerChunk_;
			return std::min(elemsPerChunk_, totalElems_ - chunkBegin);
		}

		size_t chunkStateBegin(size_t chunkIndex) const {
			if(chunkIndex >= chunkCount_) {
				throw std::out_of_range("ChunkLayout: chunk index out of range");
			}
			return chunkIndex * statesPerChunk_;
		}

		ChunkRef stateRef(StateIndex state) const {
			const size_t stateSz = static_cast<size_t>(state);
			ChunkRef ref;
			ref.chunkIndex = stateSz / statesPerChunk_;
			ref.elemOffset = (stateSz % statesPerChunk_) * 2u;
			return ref;
		}

	private:
		size_t totalStates_ = 0;
		size_t totalElems_ = 0;
		size_t statesPerChunk_ = 0;
		size_t elemsPerChunk_ = 0;
		size_t chunkCount_ = 0;
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_COMMON_CHUNK_LAYOUT_H
