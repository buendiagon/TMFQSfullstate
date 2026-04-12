#ifndef TMFQS_CORE_TYPES_H
#define TMFQS_CORE_TYPES_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

namespace tmfqs {

/** @brief Index of a qubit in a register (0 is most-significant). */
using QubitIndex = unsigned int;
/** @brief Index of a basis state in the computational basis. */
using StateIndex = std::uint64_t;

/**
 * @brief Complex amplitude stored as separate real and imaginary parts.
 */
struct Amplitude {
	/** @brief Real component. */
	double real;
	/** @brief Imaginary component. */
	double imag;
};

/** @brief Interleaved amplitude buffer `[real0, imag0, real1, imag1, ...]`. */
using AmplitudesVector = std::vector<double>;

/**
 * @brief Lightweight wrapper for ordered qubit target lists.
 */
class QubitList {
	private:
		std::vector<QubitIndex> values_;

	public:
		QubitList() = default;
		/**
		 * @brief Constructs list from brace-initializer syntax.
		 * @param init Initial qubit indices.
		 */
		QubitList(std::initializer_list<QubitIndex> init) : values_(init) {}
		/**
		 * @brief Constructs list by moving from an existing vector.
		 * @param values Source vector.
		 */
		explicit QubitList(std::vector<QubitIndex> values) : values_(std::move(values)) {}

		/** @brief Returns number of qubits in the list. */
		size_t size() const noexcept { return values_.size(); }
		/** @brief Returns whether the list is empty. */
		bool empty() const noexcept { return values_.empty(); }
		/**
		 * @brief Reserves capacity.
		 * @param n Minimum capacity in elements.
		 */
		void reserve(size_t n) { values_.reserve(n); }
		/**
		 * @brief Appends one qubit index.
		 * @param value Qubit index to append.
		 */
		void push_back(QubitIndex value) { values_.push_back(value); }

		/**
		 * @brief Returns qubit index at one position.
		 * @param idx Position in the list.
		 * @return Qubit index at `idx`.
		 */
		QubitIndex operator[](size_t idx) const { return values_[idx]; }
		/**
		 * @brief Returns mutable qubit index reference at one position.
		 * @param idx Position in the list.
		 * @return Mutable reference at `idx`.
		 */
		QubitIndex &operator[](size_t idx) { return values_[idx]; }

		/** @brief Returns const iterator to list start. */
		std::vector<QubitIndex>::const_iterator begin() const noexcept { return values_.begin(); }
		/** @brief Returns const iterator to list end. */
		std::vector<QubitIndex>::const_iterator end() const noexcept { return values_.end(); }
		/** @brief Returns mutable iterator to list start. */
		std::vector<QubitIndex>::iterator begin() noexcept { return values_.begin(); }
		/** @brief Returns mutable iterator to list end. */
		std::vector<QubitIndex>::iterator end() noexcept { return values_.end(); }

		/**
		 * @brief Exposes the underlying vector as read-only.
		 * @return Reference to internal container.
		 */
		const std::vector<QubitIndex>& values() const noexcept { return values_; }
};

/**
 * @brief Lightweight wrapper for lists of basis-state indices.
 */
class BasisStateList {
	private:
		std::vector<StateIndex> values_;

	public:
		BasisStateList() = default;
		/**
		 * @brief Constructs list from brace-initializer syntax.
		 * @param init Initial basis-state indices.
		 */
		BasisStateList(std::initializer_list<StateIndex> init) : values_(init) {}
		/**
		 * @brief Constructs list by moving from an existing vector.
		 * @param values Source vector.
		 */
		explicit BasisStateList(std::vector<StateIndex> values) : values_(std::move(values)) {}

		/** @brief Returns number of basis states in the list. */
		size_t size() const noexcept { return values_.size(); }
		/** @brief Returns whether the list is empty. */
		bool empty() const noexcept { return values_.empty(); }
		/**
		 * @brief Reserves capacity.
		 * @param n Minimum capacity in elements.
		 */
		void reserve(size_t n) { values_.reserve(n); }
		/**
		 * @brief Appends one basis-state index.
		 * @param value Basis-state index to append.
		 */
		void push_back(StateIndex value) { values_.push_back(value); }

		/**
		 * @brief Returns basis-state index at one position.
		 * @param idx Position in the list.
		 * @return Basis-state index at `idx`.
		 */
		StateIndex operator[](size_t idx) const { return values_[idx]; }
		/**
		 * @brief Returns mutable basis-state index reference at one position.
		 * @param idx Position in the list.
		 * @return Mutable reference at `idx`.
		 */
		StateIndex &operator[](size_t idx) { return values_[idx]; }

		/** @brief Returns const iterator to list start. */
		std::vector<StateIndex>::const_iterator begin() const noexcept { return values_.begin(); }
		/** @brief Returns const iterator to list end. */
		std::vector<StateIndex>::const_iterator end() const noexcept { return values_.end(); }
		/** @brief Returns mutable iterator to list start. */
		std::vector<StateIndex>::iterator begin() noexcept { return values_.begin(); }
		/** @brief Returns mutable iterator to list end. */
		std::vector<StateIndex>::iterator end() noexcept { return values_.end(); }

		/**
		 * @brief Exposes the underlying vector as read-only.
		 * @return Reference to internal container.
		 */
		const std::vector<StateIndex>& values() const noexcept { return values_; }
};

} // namespace tmfqs

#endif // TMFQS_CORE_TYPES_H
