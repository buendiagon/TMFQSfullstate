#ifndef TMFQS_STORAGE_COMMON_CHUNK_CACHE_H
#define TMFQS_STORAGE_COMMON_CHUNK_CACHE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace tmfqs {
namespace storage {

class ChunkCache {
	public:
		static constexpr size_t kInvalidIndex = std::numeric_limits<size_t>::max();

		struct SlotHandle {
			size_t slotIndex = kInvalidIndex;
			size_t chunkIndex = kInvalidIndex;
			double *data = nullptr;
			bool *dirty = nullptr;
			size_t elemCount = 0;

			bool valid() const noexcept {
				return slotIndex != kInvalidIndex && chunkIndex != kInvalidIndex && data != nullptr && dirty != nullptr;
			}
		};

		ChunkCache() = default;

		ChunkCache(size_t capacity, size_t chunkCount) {
			configure(capacity, chunkCount);
		}

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

		size_t capacity() const noexcept {
			return capacity_;
		}

		template <typename LoadChunkFn, typename StoreChunkFn, typename ElemCountFn>
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

		const std::vector<double> *findBuffer(size_t chunkIndex) const {
			if(chunkIndex >= chunkToSlot_.size()) return nullptr;
			const size_t slotIndex = chunkToSlot_[chunkIndex];
			if(slotIndex == kInvalidIndex) return nullptr;
			return &slots_[slotIndex].buffer;
		}

		template <typename StoreChunkFn, typename ElemCountFn>
		void flushAll(StoreChunkFn &&storeChunkFn, ElemCountFn &&elemCountFn) {
			for(size_t slotIndex = 0; slotIndex < slots_.size(); ++slotIndex) {
				flushSlot(slotIndex, storeChunkFn, elemCountFn);
			}
		}

	private:
		struct Slot {
			size_t chunkIndex = kInvalidIndex;
			std::vector<double> buffer;
			bool dirty = false;
			uint64_t touch = 0;
		};

		size_t capacity_ = 0;
		std::vector<Slot> slots_;
		std::vector<size_t> chunkToSlot_;
		uint64_t touchCounter_ = 0;

		SlotHandle makeSlotHandle(size_t slotIndex, size_t chunkIndex) {
			Slot &slot = slots_[slotIndex];
			return {slotIndex, chunkIndex, slot.buffer.data(), &slot.dirty, slot.buffer.size()};
		}

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
