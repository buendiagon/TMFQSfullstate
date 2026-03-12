#ifndef TMFQS_STORAGE_COMMON_CHUNK_LAYOUT_H
#define TMFQS_STORAGE_COMMON_CHUNK_LAYOUT_H

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include "tmfqs/core/types.h"

namespace tmfqs {
namespace storage {

/** @brief Locates one basis-state amplitude pair inside chunked storage. */
struct ChunkRef {
	/** @brief Chunk containing the state. */
	size_t chunkIndex = 0;
	/** @brief Offset (in doubles) inside the chunk buffer for the state's real component. */
	size_t elemOffset = 0;
};

/** @brief Describes how a full state vector is partitioned into fixed-size chunks. */
class ChunkLayout {
	public:
		ChunkLayout() = default;

		/** @brief Builds a chunk layout for `totalStates` states and `statesPerChunk` chunk width. */
		ChunkLayout(size_t totalStates, size_t statesPerChunk)
			: totalStates_(totalStates), statesPerChunk_(statesPerChunk) {
			if(statesPerChunk_ == 0u) {
				throw std::invalid_argument("ChunkLayout: statesPerChunk must be >= 1");
			}
			totalElems_ = totalStates_ * 2u;
			elemsPerChunk_ = statesPerChunk_ * 2u;
			chunkCount_ = totalElems_ == 0u ? 0u : (totalElems_ + elemsPerChunk_ - 1u) / elemsPerChunk_;
		}

		/** @brief Returns total number of basis states. */
		size_t totalStates() const noexcept { return totalStates_; }
		/** @brief Returns total number of `double` elements (`2 * totalStates`). */
		size_t totalElems() const noexcept { return totalElems_; }
		/** @brief Returns number of basis states intended per chunk. */
		size_t statesPerChunk() const noexcept { return statesPerChunk_; }
		/** @brief Returns number of `double` elements intended per chunk. */
		size_t elemsPerChunk() const noexcept { return elemsPerChunk_; }
		/** @brief Returns number of chunks required for the current layout. */
		size_t chunkCount() const noexcept { return chunkCount_; }

		/** @brief Returns actual element count for one chunk (last chunk may be smaller). */
		size_t chunkElemCount(size_t chunkIndex) const {
			if(chunkIndex >= chunkCount_) {
				throw std::out_of_range("ChunkLayout: chunk index out of range");
			}
			const size_t chunkBegin = chunkIndex * elemsPerChunk_;
			return std::min(elemsPerChunk_, totalElems_ - chunkBegin);
		}

		/** @brief Returns first basis-state index stored in a chunk. */
		size_t chunkStateBegin(size_t chunkIndex) const {
			if(chunkIndex >= chunkCount_) {
				throw std::out_of_range("ChunkLayout: chunk index out of range");
			}
			return chunkIndex * statesPerChunk_;
		}

		/** @brief Maps a basis-state index to chunk index and local element offset. */
		ChunkRef stateRef(StateIndex state) const {
			const size_t stateSz = static_cast<size_t>(state);
			ChunkRef ref;
			ref.chunkIndex = stateSz / statesPerChunk_;
			ref.elemOffset = (stateSz % statesPerChunk_) * 2u;
			return ref;
		}

	private:
		/** @brief Total number of basis states represented. */
		size_t totalStates_ = 0;
		/** @brief Total number of interleaved amplitude elements. */
		size_t totalElems_ = 0;
		/** @brief Requested chunk width in basis states. */
		size_t statesPerChunk_ = 0;
		/** @brief Requested chunk width in amplitude elements. */
		size_t elemsPerChunk_ = 0;
		/** @brief Number of chunks needed for this layout. */
		size_t chunkCount_ = 0;
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_COMMON_CHUNK_LAYOUT_H
