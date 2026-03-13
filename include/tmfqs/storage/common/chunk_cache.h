#ifndef TMFQS_STORAGE_COMMON_CHUNK_CACHE_H
#define TMFQS_STORAGE_COMMON_CHUNK_CACHE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace tmfqs {
namespace storage {

/** @brief LRU-style cache for decompressed chunk buffers used during gate updates. */
class ChunkCache {
	public:
		/** @brief Sentinel value for "no slot" / "no chunk". */
		static constexpr size_t kInvalidIndex = std::numeric_limits<size_t>::max();

		/** @brief Handle returned by `acquire` to expose slot metadata and writable data pointer. */
		struct SlotHandle {
			/** @brief Cache slot index. */
			size_t slotIndex = kInvalidIndex;
			/** @brief Chunk currently mapped to the slot. */
			size_t chunkIndex = kInvalidIndex;
			/** @brief Pointer to mutable decompressed chunk data. */
			double *data = nullptr;
			/** @brief Dirty marker that must be set when mutating `data`. */
			bool *dirty = nullptr;
			/** @brief Number of `double` elements in this chunk. */
			size_t elemCount = 0;

			/** @brief Returns whether the handle points to a valid cache slot. */
			bool valid() const noexcept {
				return slotIndex != kInvalidIndex && chunkIndex != kInvalidIndex && data != nullptr && dirty != nullptr;
			}
		};

		ChunkCache() = default;

		ChunkCache(size_t capacity, size_t chunkCount) {
			configure(capacity, chunkCount);
		}

		/** @brief Resets cache topology for a backend with `chunkCount` chunks. */
		void configure(size_t capacity, size_t chunkCount) {
			if(capacity == 0u) {
				throw std::invalid_argument("ChunkCache: capacity must be >= 1");
			}
			capacity_ = capacity;
			chunkToSlot_.assign(chunkCount, kInvalidIndex);
			slots_.clear();
			slots_.reserve(capacity_);
			touchCounter_ = 0;
		}

		/** @brief Clears all cached data and mappings. */
		void clear() {
			for(Slot &slot : slots_) {
				slot.chunkIndex = kInvalidIndex;
				slot.dirty = false;
				slot.touch = 0;
				slot.buffer.clear();
			}
			if(!chunkToSlot_.empty()) {
				std::fill(chunkToSlot_.begin(), chunkToSlot_.end(), kInvalidIndex);
			}
			slots_.clear();
			touchCounter_ = 0;
		}

		/** @brief Returns maximum number of simultaneously cached chunks. */
		size_t capacity() const noexcept {
			return capacity_;
		}

		template <typename LoadChunkFn, typename StoreChunkFn, typename ElemCountFn>
		/** @brief Returns a writable slot for `chunkIndex`, loading/evicting as needed. */
		SlotHandle acquire(
			size_t chunkIndex,
			LoadChunkFn &&loadChunkFn,
			StoreChunkFn &&storeChunkFn,
			ElemCountFn &&elemCountFn) {
			if(chunkIndex >= chunkToSlot_.size()) {
				throw std::out_of_range("ChunkCache: chunk index out of range");
			}

			const size_t mappedSlot = chunkToSlot_[chunkIndex];
			if(mappedSlot != kInvalidIndex) {
				Slot &slot = slots_[mappedSlot];
				slot.touch = ++touchCounter_;
				return makeSlotHandle(mappedSlot, chunkIndex);
			}

			size_t slotIndex = 0u;
			if(slots_.size() < capacity_) {
				slotIndex = slots_.size();
				slots_.push_back(Slot{});
			} else {
				slotIndex = selectEvictionSlot();
				evictSlot(slotIndex, storeChunkFn, elemCountFn);
			}

			Slot &slot = slots_[slotIndex];
			slot.chunkIndex = chunkIndex;
			slot.dirty = false;
			slot.touch = ++touchCounter_;
			const size_t elemCount = static_cast<size_t>(elemCountFn(chunkIndex));
			if(elemCount == 0u) {
				throw std::logic_error("ChunkCache: elemCount must be >= 1");
			}
			if(slot.buffer.size() != elemCount) {
				slot.buffer.assign(elemCount, 0.0);
			}
			loadChunkFn(chunkIndex, slot.buffer, elemCount);
			chunkToSlot_[chunkIndex] = slotIndex;
			return makeSlotHandle(slotIndex, chunkIndex);
		}

		/** @brief Returns cached chunk data if present, otherwise `nullptr`. */
		const std::vector<double> *findBuffer(size_t chunkIndex) const {
			if(chunkIndex >= chunkToSlot_.size()) return nullptr;
			const size_t slotIndex = chunkToSlot_[chunkIndex];
			if(slotIndex == kInvalidIndex) return nullptr;
			return &slots_[slotIndex].buffer;
		}

		template <typename StoreChunkFn, typename ElemCountFn>
		/** @brief Flushes all dirty slots using backend-provided write callback. */
		void flushAll(StoreChunkFn &&storeChunkFn, ElemCountFn &&elemCountFn) {
			for(size_t slotIndex = 0; slotIndex < slots_.size(); ++slotIndex) {
				flushSlot(slotIndex, storeChunkFn, elemCountFn);
			}
		}

	private:
		/** @brief One cached decompressed chunk and its bookkeeping metadata. */
		struct Slot {
			/** @brief Chunk currently mapped to this slot. */
			size_t chunkIndex = kInvalidIndex;
			/** @brief Decompressed chunk payload. */
			std::vector<double> buffer;
			/** @brief Dirty flag set by mutating operations. */
			bool dirty = false;
			/** @brief Monotonic touch counter used for LRU eviction. */
			uint64_t touch = 0;
		};

		/** @brief Maximum number of live slots. */
		size_t capacity_ = 0;
		/** @brief Slot storage. */
		std::vector<Slot> slots_;
		/** @brief Chunk-to-slot reverse lookup table. */
		std::vector<size_t> chunkToSlot_;
		/** @brief Global touch counter incremented on slot access. */
		uint64_t touchCounter_ = 0;

		/** @brief Builds a public handle for a slot. */
		SlotHandle makeSlotHandle(size_t slotIndex, size_t chunkIndex) {
			Slot &slot = slots_[slotIndex];
			return {slotIndex, chunkIndex, slot.buffer.data(), &slot.dirty, slot.buffer.size()};
		}

		/** @brief Chooses least-recently-used slot index. */
		size_t selectEvictionSlot() const {
			size_t candidate = 0u;
			uint64_t minTouch = slots_[0].touch;
			for(size_t idx = 1; idx < slots_.size(); ++idx) {
				if(slots_[idx].touch < minTouch) {
					minTouch = slots_[idx].touch;
					candidate = idx;
				}
			}
			return candidate;
		}

		template <typename StoreChunkFn, typename ElemCountFn>
		/** @brief Flushes and unmaps one slot before reuse. */
		void evictSlot(size_t slotIndex, StoreChunkFn &&storeChunkFn, ElemCountFn &&elemCountFn) {
			Slot &slot = slots_[slotIndex];
			if(slot.chunkIndex != kInvalidIndex) {
				flushSlot(slotIndex, storeChunkFn, elemCountFn);
				chunkToSlot_[slot.chunkIndex] = kInvalidIndex;
			}
			slot.chunkIndex = kInvalidIndex;
			slot.dirty = false;
			slot.touch = 0;
		}

		template <typename StoreChunkFn, typename ElemCountFn>
		/** @brief Writes one dirty slot back to backend storage. */
		void flushSlot(size_t slotIndex, StoreChunkFn &&storeChunkFn, ElemCountFn &&elemCountFn) {
			Slot &slot = slots_[slotIndex];
			if(!slot.dirty || slot.chunkIndex == kInvalidIndex) return;
			const size_t elemCount = static_cast<size_t>(elemCountFn(slot.chunkIndex));
			if(slot.buffer.size() != elemCount) {
				throw std::logic_error("ChunkCache: buffer/elemCount mismatch while flushing");
			}
			storeChunkFn(slot.chunkIndex, slot.buffer, elemCount);
			slot.dirty = false;
		}
};

} // namespace storage
} // namespace tmfqs

#endif // TMFQS_STORAGE_COMMON_CHUNK_CACHE_H
